# barph

Bad RLE Plus Huffman compression format

Public domain, extremely small (around 500 lines of actual C code) run-length-encoding and Huffman compression.

In terms of compression ratio, Barph outperforms lz4 on most inputs, but almost never outperforms DEFLATE. It decodes slower than both.

Barph's purpose is to be embedded into applications that need compression where it's more important for the code to be comprehensible than ba fast or have a high compression ratio. The decoder implemented here is fairly slow, but the code is VERY simple and easy to audit. The encoder and decoder implemented here are not streaming, but streaming decoders are possible and shouldn't be too hard to write. Streaming encoders are also possible, but only with lookahead, and only if either huffman coding is disabled or a precomputed huffman tree is used.

This project compiles cleanly both as C and C++ code.

Not fuzzed.

No I don't know why the decompression is so slow.

## Comparison

Made with the `zip`, `unzip`, and `lz4` commands from msys2's repositories, and barph.c compiled as -O3 (without -march=native). Dashes refer to compression level. barph doesn't have a 'standard' compression level setting, only technique flags, so the technique flags that differ from the defaults are noted instead. The best barph flags are used for the given file. If the default flags are the best, alternative flags are not attempted.

file | compressed size | encode time (best of 3) | decode time (best of 3)
-|-|-|-
blake recorded 11.wav | 27002 KB | - | -
blake recorded 11.wav.-9.lz4 | 26376 KB | 0.972s | **0.164s**
blake recorded 11.wav.-9.zip | 25911 KB | 1.407s | 0.253s
blake recorded 11.wav.d4.barph | **24635 KB** | **0.950s** | 0.695s
blake recorded 11.wav.barph | 24635 KB | 0.981s | 0.722s
-|-|-|-
cc0_photo.tga | 3729 KB | - | -
cc0_photo.tga.-9.lz4 | 2754 KB | 0.292s | 0.132s
cc0_photo.tga.-9.zip | 2466 KB | 0.308s | **0.065s**
cc0_photo.tga.r0.d3.barph | **2223 KB** | **0.152s** | 0.144s
cc0_photo.tga.barph | 3737 KB | 0.192s | 0.146s
-|-|-|-
unifont-jp.tga | 65537 KB | - | -
unifont-jp.tga.-9.lz4 | 2716 KB | 4.957s | **0.198s**
unifont-jp.tga.-9.zip | **1492 KB** | 16.706s | 0.263s
unifont-jp.tga.barph | 4997 KB | **0.671s** | 1.098s
-|-|-|-
moby dick.txt | 1247 KB | - | -
moby dick.txt.-9.lz4 | 583 KB | 0.195s | 0.120s
moby dick.txt.-9.zip | **500 KB** | 0.151s | **0.041s**
moby dick.txt.barph | 735 KB | **0.114s** | 0.117s
moby dick.txt.r0.barph | 722 KB | 0.115s | 0.116s
todo more
