#ifndef LOH_IMPL_HEADER
#define LOH_IMPL_HEADER

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifndef LOH_REALLOC
#define LOH_REALLOC realloc
#endif

#ifndef LOH_MALLOC
#define LOH_MALLOC malloc
#endif

#ifndef LOH_FREE
#define LOH_FREE free
#endif

typedef struct {
    uint8_t * data;
    size_t len;
    size_t cap;
} byte_buffer_t;

static void bytes_reserve(byte_buffer_t * buf, size_t extra)
{
    if (buf->cap < 8)
        buf->cap = 8;
    while (buf->len + extra > buf->cap)
        buf->cap <<= 1;
    buf->data = (uint8_t *)LOH_REALLOC(buf->data, buf->cap);
}
static void bytes_push(byte_buffer_t * buf, const uint8_t * bytes, size_t count)
{
    bytes_reserve(buf, count);
    memcpy(&buf->data[buf->len], bytes, count);
    buf->len += count;
}
static void byte_push(byte_buffer_t * buf, uint8_t byte)
{
    if (buf->len == buf->cap)
    {
        buf->cap = buf->cap << 1;
        if (buf->cap < 8)
            buf->cap = 8;
        buf->data = (uint8_t *)LOH_REALLOC(buf->data, buf->cap);
    }
    buf->data[buf->len] = byte;
    buf->len += 1;
}

typedef struct {
    byte_buffer_t buffer;
    size_t byte_index;
    size_t bit_count;
    uint8_t bit_index;
} bit_buffer_t;

static void bits_push(bit_buffer_t * buf, uint64_t data, uint8_t bits)
{
    if (bits == 0)
        return;
    for (uint8_t n = 0; n < bits; n += 1)
    {
        if (buf->bit_index >= 8 || buf->buffer.len == 0)
        {
            byte_push(&buf->buffer, 0);
            if (buf->bit_index >= 8)
            {
                buf->bit_index -= 8;
                buf->byte_index += 1;
            }
        }
        buf->buffer.data[buf->buffer.len - 1] |= (!!(data & ((uint64_t)1 << n))) << buf->bit_index;
        buf->bit_index += 1;
        buf->bit_count += 1;
    }
}
static void bit_push(bit_buffer_t * buf, uint8_t data)
{
    if (buf->bit_index >= 8 || buf->buffer.len == 0)
    {
        byte_push(&buf->buffer, 0);
        if (buf->bit_index >= 8)
        {
            buf->bit_index -= 8;
            buf->byte_index += 1;
        }
    }
    buf->buffer.data[buf->buffer.len - 1] |= data << buf->bit_index;
    buf->bit_index += 1;
    buf->bit_count += 1;
}
static uint64_t bits_pop(bit_buffer_t * buf, uint8_t bits)
{
    if (bits == 0)
        return 0;
    uint64_t ret = 0;
    for (uint8_t n = 0; n < bits; n += 1)
    {
        if (buf->bit_index >= 8)
        {
            buf->bit_index -= 8;
            buf->byte_index += 1;
        }
        ret |= (uint64_t)((buf->buffer.data[buf->byte_index] >> buf->bit_index) & 1) << n;
        buf->bit_index += 1;
    }
    return ret;
}
static uint8_t bit_pop(bit_buffer_t * buf)
{
    if (buf->bit_index >= 8)
    {
        buf->bit_index -= 8;
        buf->byte_index += 1;
    }
    uint8_t ret = (buf->buffer.data[buf->byte_index] >> buf->bit_index) & 1;
    buf->bit_index += 1;
    
    return ret;
}

static const size_t min_lookback_length = 1;

#define LOH_HASH_LENGTH 4
// size of the hash function output in bits.
// must be at most 32, but values significantly above 16 are a Bad Idea.
// higher values take up exponentially more memory.
#define LOH_HASH_SIZE 16
// log2 of the number of values per key (0 -> 1 value per key, 1 -> 2, 2 -> 4, 3 -> 8, 4 -> 16, etc)
// higher values are slower, but result in smaller files. 8 is the max.
// higher values take up exponentially more memory and are exponentially slower.
#define LOH_HASHTABLE_KEY_SHL 2

