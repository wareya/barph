#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "loh_impl.h"

int main(int argc, char ** argv)
{
    if (argc < 4 || (argv[1][0] != 'z' && argv[1][0] != 'x'))
    {
        puts("usage: loh (z|x) <in> <out> [0|1] [0|1] [number]");
        puts("");
        puts("z: compresses <in> into <out>");
        puts("x: decompresses <in> into <out>");
        puts("");
        puts("The three numeric arguments at the end are for z (compress) mode.");
        puts("");
        puts("The first turns on lookback.");
        puts("");
        puts("The second turns on Huffman coding.");
        puts("");
        puts("The third turns on delta coding, with a byte distance. 3 does good for\n"
            "3-channel RGB images, 4 does good for 4-channel RGBA images or 16-bit\n"
            "PCM audio. Only if they're not already compressed, though. Does not\n"
            "generally work well with most files, like text.");
        puts("");
        puts("If given, the numeric arguments must be given in order. If not given,\n"
            "their defaults are 1, 1, 0. In other words, RLE and Huffman are enabled\n"
            "by default, but delta coding is not.");
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
    
    uint8_t * raw_data = (uint8_t *)malloc(file_len);
    fread(raw_data, file_len, 1, f);
    byte_buffer_t buf = {raw_data, file_len, file_len};
    
    fclose(f);
    
    if (argv[1][0] == 'z')
    {
        uint8_t do_diff = 0;
        uint8_t do_rle = 1;
        uint8_t do_huff = 1;
        
        if (argc > 4)
            do_rle = strtol(argv[4], 0, 10);
        if (argc > 5)
            do_huff = strtol(argv[5], 0, 10);
        if (argc > 6)
            do_diff = strtol(argv[6], 0, 10);
        
        buf.data = loh_compress(buf.data, buf.len, do_rle, do_huff, do_diff, &buf.len);
        
        FILE * f2 = fopen(argv[3], "wb");
        
        fwrite(buf.data, buf.len, 1, f2);
        
        fclose(f2);
    }
    else if (argv[1][0] == 'x')
    {
        buf.data = loh_decompress(buf.data, buf.len, &buf.len);
        
        if (buf.data)
        {
            FILE * f2 = fopen(argv[3], "wb");
            fwrite(buf.data, buf.len, 1, f2);
            fclose(f2);
        }
        else
        {
            fprintf(stderr, "error: decompression failed");
            exit(-1);
        }
    }
    
    free(buf.data);
}