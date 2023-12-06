#ifndef BARPH_IMPL_HEADER
#define BARPH_IMPL_HEADER

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifndef BARPH_REALLOC
#define BARPH_REALLOC realloc
#endif

#ifndef BARPH_MALLOC
#define BARPH_MALLOC malloc
#endif

#ifndef BARPH_FREE
#define BARPH_FREE free
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
    buf->data = (uint8_t *)BARPH_REALLOC(buf->data, buf->cap);
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
        buf->data = (uint8_t *)BARPH_REALLOC(buf->data, buf->cap);
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

const size_t min_lookback_length = 1;
// size of the hash function output in bits.
// must be at most 16. good values range from 16 to 12.
#define BARPH_HASH_SIZE 16
// log2 of the number of values per key (0 -> 1 value per key, 1 -> 2, 2 -> 4, 3 -> 8, 4 -> 16, etc)
// higher values are slower, but result in smaller files. good values range from 8 to 2 (8 at a hash size of 12, etc.)
#define BARPH_HASHTABLE_KEY_SHL 4
// heuristic/fudge
#define BARPH_LOOKBACK_BIAS -1

// hashtable itself, with multiple cells for each key based on the SHL define
uint64_t hashtable[(1<<BARPH_HASH_SIZE)<<BARPH_HASHTABLE_KEY_SHL];
// hashtable of current cursor for overwriting, from 0 to just under (1<<SHL)
uint8_t hashtable_i[1<<BARPH_HASH_SIZE];

// hashtable hashing constants
#define BRAPH_HASH_A_MULT 0x4C35
#define BRAPH_HASH_B_MASK 0x5813
#define BRAPH_HASH_B_MULT 0xACE1

// bytes must point to four characters
void hashmap_insert(const uint8_t * bytes, uint64_t value)
{
    // hashing function (can be anything; go ahead and optimize it as long as it doesn't result in tons of collisions)
    uint16_t a = (bytes[0] | (((uint16_t)bytes[1]) << 8)) * BRAPH_HASH_A_MULT;
    uint16_t b = ((bytes[2] | (((uint16_t)bytes[3]) << 8)) ^ BRAPH_HASH_B_MASK) * BRAPH_HASH_B_MULT;
    uint32_t key_i = ((uint32_t)(a ^ b));
    key_i &= (1 << BARPH_HASH_SIZE) - 1;
    uint32_t key = key_i << BARPH_HASHTABLE_KEY_SHL;
    
    // otherwise just overwrite the value after the newest value
    hashtable[key + hashtable_i[key_i]] = value;
    hashtable_i[key_i] = (hashtable_i[key_i] + 1) & ((1 << BARPH_HASHTABLE_KEY_SHL) - 1);
}
// bytes must point to four characters and be inside of buffer
uint64_t hashmap_get(const uint8_t * bytes, const uint8_t * buffer, const size_t buffer_len, uint64_t * min_len)
{
    // hashing function (can be anything; go ahead and optimize it as long as it doesn't result in tons of collisions)
    uint16_t a = (bytes[0] | (((uint16_t)bytes[1]) << 8)) * BRAPH_HASH_A_MULT;
    uint16_t b = ((bytes[2] | (((uint16_t)bytes[3]) << 8)) ^ BRAPH_HASH_B_MASK) * BRAPH_HASH_B_MULT;
    uint32_t key_i = ((uint32_t)(a ^ b));
    key_i &= (1 << BARPH_HASH_SIZE) - 1;
    uint32_t key = key_i << BARPH_HASHTABLE_KEY_SHL;
    
    // look for match within key
    uint64_t best = -1;
    uint64_t best_len = min_lookback_length - 1;
    for (int j = 0; j < (1 << BARPH_HASHTABLE_KEY_SHL); j++)
    {
        int n = (hashtable_i[key_i] + (1 << BARPH_HASHTABLE_KEY_SHL) - 1 - j) & ((1 << BARPH_HASHTABLE_KEY_SHL) - 1);
        uint64_t value = hashtable[key + n];
        // the above is technically correct for how the keys are rotated, BUT:
        // uint64_t value = hashtable[key + j]; (which is technically wrong) works ALMOST as well.
        
        // find longest match
        uint64_t len = 0;
        for (size_t i = 0; i < buffer_len - value; i++)
        {
            if (bytes[i] != buffer[value + i])
                break;
            len += 1;
        }
        if (len > best_len || (len == best_len && value > best))
        {
            best_len = len;
            best = value;
        }
        if (len >= 256) // good enough
            break;
    }
    
    *min_len = best_len;
    return best;
}

