/*
 * Copyright © 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#ifndef GEN_SAMPLE_POSITIONS_H
#define GEN_SAMPLE_POSITIONS_H

#include <util/macros.h>

/*
 * This file defines the standard multisample positions used by both GL and
 * Vulkan.  These correspond to the Vulkan "standard sample locations".
 */

extern const float gen_sample_positions_1x[];
extern const float gen_sample_positions_2x[];
extern const float gen_sample_positions_4x[];
extern const float gen_sample_positions_8x[];
extern const float gen_sample_positions_16x[];

static inline const float *
gen_get_sample_positions(int samples)
{
   switch (samples) {
   case 1: return gen_sample_positions_1x;
   case 2: return gen_sample_positions_2x;
   case 4: return gen_sample_positions_4x;
   case 8: return gen_sample_positions_8x;
   case 16: return gen_sample_positions_16x;
   default: unreachable("Invalid sample count");
   }
}

/**
 * 1x MSAA has a single sample at the center: (0.5, 0.5) -> (0x8, 0x8).
 */
#define GEN_SAMPLE_POS_1X(prefix) \
prefix##0XOffset   = gen_sample_positions_1x[0]; \
prefix##0YOffset   = gen_sample_positions_1x[1];

/**
 * 2x MSAA sample positions are (0.25, 0.25) and (0.75, 0.75):
 *   4 c
 * 4 0
 * c   1
 */
#define GEN_SAMPLE_POS_2X(prefix) \
prefix##0XOffset   = gen_sample_positions_2x[0]; \
prefix##0YOffset   = gen_sample_positions_2x[1]; \
prefix##1XOffset   = gen_sample_positions_2x[2]; \
prefix##1YOffset   = gen_sample_positions_2x[3];

/**
 * Sample positions:
 *   2 6 a e
 * 2   0
 * 6       1
 * a 2
 * e     3
 */
#define GEN_SAMPLE_POS_4X(prefix) \
prefix##0XOffset   = gen_sample_positions_4x[0]; \
prefix##0YOffset   = gen_sample_positions_4x[1]; \
prefix##1XOffset   = gen_sample_positions_4x[2]; \
prefix##1YOffset   = gen_sample_positions_4x[3]; \
prefix##2XOffset   = gen_sample_positions_4x[4]; \
prefix##2YOffset   = gen_sample_positions_4x[5]; \
prefix##3XOffset   = gen_sample_positions_4x[6]; \
prefix##3YOffset   = gen_sample_positions_4x[7];

/**
 * Sample positions:
 *
 * From the Ivy Bridge PRM, Vol2 Part1 p304 (3DSTATE_MULTISAMPLE:
 * Programming Notes):
 *     "When programming the sample offsets (for NUMSAMPLES_4 or _8 and
 *     MSRASTMODE_xxx_PATTERN), the order of the samples 0 to 3 (or 7
 *     for 8X) must have monotonically increasing distance from the
 *     pixel center. This is required to get the correct centroid
 *     computation in the device."
 *
 * Sample positions:
 *   1 3 5 7 9 b d f
 * 1               7
 * 3     3
 * 5         0
 * 7 5
 * 9             2
 * b       1
 * d   4
 * f           6
 */
#define GEN_SAMPLE_POS_8X(prefix) \
prefix##0XOffset   = gen_sample_positions_8x[0]; \
prefix##0YOffset   = gen_sample_positions_8x[1]; \
prefix##1XOffset   = gen_sample_positions_8x[2]; \
prefix##1YOffset   = gen_sample_positions_8x[3]; \
prefix##2XOffset   = gen_sample_positions_8x[4]; \
prefix##2YOffset   = gen_sample_positions_8x[5]; \
prefix##3XOffset   = gen_sample_positions_8x[6]; \
prefix##3YOffset   = gen_sample_positions_8x[7]; \
prefix##4XOffset   = gen_sample_positions_8x[8]; \
prefix##4YOffset   = gen_sample_positions_8x[9]; \
prefix##5XOffset   = gen_sample_positions_8x[10]; \
prefix##5YOffset   = gen_sample_positions_8x[11]; \
prefix##6XOffset   = gen_sample_positions_8x[12]; \
prefix##6YOffset   = gen_sample_positions_8x[13]; \
prefix##7XOffset   = gen_sample_positions_8x[14]; \
prefix##7YOffset   = gen_sample_positions_8x[15];

/**
 * Sample positions:
 *
 *    0 1 2 3 4 5 6 7 8 9 a b c d e f
 * 0   15
 * 1                  9
 * 2         10
 * 3                        7
 * 4                               13
 * 5                1
 * 6        4
 * 7                          3
 * 8 12
 * 9                    0
 * a            2
 * b                            6
 * c     11
 * d                      5
 * e              8
 * f                             14
 */