static const uint64_t hash_mask = (1 << LOH_HASH_SIZE) - 1;
static const uint64_t hash_shl_max = 1 << LOH_HASHTABLE_KEY_SHL;
static const uint64_t hash_shl_mask = hash_shl_max - 1;

// hashtable itself, with multiple cells for each key based on the SHL define
static uint64_t hashtable[(1<<LOH_HASH_SIZE)<<LOH_HASHTABLE_KEY_SHL];
// hashtable of current cursor for overwriting, from 0 to just under (1<<SHL)
static uint8_t hashtable_i[1<<LOH_HASH_SIZE];

// hashtable hashing constants
#define BRAPH_HASH_A_MULT 0x4C35
#define BRAPH_HASH_B_MASK 0x5813
#define BRAPH_HASH_B_MULT 0xACE1

static inline uint32_t hashmap_hash(const uint8_t * bytes)
{
    // hashing function (can be anything; go ahead and optimize it as long as it doesn't result in tons of collisions)
    uint32_t temp = 0xA68BF1D7;
    for (size_t j = 0; j < LOH_HASH_LENGTH; j += 1)
    {
        temp ^= bytes[j];
        temp *= 0x1011B0D5;
    }
    return temp & hash_mask;
}

// bytes must point to four characters
static inline void hashmap_insert(const uint8_t * bytes, uint64_t value)
{
    const uint32_t key_i = hashmap_hash(bytes);
    const uint32_t key = key_i << LOH_HASHTABLE_KEY_SHL;
    
    // otherwise just overwrite the value after the newest value
    hashtable[key + hashtable_i[key_i]] = value;
    hashtable_i[key_i] = (hashtable_i[key_i] + 1) & hash_shl_mask;
}
// bytes must point to four characters and be inside of buffer
static inline uint64_t hashmap_get(const uint8_t * bytes, const uint8_t * buffer, const size_t buffer_len, uint64_t * min_len, const uint8_t final)
{
    const uint32_t key_i = hashmap_hash(bytes);
    const uint32_t key = key_i << LOH_HASHTABLE_KEY_SHL;
    
    // look for match within key
    size_t i = (size_t)(bytes - buffer);
    uint64_t best = -1;
    uint64_t best_size = min_lookback_length - 1;
    uint32_t best_j = 0;
    for (uint32_t j = 0; j < hash_shl_max; j++)
    {
        const int n = (hashtable_i[key_i] + hash_shl_max - 1 - j) & hash_shl_mask;
        const uint64_t value = hashtable[key + n];
        //const uint64_t value = hashtable[key + j];
        
        if (value >= i)
            break;
        
        // find longest match
        const uint64_t chunk_size = 16;
        const uint64_t remaining = buffer_len - i;
        uint64_t size = 0;
        while (size + chunk_size < remaining)
        {
            if (memcmp(&bytes[size], &buffer[value + size], chunk_size) == 0)
                size += chunk_size;
            else
                break;
        }
        while (size < remaining && bytes[size] == buffer[value + size])
            size += 1;
        
        if (size > best_size || (size == best_size && value > best))
        {
            // other entry hit was expensive to test; axe it
            if (best_size > 64)
                hashtable[key + best_j] = 0;
            
            best_size = size;
            best = value;
            best_j = j;
            
            if (best_size >= 256) // good enough
                break;
            if (!final && best_size >= 16)
                break;
        }
    }
    
    *min_len = best_size;
    return best;
}

