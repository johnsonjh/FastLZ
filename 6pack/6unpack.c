/* SPDX-License-Identifier: MIT */

/*
 * 6PACK - file compressor using FastLZ (lightning-fast compression library)
 *
 * Copyright (c) 2007-2020 Ariya Hidayat <ariya.hidayat@gmail.com>
 * Copyright (c) 2023 Jeffrey H. Johnson <trnsz@pobox.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 *  * The above copyright notice and this permission notice shall be
 *    included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIXPACK_VERSION_MAJOR      0
#define SIXPACK_VERSION_MINOR      1
#define SIXPACK_VERSION_REVISION   0
#define SIXPACK_VERSION_STRING     "0.1.0"

#undef FREE
#ifdef TESTING
# define FREE(p) free(p)
#else
# define FREE(p) do  \
  {                  \
    free((p));       \
    (p) = NULL;      \
  } while(0)
#endif /* ifdef TESTING */

#include "fastlz.h"

/* Magic identifier for 6pack file */
static unsigned char sixpack_magic[8] = {
  137, '6', 'P', 'K', 13, 10, 26, 10
};

#ifndef BLOCK_SIZE
# define BLOCK_SIZE  65536
#endif /* ifndef BLOCK_SIZE */

#if ( BLOCK_SIZE > 2621440 )
# error BLOCK_SIZE too large ( > 2621440 )
#endif /* if ( BLOCK_SIZE > 2621440 */

#if ( BLOCK_SIZE < 256 )
# error BLOCK_SIZE too small ( < 256 )
#endif /* if ( BLOCK_SIZE < 256 */

/* Prototypes */
static unsigned long update_adler32(unsigned long checksum, const void *buf,
                                    int len);
void usage(void);
int detect_magic(FILE *f);
static unsigned long readU16(const unsigned char *ptr);
static unsigned long readU32(const unsigned char *ptr);
void read_chunk_header(FILE *f, int *id, int *options, unsigned long *size,
                       unsigned long *checksum, unsigned long *extra);
int unpack_file(const char *archive_file);

/* Adler-32 checksum algorithm; see RFC-1950, Section 8.2 */
#define ADLER32_BASE  65521
static unsigned long
update_adler32(unsigned long checksum, const void *buf, int len)
{
  const unsigned char * ptr  = (const unsigned char *)buf;
  unsigned long         s1   = checksum & 0xffff;
  unsigned long         s2   = ( checksum >> 16 ) & 0xffff;

  while (len > 0)
    {
      unsigned k = len < 5552 ? len : 5552;
      len -= k;

      while (k >= 8)
        {
          s1  += *ptr++;
          s2  += s1;
          s1  += *ptr++;
          s2  += s1;
          s1  += *ptr++;
          s2  += s1;
          s1  += *ptr++;
          s2  += s1;
          s1  += *ptr++;
          s2  += s1;
          s1  += *ptr++;
          s2  += s1;
          s1  += *ptr++;
          s2  += s1;
          s1  += *ptr++;
          s2  += s1;
          k   -= 8;
        }

      while (k-- > 0)
        {
          s1  += *ptr++;
          s2  += s1;
        }
      s1  = s1 % ADLER32_BASE;
      s2  = s2 % ADLER32_BASE;
    }
  return ( s2 << 16 ) + s1;
}

void
usage(void)
{
  printf("6unpack: uncompress 6pack archive\n");
  printf("Copyright (C) Ariya Hidayat\n");
  printf("\n");
  printf("Usage: 6unpack archive-file\n");
  printf("\n");
}

/*
 * Return non-zero if magic sequence is detected.
 * Warning: Reset the read pointer to the beginning of the file
 */

int
detect_magic(FILE *f)
{
  unsigned char  buffer[8];
  size_t         bytes_read;
  unsigned int   c;

  fseek(f, SEEK_SET, 0);
  bytes_read = fread(buffer, 1, 8, f);
  fseek(f, SEEK_SET, 0);
  if (bytes_read < 8)
    {
      return 0;
    }

  for (c = 0; c < 8; c++)
    {
      if (buffer[c] != sixpack_magic[c])
        {
          return 0;
        }
    }

  return -1;
}

static unsigned long
readU16(const unsigned char *ptr)
{
  return ptr[0] + ( ptr[1] << 8 );
}

static unsigned long
readU32(const unsigned char *ptr)
{
  return ptr[0] + ( ptr[1] << 8 ) + ( ptr[2] << 16 ) + ( ptr[3] << 24 );
}

void
read_chunk_header(FILE *f, int *id, int *options, unsigned long *size,
                  unsigned long *checksum, unsigned long *extra)
{
  unsigned char buffer[16];

  fread(buffer, 1, 16, f);

  *id        = readU16(buffer)      & 0xffff;
  *options   = readU16(buffer + 2)  & 0xffff;
  *size      = readU32(buffer + 4)  & 0xffffffff;
  *checksum  = readU32(buffer + 8)  & 0xffffffff;
  *extra     = readU32(buffer + 12) & 0xffffffff;
}