uint64_t hashmap_get_efficient(const uint64_t i, const uint8_t * input, const size_t input_len, uint64_t * in_size)
{
    // here we only return the hashmap hit if it would be efficient to code it
    
    if (i >= input_len)
        return -1;
    size_t remaining = input_len - i;
    if (remaining <= min_lookback_length)
        return -1;
    
    uint64_t min_len = 0;
    uint64_t found_loc = hashmap_get(&input[i], input, input_len, &min_len);
    size_t dist = i - found_loc;
    if (found_loc != (uint64_t)-1 && found_loc < i && dist <= 0x3FFFFFFFF)
    {
        size_t size;
        for (size = min_len; size < (1 << 15); size += 1)
        {
            if (size >= remaining)
                break;
            if (input[i + size] != input[found_loc + size])
                break;
        }
        if (size >= (1 << 15))
            size = 0x7FFF;
        
        size_t overhead =
            dist <= 0x3F ? 1 :
            (dist <= 0x1FFF ? 2 :
            (dist <= 0xFFFFF ? 3 :
            (dist <= 0x7FFFFF ? 4 : 5)));
        overhead += 1;
        
        if (overhead + BARPH_LOOKBACK_BIAS < size + 1)
        {
            *in_size = size;
            return found_loc;
        }
    }
    return -1;
}

static byte_buffer_t lookback_compress(const uint8_t * input, size_t input_len)
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
    
    size_t i = 0;
    size_t l = 0;
    while (i < input_len)
    {
        size_t remaining = input_len - i;
        
        // check for lookback hit
        if (remaining > min_lookback_length)
        {
            size_t size = 0;
            uint64_t found_loc = hashmap_get_efficient(i, input, input_len, &size);
            if (found_loc != (uint64_t)-1)
            {
                l += 1;
                
                size_t dist = i - found_loc;
                
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
                    fprintf(stderr, "internal error: bad lookback match (goes too far)");
                    exit(-1);
                }
                
                // push size
                byte_push(&ret, size << 1);
                if (size > 0x7F)
                    byte_push(&ret, size >> 7);
                
                // advance cursor and update hashmap
                for (size_t j = 0; j < size; ++j)
                {
                    if (i + 4 < input_len)
                        hashmap_insert(&input[i], i);
                    i += 1;
                }
                continue;
            }
        }
        
        // store a literal if we found no lookback
        uint16_t size = 1;
        while (size < (1 << 14) && i + size < input_len)
        {
            size_t _size_unused;
            uint64_t found_loc = hashmap_get_efficient(i + size, input, input_len, &_size_unused);
            if (found_loc != (uint64_t)-1)
                break;
            // need to update the hashmap mid-literal
            if (i + size + 4 < input_len)
                hashmap_insert(&input[i + size], i);
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
        
        for (size_t j = 0; j < size; ++j)
            byte_push(&ret, input[i++]);
    }
    printf("lookbacks found: %lld\n", l);
    
    return ret;
}

// On error, the output byte_buffer_t's cap will be 0 *AND* its len will be greater than 0.
// In such cases, the exact value of len indicates the particular error.
// 1: malformed lookback
// 2: malformed length
static byte_buffer_t lookback_decompress(const uint8_t * input, size_t input_len)
{
    size_t i = 0;
    
    uint64_t size = 0;
    size |= input[i++];
    size |= ((uint64_t)input[i++]) << 8;
    size |= ((uint64_t)input[i++]) << 16;
    size |= ((uint64_t)input[i++]) << 24;
    size |= ((uint64_t)input[i++]) << 32;
    size |= ((uint64_t)input[i++]) << 40;
    size |= ((uint64_t)input[i++]) << 48;
    size |= ((uint64_t)input[i++]) << 56;
    
    byte_buffer_t ret = {0, 0, 0};
    
    bytes_reserve(&ret, size);
    
    while (i < input_len)
    {
        uint8_t dat = input[i++];
        
        // lookback mode
        if (dat & 1)
        {
            size_t loc = 0;
            if (!(dat & 2))
                loc = dat >> 2;
            else if (!(dat & 4))
            {
                loc = dat >> 3;
                loc |= ((uint32_t)input[i++]) << 5;
            }
            else if (!(dat & 8))
            {
                loc = dat >> 4;
                loc |= ((uint32_t)input[i++]) << 4;
                loc |= ((uint32_t)input[i++]) << 12;
            }
            else if (!(dat & 0x10))
            {
                loc = dat >> 5;
                loc |= ((uint32_t)input[i++]) << 3;
                loc |= ((uint32_t)input[i++]) << 11;
                loc |= ((uint32_t)input[i++]) << 19;
            }
            else if (!(dat & 0x20))
            {
                loc = dat >> 6;
                loc |= ((uint32_t)input[i++]) << 2;
                loc |= ((uint32_t)input[i++]) << 10;
                loc |= ((uint32_t)input[i++]) << 18;
                loc |= ((uint32_t)input[i++]) << 26;
            }
            
            // bounds limit
            if (loc >= ret.len)
            {
                fprintf(stderr, "error: malformed compression data (lookback extends beyond start of file)\n");
                ret.len = 1;
                ret.cap = 0;
                return ret;
            }
            
            uint8_t size_dat = input[i++];
            uint16_t size = size_dat >> 1;
            if (size_dat & 1)
            {
                size |= ((uint16_t)input[i++]) << 7;
                if (i >= input_len)
                {
                    fprintf(stderr, "error: malformed compression data (tried to read past end of file)\n");
                    ret.len = 2;
                    ret.cap = 0;
                    return ret;
                }
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
                size |= ((uint16_t)input[i++]) << 6;
                if (i >= input_len)
                {
                    fprintf(stderr, "error: malformed compression data (tried to read past end of file)\n");
                    ret.len = 2;
                    ret.cap = 0;
                    return ret;
                }
            }
            
            bytes_push(&ret, &input[i], size);
            i += size;
        }
    }
    
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
    return (huff_node_t *)BARPH_MALLOC(sizeof(huff_node_t));
}

