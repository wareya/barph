#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "barph_impl.h"

int main(int argc, char ** argv)
{
    if (argc < 3 || (argv[1][0] != 'z' && argv[1][0] != 'x'))
    {
        puts("usage: barph (z|x) <in> <out> [0|1] [0|1] [number]");
        puts("z: compress <in> into <out>");
        puts("x: decompress <in> into <out>");
        puts("The three numeric arguments at the end are for z (compress) mode.");
        puts("The first turns on RLE. RLE alone can give up to a 1:127 compression ratio, at most.");
        puts("The second turns on Huffman coding. Huffman coding alone can give up to a 1:8 compression ratio, at most.");
        puts("The third turns on delta coding, with a byte distance. 3 works good for 3-channel RGB images, 4 works good for 3-channel RGBA images or 16-bit PCM audio. Only if they're not already compressed, though. Does not generally work well with most files, like text.");
        puts("If given, the numeric arguments must be given in order. If not given, their defaults are 1, 1, 0.");
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
        
        buf.data = barph_compress(buf.data, buf.len, do_rle, do_huff, do_diff, &buf.len);
        
        FILE * f2 = fopen(argv[3], "wb");
        
        fwrite("bRPH", 5, 1, f2);
        fwrite(&do_diff, 1, 1, f2);
        fwrite(&do_rle, 1, 1, f2);
        fwrite(&do_huff, 1, 1, f2);
        
        fwrite(buf.data, buf.len, 1, f2);
        fclose(f2);
    }
    else if (argv[1][0] == 'x')
    {
        buf.data = barph_decompress(buf.data, buf.len, &buf.len);
        
        puts("saving...");
        FILE * f2 = fopen(argv[3], "wb");
        fwrite(buf.data, buf.len, 1, f2);
        fclose(f2);
    }
    
    free(buf.data);
}