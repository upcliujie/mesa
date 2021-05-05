/**************************************************************************
 *
 * Copyright 2012-2021 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 **************************************************************************/

#include "dxbc.h"
#include <memory.h>


/*
 * XXX: This is valid only for little endian CPUs.
 */
#define GETDWORD(DW, B) ((DW) = *(unsigned int *)&(B))
#define SETDWORD(B, DW) (*(unsigned int *)&(B) = (DW))

struct DXBCChecksum {
   unsigned int total;
   unsigned int state[4];
};

static void
ChecksumInit(struct DXBCChecksum *cs)
{
   cs->total = 0;
   cs->state[0] = 0x67452301;
   cs->state[1] = 0xefcdab89;
   cs->state[2] = 0x98badcfe;
   cs->state[3] = 0x10325476;
}

#define ROTATE(X, N) ((X << N) | (X >> (32 - N)))

#define PROCESS(A, B, C, D, K, S, T, F) A += F(B, C, D) + x[K] + T; A = ROTATE(A, S) + B

#define FUN0(X, Y, Z) (Z ^ (X & (Y ^ Z)))
#define FUN1(X, Y, Z) (Y ^ (Z & (X ^ Y)))
#define FUN2(X, Y, Z) (X ^ Y ^ Z)
#define FUN3(X, Y, Z) (Y ^ (X | ~Z))

static void
Hash(struct DXBCChecksum *cs,
     const unsigned char data[64])
{
   unsigned int i;
   unsigned int x[16];
   unsigned int a, b, c, d;

   for (i = 0; i < 16; i++) {
      GETDWORD(x[i], data[i * 4]);
   }
   a = cs->state[0];
   b = cs->state[1];
   c = cs->state[2];
   d = cs->state[3];

   PROCESS(a, b, c, d, 0,  7,  0xd76aa478, FUN0);
   PROCESS(d, a, b, c, 1,  12, 0xe8c7b756, FUN0);
   PROCESS(c, d, a, b, 2,  17, 0x242070db, FUN0);
   PROCESS(b, c, d, a, 3,  22, 0xc1bdceee, FUN0);
   PROCESS(a, b, c, d, 4,  7,  0xf57c0faf, FUN0);
   PROCESS(d, a, b, c, 5,  12, 0x4787c62a, FUN0);
   PROCESS(c, d, a, b, 6,  17, 0xa8304613, FUN0);
   PROCESS(b, c, d, a, 7,  22, 0xfd469501, FUN0);
   PROCESS(a, b, c, d, 8,  7,  0x698098d8, FUN0);
   PROCESS(d, a, b, c, 9,  12, 0x8b44f7af, FUN0);
   PROCESS(c, d, a, b, 10, 17, 0xffff5bb1, FUN0);
   PROCESS(b, c, d, a, 11, 22, 0x895cd7be, FUN0);
   PROCESS(a, b, c, d, 12, 7,  0x6b901122, FUN0);
   PROCESS(d, a, b, c, 13, 12, 0xfd987193, FUN0);
   PROCESS(c, d, a, b, 14, 17, 0xa679438e, FUN0);
   PROCESS(b, c, d, a, 15, 22, 0x49b40821, FUN0);

   PROCESS(a, b, c, d, 1,  5,  0xf61e2562, FUN1);
   PROCESS(d, a, b, c, 6,  9,  0xc040b340, FUN1);
   PROCESS(c, d, a, b, 11, 14, 0x265e5a51, FUN1);
   PROCESS(b, c, d, a, 0,  20, 0xe9b6c7aa, FUN1);
   PROCESS(a, b, c, d, 5,  5,  0xd62f105d, FUN1);
   PROCESS(d, a, b, c, 10, 9,  0x02441453, FUN1);
   PROCESS(c, d, a, b, 15, 14, 0xd8a1e681, FUN1);
   PROCESS(b, c, d, a, 4,  20, 0xe7d3fbc8, FUN1);
   PROCESS(a, b, c, d, 9,  5,  0x21e1cde6, FUN1);
   PROCESS(d, a, b, c, 14, 9,  0xc33707d6, FUN1);
   PROCESS(c, d, a, b, 3,  14, 0xf4d50d87, FUN1);
   PROCESS(b, c, d, a, 8,  20, 0x455a14ed, FUN1);
   PROCESS(a, b, c, d, 13, 5,  0xa9e3e905, FUN1);
   PROCESS(d, a, b, c, 2,  9,  0xfcefa3f8, FUN1);
   PROCESS(c, d, a, b, 7,  14, 0x676f02d9, FUN1);
   PROCESS(b, c, d, a, 12, 20, 0x8d2a4c8a, FUN1);

   PROCESS(a, b, c, d, 5,  4,  0xfffa3942, FUN2);
   PROCESS(d, a, b, c, 8,  11, 0x8771f681, FUN2);
   PROCESS(c, d, a, b, 11, 16, 0x6d9d6122, FUN2);
   PROCESS(b, c, d, a, 14, 23, 0xfde5380c, FUN2);
   PROCESS(a, b, c, d, 1,  4,  0xa4beea44, FUN2);
   PROCESS(d, a, b, c, 4,  11, 0x4bdecfa9, FUN2);
   PROCESS(c, d, a, b, 7,  16, 0xf6bb4b60, FUN2);
   PROCESS(b, c, d, a, 10, 23, 0xbebfbc70, FUN2);
   PROCESS(a, b, c, d, 13, 4,  0x289b7ec6, FUN2);
   PROCESS(d, a, b, c, 0,  11, 0xeaa127fa, FUN2);
   PROCESS(c, d, a, b, 3,  16, 0xd4ef3085, FUN2);
   PROCESS(b, c, d, a, 6,  23, 0x04881d05, FUN2);
   PROCESS(a, b, c, d, 9,  4,  0xd9d4d039, FUN2);
   PROCESS(d, a, b, c, 12, 11, 0xe6db99e5, FUN2);
   PROCESS(c, d, a, b, 15, 16, 0x1fa27cf8, FUN2);
   PROCESS(b, c, d, a, 2,  23, 0xc4ac5665, FUN2);

   PROCESS(a, b, c, d, 0,  6,  0xf4292244, FUN3);
   PROCESS(d, a, b, c, 7,  10, 0x432aff97, FUN3);
   PROCESS(c, d, a, b, 14, 15, 0xab9423a7, FUN3);
   PROCESS(b, c, d, a, 5,  21, 0xfc93a039, FUN3);
   PROCESS(a, b, c, d, 12, 6,  0x655b59c3, FUN3);
   PROCESS(d, a, b, c, 3,  10, 0x8f0ccc92, FUN3);
   PROCESS(c, d, a, b, 10, 15, 0xffeff47d, FUN3);
   PROCESS(b, c, d, a, 1,  21, 0x85845dd1, FUN3);
   PROCESS(a, b, c, d, 8,  6,  0x6fa87e4f, FUN3);
   PROCESS(d, a, b, c, 15, 10, 0xfe2ce6e0, FUN3);
   PROCESS(c, d, a, b, 6,  15, 0xa3014314, FUN3);
   PROCESS(b, c, d, a, 13, 21, 0x4e0811a1, FUN3);
   PROCESS(a, b, c, d, 4,  6,  0xf7537e82, FUN3);
   PROCESS(d, a, b, c, 11, 10, 0xbd3af235, FUN3);
   PROCESS(c, d, a, b, 2,  15, 0x2ad7d2bb, FUN3);
   PROCESS(b, c, d, a, 9,  21, 0xeb86d391, FUN3);

   cs->state[0] += a;
   cs->state[1] += b;
   cs->state[2] += c;
   cs->state[3] += d;
}

