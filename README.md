# barph

Bad RLE Plus Huffman compression format

Public domain, extremely small (around 500 lines of actual C code) run-length-encoding and Huffman compression.

In terms of compression ratio, Barph outperforms lz4 on most inputs, but almost never outperforms DEFLATE. It decodes slower than both.

Barph's purpose is to be embedded into applications that need compression where it's more important for the code to be comprehensible than ba fast or have a high compression ratio. The decoder implemented here is fairly slow, taking around 3x~4x the time that `unzip` takes to decompress a file, but the code is VERY simple and easy to audit. The encoder and decoder implemented here are not streaming, but streaming decoders are possible and shouldn't be too hard to write. Streaming encoders are also possible, but only with lookahead, and only if either huffman coding is disabled or a precomputed huffman tree is used.

This project compiles cleanly both as C and C++ code.

Not fuzzed.
