#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t * data;
    size_t len;
    size_t cap;
} byte_buffer_t;

void bytes_reserve(byte_buffer_t * buf, size_t extra)
{
    if (buf->cap < 8)
        buf->cap = 8;
    while (buf->len + extra > buf->cap)
        buf->cap <<= 1;
    buf->data = realloc(buf->data, buf->cap);
}
void bytes_push(byte_buffer_t * buf, const uint8_t * bytes, size_t count)
{
    bytes_reserve(buf, count);
    memcpy(&buf->data[buf->len], bytes, count);
    buf->len += count;
}
void byte_push_many(byte_buffer_t * buf, uint8_t byte, size_t count)
{
    bytes_reserve(buf, count);
    memset(&buf->data[buf->len], byte, count);
    buf->len += count;
}
void byte_push(byte_buffer_t * buf, uint8_t byte)
{
    if (buf->len == buf->cap)
    {
        buf->cap = buf->cap << 1;
        if (buf->cap < 8)
            buf->cap = 8;
        buf->data = realloc(buf->data, buf->cap);
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

void bits_push(bit_buffer_t * buf, uint64_t data, uint8_t bits)
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
void bit_push(bit_buffer_t * buf, uint8_t data)
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
uint64_t bits_pop(bit_buffer_t * buf, uint8_t bits)
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
uint8_t bit_pop(bit_buffer_t * buf)
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
int is_pow_2(unsigned int x)
{
    return !(x & (x - 1));
}

int has_simple_rle(const uint8_t * input, size_t input_len)
{
    for (size_t size = 1; size <= 4; size += 1)
    {
        if (size * 2 > input_len)
            break;
        
        int failed_match = 0;
        for (size_t i = size; i < input_len; i += 1)
        {
            if (input[i - size] != input[i])
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

byte_buffer_t super_big_rle_compress(const uint8_t * input, size_t input_len)
{
    byte_buffer_t ret = {0, 0, 0};
    
    byte_push(&ret, (input_len >> 56) & 0xFF);
    byte_push(&ret, (input_len >> 48) & 0xFF);
    byte_push(&ret, (input_len >> 40) & 0xFF);
    byte_push(&ret, (input_len >> 32) & 0xFF);
    byte_push(&ret, (input_len >> 24) & 0xFF);
    byte_push(&ret, (input_len >> 16) & 0xFF);
    byte_push(&ret, (input_len >> 8) & 0xFF);
    byte_push(&ret, input_len & 0xFF);
    
    size_t skip_most_above = 16;
    size_t skip_most_under = 256;
    size_t pattern_check_length = 16;
    
    size_t i = 0;
    
    while (i < input_len)
    {
//        uint8_t c = input[i];
        size_t j = i;
        size_t rle_size = 0;
        
        for (size_t _size = 1; _size <= 256; ++_size)
        {
            size_t size = _size;
            if (i + size > input_len)
                break;
            if (size > skip_most_above && size < skip_most_under && !is_pow_2(size))
                continue;
            
            int last_test = 0;
            if (size >= 4)
            {
                size_t test_buf_len = (i + size + pattern_check_length) < (i + 257) ? (i + size + pattern_check_length) : (i + 257);
                test_buf_len = test_buf_len < input_len ? test_buf_len : input_len;
                last_test = has_simple_rle(&input[i + size], test_buf_len - (i + size));
            }
            size_t j2 = i + size;
            while (j2 + size <= ((i + 127 * size) < input_len ? (i + 127 * size) : input_len))
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
                size_t old_src = j - i;
                size_t new_src = j2 - i;
                // multiplying by opposite term is, without the int division truncation, equivalent to dividing by the correct term and then multiplying both by both terms
                size_t old_eff = old_src * (size + (size < 2 ? size : 2));
                size_t new_eff = new_src * ((rle_size > 1 ? rle_size : 1) + (rle_size < 2 ? rle_size : 2));
                if (new_eff > old_eff)
                {
                    rle_size = size;
                    j = j2;
                }
                if (last_test)
                    break;
            }
        }
        
        uint8_t n = (j - i) / rle_size - 1;
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

byte_buffer_t super_big_rle_decompress(const uint8_t * input, size_t input_len)
{
    size_t i = 0;
    
    uint64_t size = 0;
    size = (size << 8) | input[i++];
    size = (size << 8) | input[i++];
    size = (size << 8) | input[i++];
    size = (size << 8) | input[i++];
    size = (size << 8) | input[i++];
    size = (size << 8) | input[i++];
    size = (size << 8) | input[i++];
    size = (size << 8) | input[i++];
    
    byte_buffer_t ret = {0, 0, 0};
    
    bytes_reserve(&ret, size);
    
    while (i < input_len)
    {
        uint8_t dat = input[i++];
        size_t n = (dat & 0x7F) + 1;
        
        if (!(dat & 0x80))
        {
            uint8_t c = input[i++];
            byte_push_many(&ret, c, n);
        }
        else
        {
            size_t rle_size = input[i++] + 2;
            bytes_push(&ret, &input[i], rle_size);
            i += rle_size;
            
            for (size_t j = 0; j < (n - 1); j += 1)
                bytes_push(&ret, &ret.data[ret.len - rle_size], rle_size);
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

huff_node_t * alloc_huff_node()
{
    return malloc(sizeof(huff_node_t));
}

void push_code(huff_node_t * node, uint8_t bit)
{
    bits_push(&node->code, bit, 1);
    if (node->children[0])
        push_code(node->children[0], bit);
    if (node->children[1])
        push_code(node->children[1], bit);
}

int count_compare(const void * a, const void * b)
{
    int64_t n = *((int64_t*)b) - *((int64_t*)a);
    return n > 0 ? 1 : n < 0 ? -1 : 0;
}

void push_huff_node(bit_buffer_t * buf, huff_node_t * node)
{
    if (node->children[0] && node->children[1])
    {
        bits_push(buf, 1, 1);
        push_huff_node(buf, node->children[0]);
        push_huff_node(buf, node->children[1]);
    }
    else
    {
        bits_push(buf, 0, 1);
        bits_push(buf, node->symbol, 8);
    }
}

huff_node_t * pop_huff_node(bit_buffer_t * buf)
{
    huff_node_t * ret;
    ret = alloc_huff_node();
    ret->symbol = 0;
    ret->code = (bit_buffer_t){{0, 0, 0}, 0, 0, 0};
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

void huff_to_table(huff_node_t * huff, uint16_t ** table, size_t * len)
{
    size_t start = *len;
    *len += 3;
    *table = realloc(*table, sizeof(uint16_t) * *len);
    
    if (huff->children[0] || huff->children[1])
    {
        huff_to_table(huff->children[0], table, len);
        size_t left_len = *len - start;
        
        huff_to_table(huff->children[1], table, len);
        
        (*table)[start    ] = 3;
        (*table)[start + 1] = left_len;
        (*table)[start + 2] = 0;
    }
    else
    {
        (*table)[start    ] = 0;
        (*table)[start + 1] = 0;
        (*table)[start + 2] = huff->symbol;
    }
}

bit_buffer_t huff_pack(uint8_t * data, size_t len)
{
    // generate dictionary
    
    size_t counts[256];
    memset(counts, 0, sizeof(size_t) * 256);
    for (size_t i = 0; i < len; i += 1)
        counts[data[i]] += 256;
    for (size_t b = 0; b < 256; b++)
        counts[b] |= b;
    qsort(&counts, 256, sizeof(size_t), count_compare);
    
    huff_node_t * unordered_dict[256];
    for (size_t i = 0; i < 256; i += 1)
    {
        unordered_dict[i] = alloc_huff_node();
        unordered_dict[i]->symbol = counts[i] & 0xFF;
        unordered_dict[i]->code = (bit_buffer_t){{0, 0, 0}, 0, 0, 0};
        unordered_dict[i]->freq = counts[i] >> 8;
        unordered_dict[i]->children[0] = 0;
        unordered_dict[i]->children[1] = 0;
    }
    
    huff_node_t * dict[256];
    for (size_t i = 0; i < 256; i += 1)
        dict[unordered_dict[i]->symbol] = unordered_dict[i];
    
    huff_node_t * queue_in[256];
    size_t queue_in_count = 256;
    
    huff_node_t * queue_out[256];
    size_t queue_out_count = 0;
    
    for (size_t i = 0; i < 256; i += 1)
        queue_in[i] = unordered_dict[i];
    for (size_t i = 0; i < 256; i += 1)
        queue_out[i] = 0;
    
    while (queue_in[queue_in_count - 1]->freq == 0 && queue_in_count > 0)
        queue_in_count -= 1;
    
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
        new_node->code = (bit_buffer_t){{0, 0, 0}, 0, 0, 0};
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
    
    bit_buffer_t ret = (bit_buffer_t){{0, 0, 0}, 0, 0, 0};
    
    bits_push(&ret, len, 8*8);
    
    push_huff_node(&ret, queue_out[0]);
    
    for(size_t i = 0; i < len; i++)
    {
        uint8_t c = data[i];
        for (size_t n = 0; n < dict[c]->code.bit_count; n++)
        {
            size_t m = dict[c]->code.bit_count - n - 1;
            int bit = (dict[c]->code.buffer.data[m / 8] >> ((m % 8))) & 1;
            bits_push(&ret, bit, 1);
        }
    }
    
    return ret;
}

byte_buffer_t huff_unpack(bit_buffer_t * buf)
{
    buf->bit_index = 0;
    buf->byte_index = 0;
    size_t len = bits_pop(buf, 8*8);
    
    byte_buffer_t ret = {0, 0, 0};
    bytes_reserve(&ret, len);
    
    huff_node_t * root = pop_huff_node(buf);
    
    uint16_t * table = 0;
    size_t table_len = 0;
    huff_to_table(root, &table, &table_len);
    
    printf("unpacking at... %d %lld\n", buf->bit_index, buf->byte_index);
    
    for(size_t i = 0; i < len; i++)
    {
        huff_node_t * node = root;
        node = node->children[bit_pop(buf)];
        while (node->children[0])
            node = node->children[bit_pop(buf)];
        byte_push(&ret, node->symbol);
        /*
        uint16_t * node = table;
        node += node[bit_pop(buf)];
        while (node[0])
            node += node[bit_pop(buf)];
        byte_push(&ret, node[2]);
        */
    }
    return ret;
}

int main(int argc, char ** argv)
{
    if (argc < 3 || (argv[1][0] != 'z' && argv[1][0] != 'x'))
    {
        puts("usage: barph (z|x) <in> <out>");
        return 0;
    }
    FILE * f = fopen(argv[2], "rb");
    if (!f)
    {
        puts("error: failed to open input file");
        return 0;
    }
    fseek(f, 0, SEEK_END);
    size_t file_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t * raw_data = malloc(file_len);
    fread(raw_data, file_len, 1, f);
    byte_buffer_t buf = {raw_data, file_len, file_len};
    
    fclose(f);
    
    int do_rle = 0;
    int do_huff = 1;
    
    if (argv[1][0] == 'z')
    {
        if (do_rle)
            buf = super_big_rle_compress(buf.data, buf.len);
        if (do_huff)
            buf = huff_pack(buf.data, buf.len).buffer;
        
        FILE * f2 = fopen(argv[3], "wb");
        fwrite(buf.data, buf.len, 1, f2);
        fclose(f2);
    }
    else if (argv[1][0] == 'x')
    {
        if (do_huff)
        {
            bit_buffer_t compressed = (bit_buffer_t){buf, 0, 0, 0};
            buf = huff_unpack(&compressed);
        }
        if (do_rle)
            buf = super_big_rle_decompress(buf.data, buf.len);
        
        FILE * f2 = fopen(argv[3], "wb");
        puts("saving...");
        fwrite(buf.data, buf.len, 1, f2);
        fclose(f2);
    }
    
    free(buf.data);
}