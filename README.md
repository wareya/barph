# LOH

Lookback and Huffman compression codec as a single-header library.

Public domain, extremely small* lookback (LZSS) and Huffman compression, with optional delta coding.

LOH's compressor is fast, and its decompressor is slightly slower than `unzip` and `lz4` (the commands). Its compression ratio is mediocre, except on uncompressed audio and images, where it outperforms codecs that don't support delta coding, and files that are overwhelmingly dominated by a single byte value, where it outperforns most codecs, including `zip` and `lz4` (the commands).

LOH is meant to be embedded into other applications, not used as a general purpose compression tool. The encoder and decoder implemented here are not streaming, but streaming encoders and decoders are possible and shouldn't be too hard to write.

LOH is good for applications that have to compress lots of data quickly, especially images, and also for applications that need a single-header compression library.

LOH can be modified to compress more aggressively at the cost of performance, but even then, it usually doesn't perform as well on non-image/audio files as `zip` does. To make it more aggressive, change LOH_HASH_SIZE to 16 and LOH_HASHTABLE_KEY_SHL to a number greater than 2 (usually 3, 4, or 5; increasing it has an exponential cost, both in time and memory usage).

This project compiles cleanly both as C and C++ code without warnings or errors. Requires C99 or C++11 or newer.

Not fuzzed. However, the compressor is probably perfectly safe, and the decompressor is probably safe on trusted/correct data.

\* Around 850 lines of actual code according to `cloc`. The file itself is over 1k lines because it's well-commented.

## Comparison

Made with the `zip`, `unzip`, and `lz4` commands from msys2's repositories, and loh.c compiled as -O3 (without -march=native). Dashes refer to compression level. loh doesn't have a 'standard' compression level setting, only technique flags, so the technique flags that differ from the defaults are noted instead (l means lookback, h means Huffman, d means delta). The best loh flags are used for the given file. If the default flags are the best, alternative flags are not attempted.

The benchmarks were run at different times, by hand, on an in-use desktop computer, so the times might not be consistent or fair, but they should be within 5% what they really are.

file | compressed size | encode time (best of 3) | decode time (best of 3)
-|-|-|-
white noise.bin | **27002 KB** | - | -
white noise.bin.-9.lz4 | 27003 KB | 0.857s | **0.148s**
white noise.bin.-9.zip | 27007 KB | 0.880s | 0.175s
white noise.bin.loh | 27006 KB | **0.689s** | 0.216s
-|-|-|-
oops all zeroes.bin | 27002 KB | - | -
oops all zeroes.bin.-9.lz4 | 107 KB * | 0.136s | 0.134s
oops all zeroes.bin.-9.zip | 27 KB | 0.173s | 0.139s
oops all zeroes.bin.loh | **462 Bytes** | **0.135s** | **0.109s**
-|-|-|-
blake recorded 11.wav | 27002 KB | - | -
blake recorded 11.wav.-9.lz4 | 26376 KB | 0.972s | **0.164s**
blake recorded 11.wav.-9.zip | 25911 KB | 1.407s | 0.253s
blake recorded 11.wav.loh | 26134 KB | **0.749s** | 0.339s
blake recorded 11.wav.d4.loh | **24464 KB** | 0.782s | 0.318s
-|-|-|-
cc0_photo.tga | 3729 KB | - | -
cc0_photo.tga.-9.lz4 | 2754 KB | 0.292s | 0.132s
cc0_photo.tga.-9.zip | 2466 KB | 0.308s | **0.065s**
cc0_photo.tga.loh | 2775 KB | 0.161s | 0.104s
cc0_photo.tga.l0d3.loh | **2223 KB** | **0.118s** | 0.098s
-|-|-|-
unifont-jp.tga | 65537 KB | - | -
unifont-jp.tga.-9.lz4 | 2716 KB | 4.957s | **0.198s**
unifont-jp.tga.-9.zip | **1492 KB** | 16.706s | 0.263s
unifont-jp.tga.loh | 2972 KB | **0.388s** | 0.224s
-|-|-|-
moby dick.txt | 1247 KB | - | -
moby dick.txt.-9.lz4 | 583 KB | 0.195s | 0.120s
moby dick.txt.-9.zip | **500 KB** | 0.151s | **0.041s**
moby dick.txt.loh | 634 KB | **0.104s** | 0.104s
-|-|-|-
Godot_v4.1.3-stable_win64.exe | 117560 KB | - | -
Godot_v4.1.3-stable_win64.exe.-9.lz4 | 60210 KB | 4.161s | **0.303s**
Godot_v4.1.3-stable_win64.exe.-9.zip | **53651 KB** | 11.367s | 1.163s
Godot_v4.1.3-stable_win64.exe.loh | 63349 KB | **3.251s** | 1.295s

