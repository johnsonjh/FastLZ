/*
  FastLZ - Byte-aligned LZ77 compression library
  Copyright (C) 2005-2020 Ariya Hidayat <ariya.hidayat@gmail.com>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef FASTLZ_H
#define FASTLZ_H

#define FASTLZ_VERSION 0x000500

#define FASTLZ_VERSION_MAJOR 0
#define FASTLZ_VERSION_MINOR 5
#define FASTLZ_VERSION_REVISION 0

#define FASTLZ_ERROR_CORRUPT -1
#define FASTLZ_ERROR_TOO_SMALL -2
#define FASTLZ_ERROR_UNKNOWN_LEVEL -3

#define FASTLZ_VERSION_STRING "0.5.0"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Compress data
 * @details
 * Compress a block of data in the input buffer and returns the size of
 * compressed block. The size of input buffer is specified by length. The
 * minimum input buffer size is 16.
 *
 * The output buffer must be at least 5% larger than the input buffer
 * and can not be smaller than 66 bytes.
 *
 * If the input is not compressible, the return value might be larger than
 * length (input buffer size).
 *
 * The input buffer and the output buffer can not overlap.
 *
 * Compression level can be specified in parameter level. At the moment,
 * only level 1 and level 2 are supported.
 * Level 1 is the fastest compression and generally useful for short data.
 * Level 2 is slightly slower but it gives better compression ratio. If
 * any other level is specified, #FASTLZ_ERROR_UNKNOWN_LEVEL is returned.
 *
 * Note that the compressed data, regardless of the level, can always be
 * decompressed using the function fastlz_decompress below.
 *
 * @param level compression level. 1 or 2.
 * @param input data to compress
 * @param length length of input
 * @param output receives compressed data
 * @retval 0 compressed okay
 * @retval #FASTLZ_ERROR_CORRUPT data could not be encoded
 * @retval #FASTLZ_ERROR_TOO_SMALL input or output buffer is too small
 * @retval #FASTLZ_ERROR_UNKNOWN_LEVEL level not one or two
*/

int fastlz_compress_level(int level, const void* input, int length, void* output);

/**

 */

/**
 * @brief Decompress data
 * @details
 * Decompress a block of compressed data and returns the size of the
 * decompressed block. If error occurs, e.g. the compressed data is
 * corrupted or the output buffer is not large enough, then a non-zero
 * error code is returned.
 *
 * The input buffer and the output buffer can not overlap.
 *
 * Decompression is memory safe and guaranteed not to write the output buffer
 * more than what is specified in maxout.
 *
 * Note that the decompression will always work, regardless of the
 * compression level specified in fastlz_compress_level above (when
 * producing the compressed block).
 *
 * @param input data to decompress
 * @param length length of input in bytes
 * @param output receives decompressed data
 * @param maxout size of output in bytes
 * @retval 0 success
 * @retval #FASTLZ_ERROR_CORRUPT input is corrupt
 * @retval #FASTLZ_ERROR_TOO_SMALL input or output buffer is too small
 * @retval #FASTLZ_ERROR_UNKNOWN_LEVEL not a known compression level
 */
int fastlz_decompress(const void* input, int length, void* output, int maxout);

#if defined(__cplusplus)
}
#endif

#endif /* FASTLZ_H */
