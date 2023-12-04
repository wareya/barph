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
static void byte_push_many(byte_buffer_t * buf, uint8_t byte, size_t count)
{
    bytes_reserve(buf, count);
    memset(&buf->data[buf->len], byte, count);
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

static int has_efficient_rle(const uint8_t * input, size_t input_len)
{
    if (input_len >= 3)
    {
        if (input[0] == input[1] && input[1] == input[2])
            return 1;
    }
    for (size_t size = 2; size <= 4; size += 1)
    {
        if (size * 2 > input_len)
            break;
        
        int failed_match = 0;
        for (size_t i = 0; i < size; i += 1)
        {
            if (input[i + size] != input[i])
            {
                failed_match = 1;
                break;
            }
        }
        if (!failed_match)
            return 1;
    }
    return 0;
}

static byte_buffer_t super_big_rle_compress(const uint8_t * input, size_t input_len)
{
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
    
    while (i < input_len)
    {
        size_t j = i;
        size_t rle_size = 0;
        
        // try out various word sizes from 1 to 16
        for (size_t _size = 1; _size < 16; ++_size)
        {
            size_t size = _size;
            
            if (i + size > input_len)
                break;
            
            // loop forward until we hit a differing byte pair
            size_t count = _size == 1 ? 127 : 63;
            size_t j2 = i + size;
            while (j2 + size <= ((i + count * size) < input_len ? (i + count * size) : input_len))
            {
                for (size_t off = 0; off < size; off += 1)
                {
                    uint8_t c = input[j2 + off - size];
                    uint8_t b = input[j2 + off];
                    if (c != b)
                        goto outer;
                }
                j2 += size;
            }
outer:
            {
                // check if this run is more efficient than the best run
                size_t old_src = j - i;
                size_t new_src = j2 - i;
                size_t new_dest = (size + (size < 2 ? size : 2));
                size_t old_dest = ((rle_size > 1 ? rle_size : 1) + (rle_size < 2 ? rle_size : 2));
                // multiplying by opposite term is, without the int division truncation, equivalent to dividing by the correct term and then multiplying both by both terms
                size_t old_eff = old_src * new_dest;
                size_t new_eff = new_src * old_dest;
                if (new_eff > old_eff)
                {
                    rle_size = size;
                    j = j2;
                }
            }
        }
        uint8_t n = (j - i) / rle_size - 1;
        // if we didn't find any RLE, store a literal
        if (n == 0)
        {
            uint16_t size;
            for (size = 17; size < (1 << 14); size += 1)
            {
                if (i + size > input_len)
                    break;
                if (has_efficient_rle(&input[i + size], input_len - (i + size)))
                    break;
            }
            size -= 1;
            
            byte_push(&ret, 0xC0 | (size & 0x3F));
            byte_push(&ret, size >> 6);
            for (size_t j = 0; j < size; ++j)
                byte_push(&ret, input[i++]);
            continue;
        }
        // if we found RLE, store the RLE
        if (rle_size > 1)
        {
            byte_push(&ret, n | 0x80);
            byte_push(&ret, rle_size - 2);
        }
        else
            byte_push(&ret, n);
        for (size_t off = 0; off < rle_size; ++off)
            byte_push(&ret, input[i + off]);
        i = j;
    }
    
    return ret;
}

static byte_buffer_t super_big_rle_decompress(const uint8_t * input, size_t input_len)
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
        // literal
        // in RLE mode, bits 7 and 6 cannot be set at the same time, so this works as a signal
        if ((dat & 0xC0) == 0xC0)
        {
            uint16_t size = 0;
            size |= dat & 0x3F;
            size |= ((uint16_t)input[i++]) << 6;
            
            bytes_push(&ret, &input[i], size);
            i += size;
        }
        // RLE
        else
        {
            // number of extra repetitions is stored in the lower 7 bits of dat
            size_t n = (dat & 0x7F) + 1;
            
            // single-byte word mode (n can be up to 127)
            if (!(dat & 0x80))
            {
                uint8_t c = input[i++];
                byte_push_many(&ret, c, n);
            }
            // long word mode (note: n will be at most 16)
            else
            {
                size_t rle_size = input[i++] + 2;
                bytes_push(&ret, &input[i], rle_size);
                i += rle_size;
                
                for (size_t j = 0; j < (n - 1); j += 1)
                    bytes_push(&ret, &ret.data[ret.len - rle_size], rle_size);
            }
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
    uint64_t counts[256];
    memset(counts, 0, sizeof(uint64_t) * 256);
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

// returned data must be freed by the caller; it was allocated with BARPH_MALLOC
static uint8_t * barph_compress(uint8_t * data, size_t len, uint8_t do_rle, uint8_t do_huff, uint8_t do_diff, size_t * out_len)
{
    byte_buffer_t buf = {data, len, len};
    
    if (do_diff)
    {
        for (size_t i = buf.len - 1; i >= do_diff; i -= 1)
            buf.data[i] -= buf.data[i - do_diff];
    }
    if (do_rle)
    {
        byte_buffer_t new_buf = super_big_rle_compress(buf.data, buf.len);
        BARPH_FREE(buf.data);
        buf = new_buf;
    }
    if (do_huff)
    {
        byte_buffer_t new_buf = huff_pack(buf.data, buf.len).buffer;
        BARPH_FREE(buf.data);
        buf = new_buf;
    }
    
    *out_len = buf.len;
    return buf.data;
}
// returned data must be freed by the caller; it was allocated with BARPH_MALLOC
static uint8_t * barph_decompress(uint8_t * data, size_t len, size_t * out_len)
{
    byte_buffer_t buf = {data, len, len};
    
    if (buf.len < 8 || memcmp(buf.data, "bRPH", 5) != 0)
    {
        puts("invalid barph file");
        exit(0);
    }
    uint8_t do_diff = buf.data[5];
    uint8_t do_rle = buf.data[6];
    uint8_t do_huff = buf.data[7];
    
    buf.data += 8;
    buf.len -= 8;
    
    if (do_huff)
    {
        bit_buffer_t compressed;
        memset(&compressed, 0, sizeof(bit_buffer_t));
        compressed.buffer = buf;
        byte_buffer_t new_buf = huff_unpack(&compressed);
        BARPH_FREE(buf.data - 8);
        buf = new_buf;
    }
    if (do_rle)
    {
        byte_buffer_t new_buf = super_big_rle_decompress(buf.data, buf.len);
        BARPH_FREE(buf.data);
        buf = new_buf;
    }
    if (do_diff)
    {
        for (size_t i = do_diff; i < buf.len; i += 1)
            buf.data[i] += buf.data[i - do_diff];
    }
    
    *out_len = buf.len;
    return buf.data;
}