static inline uint64_t hashmap_get_if_efficient(const uint64_t i, const uint8_t * input, const uint64_t input_len, uint64_t * out_size, const uint8_t final)
{
    // here we only return the hashmap hit if it would be efficient to code it
    
    uint64_t remaining = input_len - i;
    if (i >= input_len || remaining <= min_lookback_length)
        return -1;
    
    if (remaining > 0x8000)
        remaining = 0x8000;
    
    uint64_t size = 0;
    const uint64_t found_loc = hashmap_get(&input[i], input, input_len, &size, final);
    const uint64_t dist = i - found_loc;
    if (found_loc != (uint64_t)-1 && found_loc < i && dist <= 0x3FFFFFFFF)
    {
        if (final)
        {
            const uint64_t chunk_size = 1;
            while (size + chunk_size < remaining)
            {
                if (memcmp(&input[i + size], &input[found_loc + size], chunk_size) == 0)
                    size += chunk_size;
                else
                    break;
            }
            while (size < remaining && input[i + size] == input[found_loc + size])
                size += 1;
        }
        
        if (size > 0x7FFF)
            size = 0x7FFF;
        
        uint64_t overhead =
            dist <= 0x3F ? 1 :
            (dist <= 0x1FFF ? 2 :
            (dist <= 0xFFFFF ? 3 :
            (dist <= 0x7FFFFF ? 4 : 5)));
        overhead += 1;
        
        if (overhead < size)
        {
            *out_size = size;
            return found_loc;
        }
    }
    return -1;
}

static byte_buffer_t lookback_compress(const uint8_t * input, uint64_t input_len)
{
    memset(&hashtable, 0, sizeof(hashtable));
    memset(&hashtable_i, 0, sizeof(hashtable_i));
    
    byte_buffer_t ret = {0, 0, 0};
    
    byte_push(&ret, input_len & 0xFF);
    byte_push(&ret, (input_len >> 8) & 0xFF);
    byte_push(&ret, (input_len >> 16) & 0xFF);
    byte_push(&ret, (input_len >> 24) & 0xFF);
    byte_push(&ret, (input_len >> 32) & 0xFF);
    byte_push(&ret, (input_len >> 40) & 0xFF);
    byte_push(&ret, (input_len >> 48) & 0xFF);
    byte_push(&ret, (input_len >> 56) & 0xFF);
    
    uint64_t i = 0;
    uint64_t l = 0;
    while (i < input_len)
    {
        uint64_t remaining = input_len - i;
        
        // check for lookback hit
        if (remaining > min_lookback_length)
        {
            uint64_t size = 0;
            uint64_t found_loc = hashmap_get_if_efficient(i, input, input_len, &size, 1);
            if (found_loc != (uint64_t)-1)
            {
                l += 1;
                
                uint64_t dist = i - found_loc;
                
                if (dist > i)
                {
                    fprintf(stderr, "LOH internal error: broken lookback distance calculation\n");
                    exit(-1);
                }
                
                // push lookback distance
                if (dist <= 0x3F)
                    byte_push(&ret, 1 | (dist << 2));
                else if (dist <= 0x1FFF)
                {
                    byte_push(&ret, 3 | (dist << 3));
                    byte_push(&ret, dist >> 5);
                }
                else if (dist <= 0xFFFFF)
                {
                    byte_push(&ret, 7 | (dist << 4));
                    byte_push(&ret, dist >> 4);
                    byte_push(&ret, dist >> 12);
                }
                else if (dist <= 0x7FFFFF)
                {
                    byte_push(&ret, 0xF | (dist << 5));
                    byte_push(&ret, dist >> 3);
                    byte_push(&ret, dist >> 11);
                    byte_push(&ret, dist >> 19);
                }
                else if (dist <= 0x3FFFFFFFF)
                {
                    byte_push(&ret, 0x1F | (dist << 6));
                    byte_push(&ret, dist >> 2);
                    byte_push(&ret, dist >> 10);
                    byte_push(&ret, dist >> 18);
                    byte_push(&ret, dist >> 26);
                }
                else
                {
                    fprintf(stderr, "LOH internal error: bad lookback match (goes too far)");
                    exit(-1);
                }
                
                //printf("encoding lookback with size %08llX and distance %08llX starting at %08llX\n", size, dist, i);
                
                // push size
                if (size > 0x7F)
                {
                    byte_push(&ret, 1 | (size << 1));
                    byte_push(&ret, size >> 7);
                }
                else
                    byte_push(&ret, size << 1);
                
                // advance cursor and update hashmap
                for (size_t j = 0; j < size; ++j)
                {
                    if (i + LOH_HASH_LENGTH < input_len)
                        hashmap_insert(&input[i], i);
                    i += 1;
                }
                continue;
            }
        }
        
        // store a literal if we found no lookback
        // FIXME: maybe test in chunks with memcpy...?
        uint16_t size = 1;
        while (size < (1 << 14) && i + size < input_len)
        {
            uint64_t _size_unused;
            uint64_t found_loc = hashmap_get_if_efficient(i + size, input, input_len, &_size_unused, 0);
            if (found_loc != (uint64_t)-1)
                break;
            // need to update the hashmap mid-literal
            if (i + size + LOH_HASH_LENGTH < input_len && size + 1 < (1 << 14))
                hashmap_insert(&input[i + size], i + size);
            size += 1;
        }
        if (size == (1 << 14))
            size -= 1;
        
        if (size > input_len - i)
            size = input_len - i;
        
        // we store short and long literals slightly differently, with a bit flag to differentiate
        if (size > 0x3F)
        {
            byte_push(&ret, (size << 2) | 2);
            byte_push(&ret, size >> 6);
        }
        else
            byte_push(&ret, (size << 2));
        
        //printf("encoding literal with size %08X starting at %08llX\n", size, i);
        
        for (size_t j = 0; j < size; ++j)
            byte_push(&ret, input[i++]);
    }
    
    return ret;
}