int
unpack_file(const char *input_file)
{
  FILE *          in;
  unsigned long   fsize;
  int             c;
  unsigned long   percent;
  unsigned char   progress[20];
  int             chunk_id;
  int             chunk_options;
  unsigned long   chunk_size;
  unsigned long   chunk_checksum;
  unsigned long   chunk_extra;
  unsigned char   buffer[BLOCK_SIZE];
  unsigned long   checksum;

  unsigned long   decompressed_size;
  unsigned long   total_extracted;
  int             name_length;
  char *          output_file;
  FILE *          f;

  unsigned char * compressed_buffer;
  unsigned char * decompressed_buffer;
  unsigned long   compressed_bufsize;
  unsigned long   decompressed_bufsize;

  /* Sanity check */
  in = fopen(input_file, "rb");
  if (!in)
    {
      printf("Error: could not open %s\n", input_file);
      return -1;
    }

  /* Find size of the file */
  fseek(in, 0, SEEK_END);
  fsize = ftell(in);
  fseek(in, 0, SEEK_SET);

  /* Not a 6pack archive? */
  if (!detect_magic(in))
    {
      fclose(in);
      printf("Error: file %s is not a 6pack archive!\n", input_file);
      return -1;
    }

  printf("Archive: %s", input_file);

  /* Position of first chunk */
  fseek(in, 8, SEEK_SET);

  /* Initialize */
  output_file           = 0;
  f                     = 0;
  total_extracted       = 0;
  decompressed_size     = 0;
  percent               = 0;
  compressed_buffer     = 0;
  decompressed_buffer   = 0;
  compressed_bufsize    = 0;
  decompressed_bufsize  = 0;

  /* Main loop */
  for (;;)
    {
      /* End of file? */
      size_t pos = ftell(in);
      if (pos >= fsize)
        {
          break;
        }

      read_chunk_header(
        in,
        &chunk_id,
        &chunk_options,
        &chunk_size,
        &chunk_checksum,
        &chunk_extra);

      if (( chunk_id == 1 ) && ( chunk_size > 10 ) && ( chunk_size < BLOCK_SIZE ))
        {
          /* Close current file, if any */
          printf("\n");
          if (output_file)
            FREE(output_file);

          if (f)
            {
              fclose(f);
            }

          /* File entry */
          fread(buffer, 1, chunk_size, in);
          checksum = update_adler32(1L, buffer, chunk_size);
          if (checksum != chunk_checksum)
            {
              fclose(in);
              printf("\nError: checksum mismatch!\n");
              printf("Got %08lX Expecting %08lX\n", checksum, chunk_checksum);
              return -1;
            }

          decompressed_size  = readU32(buffer);
          total_extracted    = 0;
          percent            = 0;

          /* Get file to extract */
          name_length = (int)readU16(buffer + 8);
          if (name_length > (int)chunk_size - 10)
            {
              name_length = chunk_size - 10;
            }

          output_file = (char *)malloc(name_length + 1);
          if (!output_file)
            {
              printf("Out of memory. Aborting!\n");
              abort();
            }

          memset(output_file, 0, name_length + 1);
          for (c = 0; c < name_length; c++)
            {
              output_file[c] = buffer[10 + c];
            }

          /* Check if already exists */
          f = fopen(output_file, "rb");
          if (f)
            {
              fclose(f);
              printf("File %s already exists. Skipped.\n", output_file);
              FREE(output_file);
              f = 0;
            }
          else
            {
              /* Create the file */
              f = fopen(output_file, "wb");
              if (!f)
                {
                  printf("Can't create file %s. Skipped.\n", output_file);
                  FREE(output_file);
                }
              else
                {
                  /* For progress status */
                  printf("\n");
                  memset(progress, ' ', 20);
                  if (strlen(output_file) < 16)
                    {
                      for (c = 0; c < (int)strlen(output_file); c++)
                        {
                          progress[c] = output_file[c];
                        }
                    }
                  else
                    {
                      for (c = 0; c < 13; c++)
                        {
                          progress[c] = output_file[c];
                        }

                      progress[13]  = '.';
                      progress[14]  = '.';
                      progress[15]  = ' ';
                    }

                  progress[16]  = '[';
                  progress[17]  = 0;
                  printf("%s", progress);
                  for (c = 0; c < 50; c++)
                    {
                      printf(".");
                    }

                  printf("]\r");
                  printf("%s", progress);
                }
            }
        }

      if (( chunk_id == 17 ) && f && output_file && decompressed_size)
        {
          unsigned long remaining;

          /* Uncompressed */
          switch (chunk_options)
            {
            /* Stored, simply copy to output */
            case 0:
              /* Read one block at at time, write and update checksum */
              total_extracted  += chunk_size;
              remaining        = chunk_size;
              checksum         = 1L;
              for (;;)
                {
                  unsigned long  r
                    = ( BLOCK_SIZE < remaining ) ? BLOCK_SIZE : remaining;
                  size_t         bytes_read = fread(buffer, 1, r, in);
                  if (bytes_read == 0)
                    {
                      break;
                    }

                  fwrite(buffer, 1, bytes_read, f);
                  checksum   = update_adler32(checksum, buffer, bytes_read);
                  remaining  -= bytes_read;
                }

              /* Verify everything is written correctly */
              if (checksum != chunk_checksum)
                {
                  fclose(f);
                  f = 0;
                  FREE(output_file);
                  printf("\nError: checksum mismatch. Aborted.\n");
                  printf(
                    "Got %08lX Expecting %08lX\n",
                    checksum,
                    chunk_checksum);
                }

              break;

            /* Compressed using FastLZ */
            case 1:
              /* Enlarge input buffer if necessary */
              if (chunk_size > compressed_bufsize)
                {
                  compressed_bufsize = chunk_size;
                  if (compressed_buffer)
                    FREE(compressed_buffer);
                  compressed_buffer
                    = (unsigned char *)malloc(compressed_bufsize);
                }

              /* Enlarge output buffer if necessary */
              if (chunk_extra > decompressed_bufsize)
                {
                  decompressed_bufsize = chunk_extra;
                  if (decompressed_buffer)
                    FREE(decompressed_buffer);
                  decompressed_buffer
                    = (unsigned char *)malloc(decompressed_bufsize);
                }

              /* Check compressed_buffer */
              if (!compressed_buffer)
                {
                  printf("\nError: No compressed buffer! Aborting!\n");
                  abort();
                }

              /* Read and check checksum */
              fread(compressed_buffer, 1, chunk_size, in);
              checksum         = update_adler32(1L, compressed_buffer, chunk_size);
              total_extracted  += chunk_extra;

              /* Verify that the chunk data is correct */
              if (checksum != chunk_checksum)
                {
                  fclose(f);
                  f = 0;
                  FREE(output_file);
                  printf("\nError: checksum mismatch. Skipped.\n");
                  printf(
                    "Got %08lX Expecting %08lX\n",
                    checksum,
                    chunk_checksum);
                }
              else
                {
                  /* Decompress and verify */
                  remaining
                    = fastlz_decompress(
                        compressed_buffer,
                        chunk_size,
                        decompressed_buffer,
                        chunk_extra);
                  if (remaining != chunk_extra)
                    {
                      fclose(f);
                      f = 0;
                      FREE(output_file);
                      printf("\nError: decompression failed. Skipped.\n");
                    }
                  else
                    {
                      /* Check compressed_buffer */
                      if (!decompressed_buffer)
                        {
                          printf("\nError: No decompressed buffer! Aborting!\n");
                          abort();
                        }
                      fwrite(decompressed_buffer, 1, chunk_extra, f);
                    }
                }

              break;

            default:
              printf(
                "\nError: unknown compression method (%d)\n",
                chunk_options);
              fclose(f);
              f = 0;
              FREE(output_file);
              break;
            }

          /* For progress, if everything is fine */
          if (f)
            {
              int last_percent = (int)percent;
              if (decompressed_size < ( 1 << 24 ))
                {
                  percent = total_extracted * 100 / decompressed_size;
                }
              else
                {
                  percent
                    = total_extracted / 256 * 100 / ( decompressed_size >> 8 );
                }

              percent >>= 1;
              while (last_percent < (int)percent)
                {
                  printf("#");
                  last_percent++;
                }
            }
        }

      /* Position of next chunk */
      fseek(in, pos + 16 + chunk_size, SEEK_SET);
    }

  printf("\n\n");

  /* Free allocated stuff */
  FREE(compressed_buffer);
  FREE(decompressed_buffer);
  FREE(output_file);

  /* Close working files */
  if (f)
    {
      fclose(f);
    }

  fclose(in);

  /* So far so good */
  return 0;
}

int
main(int argc, char **argv)
{
  int          i;
  const char * archive_file;

  /* Show help with no argument at all */
  if (argc == 1)
    {
      usage();
      return 0;
    }

  /* Check for help on usage */
  for (i = 1; i <= argc; i++)
    {
      if (argv[i])
        {
          if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
            {
              usage();
              return 0;
            }
        }
    }

  /* Check for version information */
  for (i = 1; i <= argc; i++)
    {
      if (argv[i])
        {
          if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version"))
            {
              printf("6unpack: high-speed file compression tool\n");
              printf(
                "Version %s (using FastLZ %s)\n",
                SIXPACK_VERSION_STRING,
                FASTLZ_VERSION_STRING);
              printf("Copyright (C) Ariya Hidayat\n");
              printf("\n");
              return 0;
            }
        }
    }

  /* Needs at least two arguments */
  if (argc <= 1)
    {
      usage();
      return 0;
    }

  archive_file = argv[1];

  return unpack_file(archive_file);
}
