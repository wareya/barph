# LOH

Lookback and huffman compression codec as a single-header library.

Public domain, extremely small* lookback (LZSS) and Huffman compression, with optional delta coding.

LOH's compression ratio is mediocre, except on audio and images, where its compression ratio outperforms codecs that don't perform delta coding. LOH's compressor is faster than others. Its decompressor is much slower.

LOH is meant to be embedded into other applications, not used as a general purpose compression tool. The encoder and decoder implemented here are not streaming, but streaming encoders and decoders are possible and shouldn't be too hard to write.

The lack of streaming is probably why the decompressor is so slow.

LOH is good for applications that have to compress lots of data quickly even if the compression ratio isn't very good, and also for applications that need a single-header compression library.

LOH can be modified to compress more aggressively at the cost of performance, but even then, it usually doesn't perform as well on non-image/audio files as `zip` does. To make it more aggressive, change LOH_HASH_SIZE to 16 and LOH_HASHTABLE_KEY_SHL to a number greater than 2 (usually 3, 4, or 5; increasing it has an exponential cost, both in time and memory usage).

This project compiles cleanly both as C and C++ code without warnings or errors. Requires C99 or C++11 or newer.

Not fuzzed. However, the compressor is probably perfectly safe, and the decompressor is probably safe on trusted/correct data.

\* Around 850 lines of actual code according to `cloc`. The file itself is over 1k lines because it's well-commented.

## Comparison

Made with the `zip`, `unzip`, and `lz4` commands from msys2's repositories, and loh.c compiled as -O3 (without -march=native). Dashes refer to compression level. loh doesn't have a 'standard' compression level setting, only technique flags, so the technique flags that differ from the defaults are noted instead (l means lookback, h means huffman, d means delta). The best loh flags are used for the given file. If the default flags are the best, alternative flags are not attempted.

file | compressed size | encode time (best of 3) | decode time (best of 3)
-|-|-|-
white noise.bin | **27002 KB** | - | -
white noise.bin.-9.lz4 | 27003 KB | 0.857s | **0.148s**
white noise.bin.-9.zip | 27007 KB | 0.880s | 0.175s
white noise.bin.loh | 27006 KB | **0.726s** | 0.271s
-|-|-|-
oops all zeroes.bin | 27002 KB | - | -
oops all zeroes.bin.-9.zip | 107 KB * | 0.136s | **0.134s**
oops all zeroes.bin.-9.zip | 27 KB | 0.173s | 0.139s
oops all zeroes.bin.loh | **462 Bytes** | **0.139s** | 0.152s
-|-|-|-
blake recorded 11.wav | 27002 KB | - | -
blake recorded 11.wav.-9.lz4 | 26376 KB | 0.972s | **0.164s**
blake recorded 11.wav.-9.zip | 25911 KB | 1.407s | 0.253s
blake recorded 11.wav.loh | 26134 KB | **0.780s** | 0.383s
blake recorded 11.wav.d4.loh | **24464 KB** | 0.801s | 0.361s
-|-|-|-
cc0_photo.tga | 3729 KB | - | -
cc0_photo.tga.-9.lz4 | 2754 KB | 0.292s | 0.132s
cc0_photo.tga.-9.zip | 2466 KB | 0.308s | **0.065s**
cc0_photo.tga.loh | 2775 KB | 0.166s | 0.107s
cc0_photo.tga.l0d3.loh | **2223 KB** | **0.104s** | 0.099s
-|-|-|-
unifont-jp.tga | 65537 KB | - | -
unifont-jp.tga.-9.lz4 | 2716 KB | 4.957s | **0.198s**
unifont-jp.tga.-9.zip | **1492 KB** | 16.706s | 0.263s
unifont-jp.tga.loh | 2972 KB | **0.390s** | 0.381s
-|-|-|-
moby dick.txt | 1247 KB | - | -
moby dick.txt.-9.lz4 | 583 KB | 0.195s | 0.120s
moby dick.txt.-9.zip | **500 KB** | 0.151s | **0.041s**
moby dick.txt.loh | 634 KB | **0.104s** | 0.103s
-|-|-|-
Godot_v4.1.3-stable_win64.exe | 117560 KB | - | -
Godot_v4.1.3-stable_win64.exe.-9.lz4 | 60210 KB | 4.161s | **0.303s**
Godot_v4.1.3-stable_win64.exe.-9.zip | **53651 KB** | 11.367s | 1.163s
Godot_v4.1.3-stable_win64.exe.loh | 63349 KB | **3.417s** | 1.597s

\* LZ4 has a maximum compression ratio of 1:256-ish (because of how it stores long integers)