\* LZ4 has a maximum compression ratio of 1:256-ish (because of how it stores long integers)

## Format

LOH has three compression steps:

1) An optional delta step that differentiates the file with an arbitrary comparison distance from 1 to 255 (inclusive). Unsigned 8-bit subtraction, overflow wraps around. This does not change the size of the file, but it can make the following steps more efficient for some types of file.
2) An optional LZSS-style lookback stage, where a given run of output bytes can either be encoded as a literal, or a lookback reference defined by distance and length (and the distance is allowed to be less than the length).
3) An optional Huffman coding stage, using a length-limited canonical Huffman code.

These steps are performed in order on the entire output of the previous stage. So, while step 2 has an output length prefix, that length prefix is compressed by step 3, and then step 3 has its own output length prefix as well.

All three steps are optional, and whether they're done is stored in the header. This means you can use LOH as a preprocessor or postprocessor for other formats, e.g. applying delta coding to an image before `zip`ing it, or applying Huffman coding to an `lz4` file.

### Lookback

The LZSS-style layer works strictly with bytes, not with a bitstream. When the decoder's control loop reads a byte, it looks at the least-significant bit (i.e. it calculates `byte & 1`). If the bit is set, then the current byte is the start of a lookback command. If it's unset, then the current byte is the start of a literal sequence, i.e. uncompressed data. Literal control byte sequences store just the number of bytes that follow. Lookback sequences encode first the lookback distance and then the length (number of bytes to copy). A description of every possible control byte sequence follows:

```
Literals:

xxxx xx00 ...
literals, ... contains <xxxx xx> literals

xxxx xx10 u8 ...
long literals, ... contains <u8 xxxx xx> literals

Lookback command distance parts:

xxxx xx01
Lookback has a distance <xxxx xx>.

xxxx x011 +u8
13-bit lookback
Lookback has a distance <u8 xxxx x>.

xxxx 0111 +u16
20-bit lookback
Lookback has a distance <u16 xxxx>. The u16 is least-significant-chunk-first.

xxx0 1111 +u24
27-bit lookback
Etc.

xx01 1111 +u32
34-bit lookback

The distance part is followed by a lookback length sequence.

Lookback lengths:

xxxx xxx0
Lookback has a length of <xxxx xxx>.

xxxx xxx1 u8
Lookback has a length of <u8 xxxx xxx>.
```

Numbers stored in multiple bytes are stored with the less-significant parts of that number in earlier bytes. For example, if you decode 4 bits from byte 0, 7 bits from byte 1, and 2 bytes from byte 2, and combine them into a single value, the layout of bits from most significant to least significant should have the following byte origins: `2 2111 1111 0000`

### Huffman coding

LOH uses canonical Huffman codes to allow for faster decoding.

Also, this stage uses individual bit access, unlike the lookback stage. The encoder writes bits starting with the first bit in the least-significant bit of the first byte (the 1 bit), going up to the most-significant bit (the 128 bit), then going on to the 1 bit of the second byte, and so on.

The Huffman code table is stored at the start of the output of this stage, in order from shortest to longest code, in order of code value, starting with a code length of 1. The encoder outputs a `1` bit to indicate that it's moved on to the next code length, or a `0` to indicate that there's another symbol associated with this code length, followed by whatever that symbol is. Symbols are encoded least-significant-bit-first. So, if the shortest code is 3 bits long, and maps to the character 'a' (byte 0x61), then the encoder ouputs `1`, `1` (advance from length 1 to 2, then from 2 to 3), `0`, (symbol at current code length follows), `1`, `0`, `0`, `0`, `0`, `1`, `1`, `0` (bits of 0x61 in order). In a hex editor, this would look like `0B 03`. This is space-inefficient, but allows for near-ideally-fast construction of all the relevant tables while decoding the huffman code table..

Before the code table, the number of symbols coded in it minus one is written as an 8-bit integer. So if there are 8 symbols `0x07` is written. This is how the decoder knows when to stop decoding the code table and start decoding the compressed data.

After the code table, the encoder rounds its bit cursor up to the start of the next byte, allowing the decoder to work on a byte-by-byte basis when decompressing the compressed data rather instead of slow bit-banging.

Huffman codes in the compressed data are stored starting from the most significant bit of each code word (i.e. the root of the Huffman tree) and working towards the least significant bit. These bits are written into the output buffer starting at the least-significant bit of a given byte and working towards the most-significant bit of that byte, before moving on to the next byte, where bits also start out being stored in the least-significant bit.