static void free_huff_nodes(huff_node_t * node)
{
    if (node->children[0])
        free_huff_nodes(node->children[0]);
    if (node->children[1])
        free_huff_nodes(node->children[1]);
    BARPH_FREE(node);
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

// passed-in data is modified, but not stored; it still belongs to the caller, and must be freed by the caller
// returned data must be freed by the caller; it was allocated with BARPH_MALLOC
static uint8_t * barph_compress(uint8_t * data, size_t len, uint8_t do_lookback, uint8_t do_huff, uint8_t do_diff, size_t * out_len)
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
            BARPH_FREE(buf.data);
        buf = new_buf;
    }
    if (do_huff)
    {
        byte_buffer_t new_buf = huff_pack(buf.data, buf.len).buffer;
        if (buf.data != data)
            BARPH_FREE(buf.data);
        buf = new_buf;
    }
    
    byte_buffer_t real_buf = {0, 0, 0};
    bytes_reserve(&real_buf, buf.len + 8 + 4);
    
    bytes_push(&real_buf, (const uint8_t *)"bRPH\2", 5);
    byte_push(&real_buf, do_diff);
    byte_push(&real_buf, do_lookback);
    byte_push(&real_buf, do_huff);
    bytes_push(&real_buf, (uint8_t *)&checksum, 4);
    bytes_push(&real_buf, buf.data, buf.len);
    
    if (buf.data != data)
        BARPH_FREE(buf.data);
    
    *out_len = real_buf.len;
    return real_buf.data;
}
// passed-in data is modified, but not stored; it still belongs to the caller, and must be freed by the caller
// returned data must be freed by the caller; it was allocated with BARPH_MALLOC
static uint8_t * barph_decompress(uint8_t * data, size_t len, size_t * out_len)
{
    if (!data || !out_len) return 0;
    
    byte_buffer_t buf = {data, len, len};
    
    if (buf.len < 8 || memcmp(buf.data, "bRPH\2", 5) != 0)
    {
        puts("invalid barph file");
        exit(0);
    }
    uint8_t do_diff = buf.data[5];
    uint8_t do_lookback = buf.data[6];
    uint8_t do_huff = buf.data[7];
    
    uint32_t stored_checksum = buf.data[8]
        | (((uint32_t)buf.data[9]) << 8)
        | (((uint32_t)buf.data[10]) << 16)
        | (((uint32_t)buf.data[11]) << 24);
    
    buf.data += 12;
    buf.len -= 12;
    
    if (do_huff)
    {
        bit_buffer_t compressed;
        memset(&compressed, 0, sizeof(bit_buffer_t));
        compressed.buffer = buf;
        byte_buffer_t new_buf = huff_unpack(&compressed);
        if (buf.data != data + 12)
            BARPH_FREE(buf.data);
        buf = new_buf;
    }
    if (do_lookback)
    {
        byte_buffer_t new_buf = lookback_decompress(buf.data, buf.len);
        if (buf.data != data + 12)
            BARPH_FREE(buf.data);
        buf = new_buf;
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
        if (buf.data != data)
            BARPH_FREE(buf.data);
        return 0;
    }
}

#endif // BARPH_IMPL_HEADER
