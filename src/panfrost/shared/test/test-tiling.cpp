/*
 * Copyright (C) 2022 Collabora, Ltd.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "pan_tiling.h"

#include <gtest/gtest.h>

/*
 * Reference tiling algorithm. This should be obviously correct.  Test cases
 * compare the optimized production version with this reference.
 *
 * 16x16 u-interleaving arranges bits within 16x16 tiles as:
 *
 * | y3 | (x3 ^ y3) | y2 | (y2 ^ x2) | y1 | (y1 ^ x1) | y0 | (y0 ^ x0) |
 *
 * 16x16 tiles themelves are in raster order.
 */
static unsigned
u_order_4(unsigned x, unsigned y)
{
   assert(x < 4 && y < 4);

   unsigned xy0 = ((x ^ y) & 1) ? 1 : 0;
   unsigned xy1 = ((x ^ y) & 2) ? 1 : 0;

   unsigned y0 = (y & 1) ? 1 : 0;
   unsigned y1 = (y & 2) ? 1 : 0;

   return (xy0 << 0) | (y0 << 1) | (xy1 << 2) | (y1 << 3);
}

static unsigned
u_order_16(unsigned x, unsigned y)
{
   assert(x < 16 && y < 16);

   unsigned xy0 = ((x ^ y) & 1) ? 1 : 0;
   unsigned xy1 = ((x ^ y) & 2) ? 1 : 0;
   unsigned xy2 = ((x ^ y) & 4) ? 1 : 0;
   unsigned xy3 = ((x ^ y) & 8) ? 1 : 0;

   unsigned y0 = (y & 1) ? 1 : 0;
   unsigned y1 = (y & 2) ? 1 : 0;
   unsigned y2 = (y & 4) ? 1 : 0;
   unsigned y3 = (y & 8) ? 1 : 0;

   return (xy0 << 0) | (y0 << 1) | (xy1 << 2) | (y1 << 3) |
          (xy2 << 4) | (y2 << 5) | (xy3 << 6) | (y3 << 7);
}

static unsigned
tiled_offset(unsigned x, unsigned y, unsigned stride, const struct util_format_description *desc)
{
   assert(desc->block.width == desc->block.height);
   assert((desc->block.width == 1) || (desc->block.height == 4));

   if (desc->block.width == 1) {
      unsigned tile_x = x / 16;
      unsigned tile_y = y / 16;

      unsigned x_in_tile = x % 16;
      unsigned y_in_tile = y % 16;

      unsigned index_in_tile = u_order_16(x_in_tile, y_in_tile);

      return (tile_y * (stride * 16)) + (((tile_x * 16 * 16) + index_in_tile) * (desc->block.bits / 8));
   } else {
      unsigned tile_x = x / 4;
      unsigned tile_y = y / 4;

      unsigned x_in_tile = x % 4;
      unsigned y_in_tile = y % 4;

      unsigned index_in_tile = u_order_4(x_in_tile, y_in_tile);

      return (tile_y * (stride * 4)) + ((tile_x * 4 * 4) + index_in_tile) * (desc->block.bits / 8);
   }
}

/* Arguments in blocks */
static unsigned
linear_offset(unsigned x, unsigned y, unsigned stride, const struct util_format_description *desc)
{
   return (stride * y) + (x * (desc->block.bits / 8));
}

static void
ref_access_tiled(void *dst, const void *src,
                 unsigned region_x, unsigned region_y,
                 unsigned w, unsigned h,
                 uint32_t dst_stride,
                 uint32_t src_stride,
                 enum pipe_format format,
                 bool dst_is_tiled)
{
   const struct util_format_description *desc = util_format_description(format);;

   unsigned w_block = w / desc->block.width;
   unsigned h_block = h / desc->block.height;

   unsigned region_x_block = region_x / desc->block.width;
   unsigned region_y_block = region_y / desc->block.height;

   for (unsigned src_y_block = 0; src_y_block < h_block; ++src_y_block) {
      for (unsigned src_x_block = 0; src_x_block < w_block; ++src_x_block) {

         unsigned dst_x_block = region_x_block + src_x_block;
         unsigned dst_y_block = region_y_block + src_y_block;

         unsigned dst_offset, src_offset;

         if (dst_is_tiled) {
            dst_offset = tiled_offset(dst_x_block, dst_y_block, dst_stride, desc);
            src_offset = linear_offset(src_x_block, src_y_block, src_stride, desc);
         } else {
            dst_offset = linear_offset(dst_x_block, dst_y_block, dst_stride, desc);
            src_offset = tiled_offset(src_x_block, src_y_block, src_stride, desc);
         }

         memcpy((uint8_t *) dst + dst_offset,
                (const uint8_t *) src + src_offset,
                desc->block.bits / 8);
      }
   }
}

class UInterleavedTiling : public testing::Test {
protected:
   UInterleavedTiling() {
   }
};

static void
test_store(unsigned width, unsigned height, unsigned rx, unsigned ry,
           unsigned rw, unsigned rh, unsigned linear_stride,
           enum pipe_format format)
{
   unsigned bpp = util_format_get_blocksize(format);

   unsigned tiled_width  = ALIGN_POT(width, 16);
   unsigned tiled_height = ALIGN_POT(height, 16);
   unsigned tiled_stride = tiled_width * bpp;

   void *tiled = calloc(bpp, tiled_width * tiled_height);
   void *ref = calloc(bpp, tiled_width * tiled_height);
   void *linear = calloc(bpp, rw * rh);

   for (unsigned i = 0; i < bpp * rw * rh; ++i) {
      ((uint8_t *) linear)[i] = (i & 0xFF);
   }

   panfrost_store_tiled_image(tiled, linear, rx, ry, rw, rh,
                              tiled_stride, linear_stride, format);

   ref_access_tiled(ref, linear, rx, ry, rw, rh,
                    tiled_stride, linear_stride, format, true);

   ASSERT_EQ(memcmp(ref, tiled, bpp * tiled_width * tiled_height), 0);

   free(ref);
   free(tiled);
   free(linear);
}

TEST_F(UInterleavedTiling, AllSizes)
{
   /* 8-bit */
   test_store(23, 17, 0, 0, 23, 17, 17 * 4, PIPE_FORMAT_R8_UINT);

   /* 16-bit */
   test_store(23, 17, 0, 0, 23, 17, 17 * 4, PIPE_FORMAT_R8G8_UINT);

   /* 24-bit */
   test_store(23, 17, 0, 0, 23, 17, 17 * 4, PIPE_FORMAT_R8G8B8_UINT);

   /* 32-bit */
   test_store(23, 17, 0, 0, 23, 17, 17 * 4, PIPE_FORMAT_R32_UINT);

   /* 48-bit */
   test_store(23, 17, 0, 0, 23, 17, 17 * 4, PIPE_FORMAT_R16G16B16_UINT);

   /* 64-bit */
   test_store(23, 17, 0, 0, 23, 17, 17 * 4, PIPE_FORMAT_R32G32_UINT);

   /* 96-bit */
   test_store(23, 17, 0, 0, 23, 17, 17 * 4, PIPE_FORMAT_R32G32B32_UINT);

   /* 128-bit */
   test_store(23, 17, 0, 0, 23, 17, 17 * 4, PIPE_FORMAT_R32G32B32A32_UINT);
}