// On error, the output byte_buffer_t's cap will be 0 *AND* its len will be greater than 0.
// Any partially-decompressed data is returned rather than being freed and nulled.
static byte_buffer_t lookback_decompress(const uint8_t * input, size_t input_len)
{
    size_t i = 0;
    
    byte_buffer_t ret = {0, 0, 0};
    
    if (input_len < 8)
    {
        ret.len = 1;
        ret.cap = 0;
        return ret;
    }
    
    uint64_t size = 0;
    size |= input[i++];
    size |= ((uint64_t)input[i++]) << 8;
    size |= ((uint64_t)input[i++]) << 16;
    size |= ((uint64_t)input[i++]) << 24;
    size |= ((uint64_t)input[i++]) << 32;
    size |= ((uint64_t)input[i++]) << 40;
    size |= ((uint64_t)input[i++]) << 48;
    size |= ((uint64_t)input[i++]) << 56;
    
    bytes_reserve(&ret, size);
    
#define _LOH_CHECK_I_VS_LEN_OR_RETURN \
    if (i >= input_len)\
    {\
        ret.len = 1;\
        ret.cap = 0;\
        return ret;\
    }

    while (i < input_len)
    {
        uint8_t dat = input[i++];
        
        // lookback mode
        if (dat & 1)
        {
            uint64_t loc = 0;
            if (!(dat & 2))
                loc = dat >> 2;
            else if (!(dat & 4))
            {
                loc = dat >> 3;
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                loc |= ((uint64_t)input[i++]) << 5;
            }
            else if (!(dat & 8))
            {
                loc = dat >> 4;
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                loc |= ((uint64_t)input[i++]) << 4;
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                loc |= ((uint64_t)input[i++]) << 12;
            }
            else if (!(dat & 0x10))
            {
                loc = dat >> 5;
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                loc |= ((uint64_t)input[i++]) << 3;
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                loc |= ((uint64_t)input[i++]) << 11;
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                loc |= ((uint64_t)input[i++]) << 19;
            }
            else if (!(dat & 0x20))
            {
                loc = dat >> 6;
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                loc |= ((uint64_t)input[i++]) << 2;
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                loc |= ((uint64_t)input[i++]) << 10;
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                loc |= ((uint64_t)input[i++]) << 18;
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                loc |= ((uint64_t)input[i++]) << 26;
            }
            
            // bounds limit
            if (loc > ret.len)
            {
                ret.len = 2;
                ret.cap = 0;
                return ret;
            }
            
            _LOH_CHECK_I_VS_LEN_OR_RETURN
            uint8_t size_dat = input[i++];
            
            uint16_t size = size_dat >> 1;
            if (size_dat & 1)
            {
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                size |= ((uint16_t)input[i++]) << 7;
            }
            
            // bytes_push uses memcpy, which doesn't work if there's any overlap.
            // overlap is allowed here, so we have to copy bytes manually.
            for(size_t j = 0; j < size; j++)
                byte_push(&ret, ret.data[ret.len - loc]);
        }
        // literal mode
        else
        {
            uint16_t size = dat >> 2;
            // this bit is true if the literal is long and has an extra length byte
            if (dat & 2)
            {
                _LOH_CHECK_I_VS_LEN_OR_RETURN
                size |= ((uint16_t)input[i++]) << 6;
            }
            
            bytes_push(&ret, &input[i], size);
            i += size;
        }
    }
    
#undef _LOH_CHECK_I_VS_LEN_OR_RETURN
    
    return ret;
}

