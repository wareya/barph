# LOH

Lookback and huffman compression codec as a single-header library.

Public domain, extremely small (around 700 lines of actual C code) lookback (LZ77-style) and Huffman compression.

LOH's compression ratio and speed are mediocre.

LOH is meant to be embedded into other applications, not used as a general purpose compression tool. The encoder and decoder implemented here are not streaming, but streaming encoders and decoders are possible and shouldn't be too hard to write.

This project compiles cleanly both as C and C++ code.

Not fuzzed.

No, I don't know why the decompression is so slow.

## Comparison

**(not yet updated)**

Made with the `zip`, `unzip`, and `lz4` commands from msys2's repositories, and loh.c compiled as -O3 (without -march=native). Dashes refer to compression level. loh doesn't have a 'standard' compression level setting, only technique flags, so the technique flags that differ from the defaults are noted instead (l means lookback, h means huffman, d means delta). The best loh flags are used for the given file. If the default flags are the best, alternative flags are not attempted.

file | compressed size | encode time (best of 3) | decode time (best of 3)
-|-|-|-
blake recorded 11.wav | 27002 KB | - | -
blake recorded 11.wav.-9.lz4 | 26376 KB | 0.972s | **0.164s**
blake recorded 11.wav.-9.zip | 25911 KB | 1.407s | 0.253s
-|-|-|-
cc0_photo.tga | 3729 KB | - | -
cc0_photo.tga.-9.lz4 | 2754 KB | 0.292s | 0.132s
cc0_photo.tga.-9.zip | 2466 KB | 0.308s | **0.065s**
-|-|-|-
unifont-jp.tga | 65537 KB | - | -
unifont-jp.tga.-9.lz4 | 2716 KB | 4.957s | **0.198s**
unifont-jp.tga.-9.zip | **1492 KB** | 16.706s | 0.263s
-|-|-|-
moby dick.txt | 1247 KB | - | -
moby dick.txt.-9.lz4 | 583 KB | 0.195s | 0.120s
moby dick.txt.-9.zip | **500 KB** | 0.151s | **0.041s**
-|-|-|-
Godot_v4.1.3-stable_win64.exe | 117560 KB | - | -
Godot_v4.1.3-stable_win64.exe.-9.lz4 | 60210 KB | **4.161s** | **0.303s**
Godot_v4.1.3-stable_win64.exe.-9.zip | **53651 KB** | 11.367s | 1.163s

