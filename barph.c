#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t * data;
    size_t len;
    size_t cap;
} byte_buffer_t;

void byte_push(byte_buffer_t * buf, uint8_t byte)
{
    if (buf->len >= buf->cap)
    {
        buf->cap = (buf->cap) << 1;
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
int is_pow_2(unsigned int x)
{
    return !(x & (x - 1));
}

size_t super_big_rle_compress_size_estimate(const uint8_t *input, size_t input_len)
{
    size_t skip_most_above = 16;
    size_t skip_most_under = 256;
    
    size_t len = 0;
    size_t i = 0;
    while (i < input_len)
    {
        size_t j = i;
        size_t rle_size = 0;
        
        for (size_t size = 1; size <= 257; ++size)
        {
            if (i + size > input_len)
                break;
            if (size > skip_most_above && size < skip_most_under && !is_pow_2(size / 3))
                continue;
            size_t j2 = i + size;
            while (j2 + size <= (i + 127 * size < input_len ? i + 127 * size : input_len))
            {
                int match = 1;
                for (size_t off = 0; off < size; ++off) {
                    uint8_t c = input[j2 + off - size];
                    uint8_t b = input[j2 + off];
                    if (c != b)
                    {
                        match = 0;
                        break;
                    }
                }
                if (!match)
                    break;
                j2 += size;
            }
            size_t old_src = j - i;
            size_t new_src = j2 - i;
            size_t old_eff = old_src * (size + (size < 2 ? size : 2));
            size_t new_eff = new_src * (rle_size > 0 ? rle_size > 1 ? rle_size : 2 : 1);
            if (new_eff > old_eff)
            {
                rle_size = size;
                j = j2;
            }
        }
        
        if (rle_size > 1)
            len += 1;
        len += rle_size + 1;
        i = j;
    }
    return len;
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
            if (size > skip_most_above && size < skip_most_under && !is_pow_2(size / 3))
                continue;
            int last_test = 0;
            if (size >= 4)
            {
                size_t test_buf_len = (i + size + pattern_check_length) < (i + 257) ? (i + size + pattern_check_length) : (i + 257);
                test_buf_len = test_buf_len < input_len ? test_buf_len : input_len;
                size_t rest_compressed = super_big_rle_compress_size_estimate(&input[i + size], test_buf_len - (i + size));
                last_test = rest_compressed < (test_buf_len - (i + size));
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
                    if (size > 1 && j2 >= 2 && (j2 - i) / size == 1)
                    {
                        // prevent "literal" entries from having trails of single identical bytes at the end
                        int did_loop = 0;
                        while (input[j2 - 1] == input[j2 - 2])
                        {
                            did_loop = 1;
                            j2 -= 1;
                            size -= 1;
                        }
                        if (did_loop)
                        {
                            j2 -= 1;
                            size -= 1;
                        }
                    }
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

byte_buffer_t super_big_rle_decompress(const uint8_t *input, size_t input_len)
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
    
    while (i < input_len)
    {
        int has_size = (input[i] & 0x80) != 0;
        size_t n = (input[i] & 0x7F) + 1;
        i++;

        if (!has_size)
        {
            uint8_t c = input[i++];
            for (size_t j = 0; j < n; ++j)
                byte_push(&ret, c);
        }
        else
        {
            size_t rle_size = input[i++] + 2;
            for (size_t j = 0; j < rle_size; j += 1)
                byte_push(&ret, input[i + j]);
            i += rle_size;
            
            for (size_t j = 0; j < (n - 1); j += 1)
                for (size_t k = 0; k < rle_size; k += 1)
                    byte_push(&ret, ret.data[ret.len - rle_size]);
        }
    }

    return ret;
}

typedef struct _huff_node {
    uint8_t symbol;
    bit_buffer_t code;
    int64_t freq;
    struct _huff_node * left;
    struct _huff_node * right;
} huff_node_t;

void push_code(huff_node_t * node, uint8_t bit)
{
    bits_push(&node->code, bit, 1);
    if (node->left)
        push_code(node->left, bit);
    if (node->right)
        push_code(node->right, bit);
}

int count_compare(const void * a, const void * b)
{
    int64_t n = *((int64_t*)b) - *((int64_t*)a);
    return n > 0 ? 1 : n < 0 ? -1 : 0;
}

void push_huff_node(bit_buffer_t * buf, huff_node_t * node)
{
    if (node->left && node->right)
    {
        bits_push(buf, 1, 1);
        push_huff_node(buf, node->left);
        push_huff_node(buf, node->right);
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
    ret = malloc(sizeof(huff_node_t));
    ret->symbol = 0;
    ret->code = (bit_buffer_t){{0, 0, 0}, 0, 0, 0};
    ret->freq = 0;
    ret->left = 0;
    ret->right = 0;
    
    int children = bits_pop(buf, 1);
    if (children)
    {
        ret->left = pop_huff_node(buf);
        ret->right = pop_huff_node(buf);
        push_code(ret->left, 0);
        push_code(ret->right, 1);
    }
    else
    {
        ret->symbol = bits_pop(buf, 8);
    }
    
    return ret;
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
        unordered_dict[i] = malloc(sizeof(huff_node_t));
        unordered_dict[i]->symbol = counts[i] & 0xFF;
        unordered_dict[i]->code = (bit_buffer_t){{0, 0, 0}, 0, 0, 0};
        unordered_dict[i]->freq = counts[i] >> 8;
        unordered_dict[i]->left = 0;
        unordered_dict[i]->right = 0;
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
        huff_node_t * new_node = malloc(sizeof(huff_node_t));
        new_node->symbol = 0;
        new_node->code = (bit_buffer_t){{0, 0, 0}, 0, 0, 0};
        new_node->freq = lowest->freq + next_lowest->freq;
        new_node->left = lowest;
        new_node->right = next_lowest;
        
        push_code(new_node->left , 0);
        push_code(new_node->right, 1);
        
        // insert new out-queue element at bottom
        queue_out_count += 1;
        for (size_t i = queue_out_count; i > 0; i -= 1)
            queue_out[i] = queue_out[i-1];
        queue_out[0] = new_node;
    }
    
    bit_buffer_t ret = (bit_buffer_t){{0, 0, 0}, 0, 0, 0};
    
    bits_push(&ret, len, 8*8);
    
    push_huff_node(&ret, queue_out[0]);
    
    printf("packing at... %d %lld\n", ret.bit_index, ret.byte_index);
    
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
    
    huff_node_t * root = pop_huff_node(buf);
    
    printf("unpacking at... %d %lld\n", buf->bit_index, buf->byte_index);
    
    for(size_t i = 0; i < len; i++)
    {
        huff_node_t * node = root;
        while (node->left || node->right)
            node = !bits_pop(buf, 1) ? node->left : node->right;
        byte_push(&ret, node->symbol);
    }
    return ret;
}

int main()
{
    //FILE * f = fopen("data/font.tga", "rb");
    FILE * f = fopen("data/ufeff_tiles_v2.tga", "rb");
    fseek(f, 0, SEEK_END);
    size_t file_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    byte_buffer_t buf = {0, 0, 0};
    for (size_t i = 0; i < file_len; i += 1)
    {
        int c = fgetc(f);
        if (c != EOF)
            byte_push(&buf, c);
        else
        {
            fprintf(stderr, "unhandled error %d (file len 0x%08llX)", ferror(f), file_len);
            exit(-1);
        }
    }
    
    fclose(f);
    
    byte_buffer_t rle_compressed = super_big_rle_compress(buf.data, buf.len);
    
    
    //printf("%0llX %0llX\n", (uint64_t)queue_in_count, (uint64_t)queue_out_count);
    
    //print_node(queue_out[0], 0);
    
    /*
    for (size_t b = 0; b < 256; b++)
    {
        printf("%03lld: %04lld\t", counts[b] & 0xFF, counts[b] >> 8);
        if ((b+1) % 8 == 0)
            puts("");
    }
    puts("huffman codes:");
    for (size_t b = 0; b < 256; b++)
    {
        if (dict[b]->freq == 0)
            continue;
        printf("%d : ", dict[b]->symbol);
        //for (size_t n = 0; n < dict[b]->code.buffer.len; n++)
        //    printf("%02X", dict[b]->code.buffer.data[n]);
        dict[b]->code.bit_index = 0;
        dict[b]->code.byte_index = 0;
        for (size_t n = 0; n < dict[b]->code.bit_count; n++)
        {
            //int bit = bits_pop(&dict[b]->code, 1);
            size_t m = dict[b]->code.bit_count - n - 1;
            int bit = (dict[b]->code.buffer.data[m / 8] >> ((m % 8))) & 1;
            printf(bit ? "1" : "0");
        }
        puts("");
    }
    */
    
    bit_buffer_t huff_compressed = huff_pack(rle_compressed.data, rle_compressed.len);
    byte_buffer_t huff_decompressed = huff_unpack(&huff_compressed);
    byte_buffer_t decompressed = super_big_rle_decompress(huff_decompressed.data, huff_decompressed.len);
    
    //byte_buffer_t decompressed = super_big_rle_decompress(rle_compressed.data, rle_compressed.len);
    
    //for (long long unsigned int i = 0; i < compressed.len; i += 1)
    //    printf("0x%02X, ", compressed.data[i]);
    
    FILE * f0 = fopen("_test_asdf_dummy.out", "w");
    fwrite(buf.data, 1, buf.len, f0);
    
    FILE * f2 = fopen("_test_asdf.out", "w");
    fwrite(rle_compressed.data, 1, rle_compressed.len, f2);
    
    FILE * f4 = fopen("_test_asdf_huff.out", "w");
    fwrite(huff_compressed.buffer.data, 1, huff_compressed.buffer.len, f4);
    
    FILE * f5 = fopen("_test_asdf_huff_dec.out", "w");
    fwrite(huff_decompressed.data, 1, huff_decompressed.len, f5);
    
    FILE * f3 = fopen("_test_asdf_dec.out", "w");
    fwrite(decompressed.data, 1, decompressed.len, f3);
    
    free(buf.data);
}