typedef struct _huff_node {
    struct _huff_node * children[2];
    int64_t freq;
    bit_buffer_t code;
    uint8_t symbol;
} huff_node_t;

static huff_node_t * alloc_huff_node()
{
    return (huff_node_t *)LOH_MALLOC(sizeof(huff_node_t));
}

static void free_huff_nodes(huff_node_t * node)
{
    if (node->children[0])
        free_huff_nodes(node->children[0]);
    if (node->children[1])
        free_huff_nodes(node->children[1]);
    LOH_FREE(node);
}

static void push_code(huff_node_t * node, uint8_t bit)
{
    bit_push(&node->code, bit);
    if (node->children[0])
        push_code(node->children[0], bit);
    if (node->children[1])
        push_code(node->children[1], bit);
}

static int count_compare(const void * a, const void * b)
{
    int64_t n = *((int64_t*)b) - *((int64_t*)a);
    return n > 0 ? 1 : n < 0 ? -1 : 0;
}

static void push_huff_node(bit_buffer_t * buf, huff_node_t * node)
{
    if (node->children[0] && node->children[1])
    {
        bit_push(buf, 1);
        push_huff_node(buf, node->children[0]);
        push_huff_node(buf, node->children[1]);
    }
    else
    {
        bit_push(buf, 0);
        bits_push(buf, node->symbol, 8);
    }
}

static huff_node_t * pop_huff_node(bit_buffer_t * buf)
{
    huff_node_t * ret;
    ret = alloc_huff_node();
    ret->symbol = 0;
    memset(&ret->code, 0, sizeof(bit_buffer_t));
    ret->freq = 0;
    ret->children[0] = 0;
    ret->children[1] = 0;
    
    int children = bit_pop(buf);
    if (children)
    {
        ret->children[0] = pop_huff_node(buf);
        ret->children[1] = pop_huff_node(buf);
        push_code(ret->children[0], 0);
        push_code(ret->children[1], 1);
    }
    else
    {
        ret->symbol = bits_pop(buf, 8);
    }
    
    return ret;
}