void
DXBC_Checksum(const unsigned char *data,
              unsigned int dataSize,
              unsigned char checksum[16])
{
   struct DXBCChecksum cs;
   unsigned int bitLen, evilLen, pad;
   unsigned char buffer[128];

   ChecksumInit(&cs);

   bitLen = dataSize * 8;
   evilLen = (dataSize * 2) | 1;

   while (dataSize >= 64) {
      Hash(&cs, data);
      dataSize -= 64;
      data += 64;
   }

   if (dataSize < 56) {
      memcpy(buffer, &bitLen, 4);

      if (dataSize) {
         memcpy(&buffer[4], data, dataSize);
      }

      pad = 56 - dataSize;
      buffer[4 + dataSize] = 0x80;
      if (pad > 1) {
         memset(&buffer[4 + dataSize + 1], 0, pad - 1);
      }

      memcpy(&buffer[4 + dataSize + pad], &evilLen, 4);

      Hash(&cs, buffer);
   } else {
      memcpy(buffer, data, dataSize);

      pad = 64 - dataSize;
      buffer[dataSize] = 0x80;
      if (pad > 1) {
         memset(&buffer[dataSize + 1], 0, pad - 1);
      }

      memcpy(&buffer[64], &bitLen, 4);

      memset(&buffer[68], 0, 56);

      memcpy(&buffer[124], &evilLen, 4);

      Hash(&cs, buffer);
      Hash(&cs, &buffer[64]);
   }

   SETDWORD(checksum[0], cs.state[0]);
   SETDWORD(checksum[4], cs.state[1]);
   SETDWORD(checksum[8], cs.state[2]);
   SETDWORD(checksum[12], cs.state[3]);
}