#define GEN_SAMPLE_POS_16X(prefix) \
prefix##0XOffset   = gen_sample_positions_16x[0]; \
prefix##0YOffset   = gen_sample_positions_16x[1]; \
prefix##1XOffset   = gen_sample_positions_16x[2]; \
prefix##1YOffset   = gen_sample_positions_16x[3]; \
prefix##2XOffset   = gen_sample_positions_16x[4]; \
prefix##2YOffset   = gen_sample_positions_16x[5]; \
prefix##3XOffset   = gen_sample_positions_16x[6]; \
prefix##3YOffset   = gen_sample_positions_16x[7]; \
prefix##4XOffset   = gen_sample_positions_16x[8]; \
prefix##4YOffset   = gen_sample_positions_16x[9]; \
prefix##5XOffset   = gen_sample_positions_16x[10]; \
prefix##5YOffset   = gen_sample_positions_16x[11]; \
prefix##6XOffset   = gen_sample_positions_16x[12]; \
prefix##6YOffset   = gen_sample_positions_16x[13]; \
prefix##7XOffset   = gen_sample_positions_16x[14]; \
prefix##7YOffset   = gen_sample_positions_16x[15]; \
prefix##8XOffset   = gen_sample_positions_16x[16]; \
prefix##8YOffset   = gen_sample_positions_16x[17]; \
prefix##9XOffset   = gen_sample_positions_16x[18]; \
prefix##9YOffset   = gen_sample_positions_16x[19]; \
prefix##10XOffset  = gen_sample_positions_16x[20]; \
prefix##10YOffset  = gen_sample_positions_16x[21]; \
prefix##11XOffset  = gen_sample_positions_16x[22]; \
prefix##11YOffset  = gen_sample_positions_16x[23]; \
prefix##12XOffset  = gen_sample_positions_16x[24]; \
prefix##12YOffset  = gen_sample_positions_16x[25]; \
prefix##13XOffset  = gen_sample_positions_16x[26]; \
prefix##13YOffset  = gen_sample_positions_16x[27]; \
prefix##14XOffset  = gen_sample_positions_16x[28]; \
prefix##14YOffset  = gen_sample_positions_16x[29]; \
prefix##15XOffset  = gen_sample_positions_16x[30]; \
prefix##15YOffset  = gen_sample_positions_16x[31];

/* Examples:
 * in case of GEN_GEN < 8:
 * GEN_SAMPLE_POS_ELEM(ms.Sample, info->pSampleLocations, 0); expands to:
 *    ms.Sample0XOffset = info->pSampleLocations[0].pos.x;
 *    ms.Sample0YOffset = info->pSampleLocations[0].y;
 *
 * in case of GEN_GEN >= 8:
 * GEN_SAMPLE_POS_ELEM(sp._16xSample, info->pSampleLocations, 0); expands to:
 *    sp._16xSample0XOffset = info->pSampleLocations[0].x;
 *    sp._16xSample0YOffset = info->pSampleLocations[0].y;
 */

#define GEN_SAMPLE_POS_ELEM(prefix, arr, sample_idx) \
prefix##sample_idx##XOffset = arr[sample_idx].x; \
prefix##sample_idx##YOffset = arr[sample_idx].y;

#define GEN_SAMPLE_POS_1X_ARRAY(prefix, arr)\
GEN_SAMPLE_POS_ELEM(prefix, arr, 0);

#define GEN_SAMPLE_POS_2X_ARRAY(prefix, arr) \
GEN_SAMPLE_POS_ELEM(prefix, arr, 0); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 1);

#define GEN_SAMPLE_POS_4X_ARRAY(prefix, arr) \
GEN_SAMPLE_POS_ELEM(prefix, arr, 0); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 1); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 2); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 3);

#define GEN_SAMPLE_POS_8X_ARRAY(prefix, arr) \
GEN_SAMPLE_POS_ELEM(prefix, arr, 0); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 1); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 2); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 3); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 4); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 5); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 6); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 7);

#define GEN_SAMPLE_POS_16X_ARRAY(prefix, arr) \
GEN_SAMPLE_POS_ELEM(prefix, arr, 0); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 1); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 2); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 3); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 4); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 5); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 6); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 7); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 8); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 9); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 10); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 11); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 12); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 13); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 14); \
GEN_SAMPLE_POS_ELEM(prefix, arr, 15);

#endif /* GEN_SAMPLE_POSITIONS_H */