static bit_buffer_t huff_pack(uint8_t * data, size_t len)
{
    // build huff dictionary
    
    // count bytes, then sort them
    // we stuff the byte identity into the bottom 8 bits
    uint64_t counts[256] = {0};
    for (size_t i = 0; i < len; i += 1)
        counts[data[i]] += 256;
    for (size_t b = 0; b < 256; b++)
        counts[b] |= b;
    qsort(&counts, 256, sizeof(uint64_t), count_compare);
    
    // set up raw huff nodes
    huff_node_t * unordered_dict[256];
    for (size_t i = 0; i < 256; i += 1)
    {
        unordered_dict[i] = alloc_huff_node();
        unordered_dict[i]->symbol = counts[i] & 0xFF;
        memset(&unordered_dict[i]->code, 0, sizeof(bit_buffer_t));
        unordered_dict[i]->freq = counts[i] >> 8;
        unordered_dict[i]->children[0] = 0;
        unordered_dict[i]->children[1] = 0;
    }
    
    // set up byte name -> huff node dict
    huff_node_t * dict[256];
    for (size_t i = 0; i < 256; i += 1)
        dict[unordered_dict[i]->symbol] = unordered_dict[i];
    
    // set up tree generation queues
    huff_node_t * queue_in[256];
    size_t queue_in_count = 256;
    
    huff_node_t * queue_out[256];
    size_t queue_out_count = 0;
    
    for (size_t i = 0; i < 256; i += 1)
        queue_in[i] = unordered_dict[i];
    for (size_t i = 0; i < 256; i += 1)
        queue_out[i] = 0;
    
    // remove zero-frequency items from the input queue
    while (queue_in[queue_in_count - 1]->freq == 0 && queue_in_count > 0)
    {
        free_huff_nodes(queue_in[queue_in_count - 1]);
        queue_in_count -= 1;
    }
    
    // start pumping through the queues
    while (queue_in_count + queue_out_count > 1)
    {
        if (queue_out_count > 250)
        {
            fprintf(stderr, "Huffman tree generation failed; vastly exceeded maximum tree depth.");
            exit(-1);
        }
        
        // find lowest-frequency items
        huff_node_t * nodes[4] = {0, 0, 0, 0};
        nodes[0] = (queue_in_count  >= 1) ? (queue_in [queue_in_count  - 1]) : 0;
        nodes[1] = (queue_in_count  >= 2) ? (queue_in [queue_in_count  - 2]) : 0;
        nodes[2] = (queue_out_count >= 1) ? (queue_out[queue_out_count - 1]) : 0;
        nodes[3] = (queue_out_count >= 2) ? (queue_out[queue_out_count - 2]) : 0;
        
        huff_node_t * lowest = 0;
        for (size_t i = 0; i < 4; i++)
            if (!lowest || (nodes[i] && nodes[i]->freq < lowest->freq))
                lowest = nodes[i];
        
        huff_node_t * next_lowest = 0;
        for (size_t i = 0; i < 4; i++)
            if (nodes[i] != lowest && (!next_lowest || (nodes[i] && nodes[i]->freq < next_lowest->freq)))
                next_lowest = nodes[i];
        
        // shrink queues depending on which nodes were gotten from which queue
        queue_in_count  -=  (lowest == nodes[0] || lowest == nodes[1]) +  (next_lowest == nodes[0] || next_lowest == nodes[1]);
        queue_out_count -= !(lowest == nodes[0] || lowest == nodes[1]) + !(next_lowest == nodes[0] || next_lowest == nodes[1]);
        
        // make new node
        huff_node_t * new_node = alloc_huff_node();
        new_node->symbol = 0;
        memset(&new_node->code, 0, sizeof(bit_buffer_t));
        new_node->freq = lowest->freq + next_lowest->freq;
        new_node->children[0] = lowest;
        new_node->children[1] = next_lowest;
        
        push_code(new_node->children[0], 0);
        push_code(new_node->children[1], 1);
        
        // insert new out-queue element at bottom
        queue_out_count += 1;
        for (size_t i = queue_out_count; i > 0; i -= 1)
            queue_out[i] = queue_out[i-1];
        queue_out[0] = new_node;
    }
    
    // set up buffers and start pushing data to them
    
    bit_buffer_t ret;
    memset(&ret, 0, sizeof(bit_buffer_t));
    
    bits_push(&ret, len, 8*8);
    
    push_huff_node(&ret, queue_out[0]);
    
    for(size_t i = 0; i < len; i++)
    {
        uint8_t c = data[i];
        for (size_t n = 0; n < dict[c]->code.bit_count; n++)
        {
            size_t m = dict[c]->code.bit_count - n - 1;
            int bit = (dict[c]->code.buffer.data[m / 8] >> ((m % 8))) & 1;
            bit_push(&ret, bit);
        }
    }
    
    free_huff_nodes(queue_out[0]);
    
    return ret;
}

static byte_buffer_t huff_unpack(bit_buffer_t * buf)
{
    buf->bit_index = 0;
    buf->byte_index = 0;
    size_t len = bits_pop(buf, 8*8);
    
    byte_buffer_t ret = {0, 0, 0};
    bytes_reserve(&ret, len);
    
    huff_node_t * root = pop_huff_node(buf);
    
    for(size_t i = 0; i < len; i++)
    {
        huff_node_t * node = root;
        node = node->children[bit_pop(buf)];
        while (node->children[0])
            node = node->children[bit_pop(buf)];
        byte_push(&ret, node->symbol);
    }
    
    free_huff_nodes(root);
    
    return ret;
}

// LLH

