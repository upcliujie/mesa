/*
 * Copyright 2020 Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "image_compare.h"
#include "macros.h"
#include <string.h>
#include <stdint.h>
#include <immintrin.h>

typedef bool (*pixels_equal_func_t)(unsigned width, unsigned height,
                                    unsigned stride, const void *ref,
                                    const void *src);

#define DEFINE_PIXELS_EQUAL_FUNC(TYPE, ALIGNMENT, PIXEL_SIZE)              \
static ATTRIBUTE_NOINLINE bool                                             \
pixels_equal_align##ALIGNMENT##_size##PIXEL_SIZE(unsigned width,           \
                                 unsigned height, unsigned stride,         \
                                 const void *ref, const TYPE *src)         \
{                                                                          \
   TYPE value[(PIXEL_SIZE) / sizeof(TYPE)];                                \
   memcpy(value, ref, (PIXEL_SIZE));                                       \
   unsigned stride_in_elems = stride / sizeof(TYPE);                       \
   unsigned elems_per_pixel = (PIXEL_SIZE) / sizeof(TYPE);                 \
   unsigned elems_per_row = width * elems_per_pixel;                       \
   for (unsigned y = 0; y < height; y++) {                                 \
      const TYPE *row_src = src + stride_in_elems * y;                     \
      for (unsigned x = 0; x < elems_per_row; x += elems_per_pixel) {      \
         if (memcmp(row_src + x, value, (PIXEL_SIZE)))                     \
            return false;                                                  \
      }                                                                    \
   }                                                                       \
   return true;                                                            \
}

DEFINE_PIXELS_EQUAL_FUNC(uint8_t, 1, 1)
DEFINE_PIXELS_EQUAL_FUNC(uint8_t, 1, 2)
DEFINE_PIXELS_EQUAL_FUNC(uint8_t, 1, 3)
DEFINE_PIXELS_EQUAL_FUNC(uint8_t, 1, 4)
DEFINE_PIXELS_EQUAL_FUNC(uint16_t, 2, 2)
DEFINE_PIXELS_EQUAL_FUNC(uint16_t, 2, 4)
DEFINE_PIXELS_EQUAL_FUNC(uint16_t, 2, 6)
DEFINE_PIXELS_EQUAL_FUNC(uint16_t, 2, 8)
DEFINE_PIXELS_EQUAL_FUNC(uint32_t, 4, 4)
DEFINE_PIXELS_EQUAL_FUNC(uint32_t, 4, 8)
DEFINE_PIXELS_EQUAL_FUNC(uint32_t, 4, 12)
DEFINE_PIXELS_EQUAL_FUNC(uint32_t, 4, 16)
DEFINE_PIXELS_EQUAL_FUNC(uint64_t, 8, 8)
DEFINE_PIXELS_EQUAL_FUNC(uint64_t, 8, 16)
DEFINE_PIXELS_EQUAL_FUNC(__m128, 16, 16)

static const pixels_equal_func_t pixels_equal_func_table[17][17] = {
   [1] = {
      [1] = (pixels_equal_func_t)pixels_equal_align1_size1,
      [2] = (pixels_equal_func_t)pixels_equal_align1_size2,
      [3] = (pixels_equal_func_t)pixels_equal_align1_size3,
      [4] = (pixels_equal_func_t)pixels_equal_align1_size4,
   },
   [2] = {
      [2] = (pixels_equal_func_t)pixels_equal_align2_size2,
      [4] = (pixels_equal_func_t)pixels_equal_align2_size4,
      [6] = (pixels_equal_func_t)pixels_equal_align2_size6,
      [8] = (pixels_equal_func_t)pixels_equal_align2_size8,
   },
   [4] = {
      [4] = (pixels_equal_func_t)pixels_equal_align4_size4,
      [8] = (pixels_equal_func_t)pixels_equal_align4_size8,
      [12] = (pixels_equal_func_t)pixels_equal_align4_size12,
      [16] = (pixels_equal_func_t)pixels_equal_align4_size16,
   },
   [8] = {
      [8] = (pixels_equal_func_t)pixels_equal_align8_size8,
      [16] = (pixels_equal_func_t)pixels_equal_align8_size16,
   },
   [16] = {
      [16] = (pixels_equal_func_t)pixels_equal_align16_size16,
   },
};

/** Return true if all src pixels are equal to the reference value. */
bool
util_pixels_equal_to_ref(unsigned width, unsigned height, unsigned pixel_size,
                         unsigned stride, const void *ref, const void *src)
{
   unsigned row_size = width * pixel_size;
   unsigned alignment;

   /* Determine the image alignment. */
   if ((uintptr_t)src % 16 == 0 && stride % 16 == 0 && row_size % 16 == 0)
      alignment = 16;
   else if ((uintptr_t)src % 8 == 0 && stride % 8 == 0 && row_size % 8 == 0)
      alignment = 8;
   else if ((uintptr_t)src % 4 == 0 && stride % 4 == 0 && row_size % 4 == 0)
      alignment = 4;
   else if ((uintptr_t)src % 2 == 0 && stride % 2 == 0 && row_size % 2 == 0)
      alignment = 2;
   else
      alignment = 1;

   /* Upgrade the pixel size if we have better alignment. */
   char new_ref[16];
   if (alignment > pixel_size && alignment % pixel_size == 0) {
      /* Duplicate the pixel into a bigger pixel. */
      for (unsigned offset = 0; offset < alignment; offset += pixel_size)
         memcpy(new_ref + offset, ref, pixel_size);

      unsigned div = alignment / pixel_size;
      stride /= div;
      width /= div;

      pixel_size = alignment;
      ref = new_ref;
   } else {
      /* Reduce the assumed alignment if we don't have a comparison
       * function for it.
       */
      while (alignment > pixel_size || pixel_size % alignment)
         alignment /= 2;
   }

   assert(alignment <= pixel_size);
   assert(pixel_size <= alignment * 4);
   assert(pixel_size % alignment == 0);

   if (pixel_size <= 16) {
      pixels_equal_func_t func = pixels_equal_func_table[alignment][pixel_size];

      if (func)
         return func(width, height, stride, ref, src);
   }

   /* unreachable implies undefined behavior, but we want it defined here */
   assert(!"invalid pixel size or address not aligned to element size\n");
   return false;
}

/** Return true if all src pixels are equal. */
bool
util_pixels_equal(unsigned width, unsigned height, unsigned pixel_size,
                  unsigned stride, const void *src)
{
   return util_pixels_equal_to_ref(width, height, pixel_size, stride,
                                   src, src);
}