// passed-in data is modified, but not stored; it still belongs to the caller, and must be freed by the caller
// returned data must be freed by the caller; it was allocated with LOH_MALLOC
static uint8_t * loh_compress(uint8_t * data, size_t len, uint8_t do_lookback, uint8_t do_huff, uint8_t do_diff, size_t * out_len)
{
    if (!data || !out_len) return 0;
    
    byte_buffer_t buf = {data, len, len};
    
    const uint32_t big_prime = 0x1011B0D5;
    uint32_t checksum = 0x87654321;
    
    for (size_t i = 0; i < len; i += 1)
        checksum = (checksum ^ buf.data[i] ^ i) * big_prime;
    
    if (do_diff)
    {
        for (size_t i = buf.len - 1; i >= do_diff; i -= 1)
            buf.data[i] -= buf.data[i - do_diff];
    }
    if (do_lookback)
    {
        byte_buffer_t new_buf = lookback_compress(buf.data, buf.len);
        if (buf.data != data)
            LOH_FREE(buf.data);
        buf = new_buf;
    }
    if (do_huff)
    {
        byte_buffer_t new_buf = huff_pack(buf.data, buf.len).buffer;
        if (buf.data != data)
            LOH_FREE(buf.data);
        buf = new_buf;
    }
    
    byte_buffer_t real_buf = {0, 0, 0};
    bytes_reserve(&real_buf, buf.len + 8 + 4);
    
    bytes_push(&real_buf, (const uint8_t *)"LOHz", 5);
    byte_push(&real_buf, do_diff);
    byte_push(&real_buf, do_lookback);
    byte_push(&real_buf, do_huff);
    bytes_push(&real_buf, (uint8_t *)&checksum, 4);
    bytes_push(&real_buf, buf.data, buf.len);
    
    if (buf.data != data)
        LOH_FREE(buf.data);
    
    *out_len = real_buf.len;
    return real_buf.data;
}
// passed-in data is modified, but not stored; it still belongs to the caller, and must be freed by the caller
// returned data must be freed by the caller; it was allocated with LOH_MALLOC
static uint8_t * loh_decompress(uint8_t * data, size_t len, size_t * out_len)
{
    if (!data || !out_len) return 0;
    
    byte_buffer_t buf = {data, len, len};
    
    if (buf.len < 8 || memcmp(buf.data, "LOHz", 5) != 0)
        return 0;
    
    uint8_t do_diff = buf.data[5];
    uint8_t do_lookback = buf.data[6];
    uint8_t do_huff = buf.data[7];
    
    uint32_t stored_checksum = buf.data[8]
        | (((uint32_t)buf.data[9]) << 8)
        | (((uint32_t)buf.data[10]) << 16)
        | (((uint32_t)buf.data[11]) << 24);
    
    const uint16_t header_size = 12;
    
    buf.data += header_size;
    buf.len -= header_size;
    
    if (do_huff)
    {
        bit_buffer_t compressed;
        memset(&compressed, 0, sizeof(bit_buffer_t));
        compressed.buffer = buf;
        byte_buffer_t new_buf = huff_unpack(&compressed);
        if (buf.data != data + header_size)
            LOH_FREE(buf.data);
        buf = new_buf;
    }
    if (do_lookback)
    {
        byte_buffer_t new_buf = lookback_decompress(buf.data, buf.len);
        if (buf.data != data + header_size)
            LOH_FREE(buf.data);
        buf = new_buf;
        if (buf.len > buf.cap)
        {
            if (buf.data && buf.data != data + header_size)
                LOH_FREE(buf.data);
            return 0;
        }
    }
    if (do_diff)
    {
        for (size_t i = do_diff; i < buf.len; i += 1)
            buf.data[i] += buf.data[i - do_diff];
    }
    
    const uint32_t big_prime = 0x1011B0D5;
    uint32_t checksum = 0x87654321;
    
    if (stored_checksum != 0)
    {
        for (size_t i = 0; i < buf.len; i += 1)
            checksum = (checksum ^ buf.data[i] ^ i) * big_prime;
    }
    else
        checksum = stored_checksum;
    
    if (checksum == stored_checksum)
    {
        *out_len = buf.len;
        return buf.data;
    }
    else
    {
        if (buf.data && buf.data != data + header_size)
            LOH_FREE(buf.data);
        return 0;
    }
}

#endif // LOH_IMPL_HEADER
