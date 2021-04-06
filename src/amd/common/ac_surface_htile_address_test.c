/*
 * Copyright © 2021 Advanced Micro Devices, Inc.
 * Copyright © 2021 Valve Corporation
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS, AUTHORS
 * AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */

/* Make the test not meaningless when asserts are disabled. */
#undef NDEBUG

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <amdgpu.h>
#include "drm-uapi/amdgpu_drm.h"
#include "drm-uapi/drm_fourcc.h"

#include "ac_surface.h"
#include "util/macros.h"
#include "util/u_atomic.h"
#include "util/u_math.h"
#include "util/u_vector.h"
#include "util/mesa-sha1.h"
#include "addrlib/inc/addrinterface.h"

#include "ac_surface_test_common.h"

/*
 * The main goal of this test is to validate that our HTILE addressing
 * functions match addrlib behavior.
 */

/* HTILE address computation without mipmapping and MSAA. */
static unsigned gfx10_htile_addr_from_coord(const struct radeon_info *info,
                                            const uint16_t *equation,
                                            unsigned meta_block_width,
                                            unsigned meta_block_height,
                                            unsigned pitch, unsigned slice_size,
                                            unsigned x, unsigned y, unsigned z,
                                            unsigned pipe_xor)
{
   unsigned meta_block_width_log2 = util_logbase2(meta_block_width);
   unsigned meta_block_height_log2 = util_logbase2(meta_block_height);
   unsigned blkSizeLog2 = meta_block_width_log2 + meta_block_height_log2 - 4;

   unsigned coord[] = {x, y, z, 0};
   unsigned address = 0;

   for (unsigned i = 0; i < blkSizeLog2 + 1; i++) {
      unsigned v = 0;

      for (unsigned c = 0; c < 4; c++) {
         if (equation[i*4+c] != 0) {
            unsigned mask = equation[i*4+c];
            unsigned bits = coord[c];

            while (mask)
               v ^= (bits >> u_bit_scan(&mask)) & 0x1;
         }
      }

      address |= v << i;
   }

   unsigned blkMask = (1 << blkSizeLog2) - 1;
   unsigned pipeMask = (1 << G_0098F8_NUM_PIPES(info->gb_addr_config)) - 1;
   unsigned m_pipeInterleaveLog2 = 8 + G_0098F8_PIPE_INTERLEAVE_SIZE_GFX9(info->gb_addr_config);
   unsigned xb = x >> meta_block_width_log2;
   unsigned yb = y >> meta_block_height_log2;
   unsigned pb = pitch >> meta_block_width_log2;
   unsigned blkIndex = (yb * pb) + xb;
   unsigned pipeXor = ((pipe_xor & pipeMask) << m_pipeInterleaveLog2) & blkMask;

   return (slice_size * z) +
          (blkIndex * (1 << blkSizeLog2)) +
          ((address >> 1) ^ pipeXor);
}

static bool one_htile_address_test(const char *name, const char *test, ADDR_HANDLE addrlib,
                                   const struct radeon_info *info,
                                   unsigned width, unsigned height, unsigned depth,
                                   unsigned bpp, unsigned swizzle_mode,
                                   unsigned start_x, unsigned start_y, unsigned start_z)
{
   ADDR2_COMPUTE_PIPEBANKXOR_INPUT xin = {0};
   ADDR2_COMPUTE_PIPEBANKXOR_OUTPUT xout = {0};
   ADDR2_COMPUTE_HTILE_INFO_INPUT hin = {0};
   ADDR2_COMPUTE_HTILE_INFO_OUTPUT hout = {0};
   ADDR2_COMPUTE_HTILE_ADDRFROMCOORD_INPUT in = {0};
   ADDR2_COMPUTE_HTILE_ADDRFROMCOORD_OUTPUT out = {0};
   ADDR2_META_MIP_INFO meta_mip_info[RADEON_SURF_MAX_LEVELS] = {0};

   hout.pMipInfo = meta_mip_info;

   /* Compute HTILE info. */
   hin.hTileFlags.pipeAligned = 1;
   hin.hTileFlags.rbAligned = 1;
   hin.depthFlags.depth = 1;
   hin.depthFlags.texture = 1;
   hin.depthFlags.opt4space = 1;
   hin.swizzleMode = in.swizzleMode = xin.swizzleMode = swizzle_mode;
   hin.unalignedWidth = in.unalignedWidth = width;
   hin.unalignedHeight = in.unalignedHeight = height;
   hin.numSlices = in.numSlices = depth;
   hin.numMipLevels = in.numMipLevels = 1; /* addrlib can't do HtileAddrFromCoord with mipmapping. */
   hin.firstMipIdInTail = 1;

   int ret = Addr2ComputeHtileInfo(addrlib, &hin, &hout);
   assert(ret == ADDR_OK);

   /* Compute xor. */
   static AddrFormat format[] = {
      ADDR_FMT_16,
      ADDR_FMT_32,
   };
   xin.flags = hin.depthFlags;
   xin.resourceType = ADDR_RSRC_TEX_2D;
   xin.format = format[util_logbase2(bpp / 8)];
   xin.numFrags = xin.numSamples = in.numSamples = 1;

   ret = Addr2ComputePipeBankXor(addrlib, &xin, &xout);
   assert(ret == ADDR_OK);

   in.hTileFlags = hin.hTileFlags;
   in.depthflags = xin.flags;
   in.bpp = bpp;
   in.pipeXor = xout.pipeBankXor;

   for (in.x = start_x; in.x < width; in.x++) {
      for (in.y = start_y; in.y < height; in.y++) {
         for (in.slice = start_z; in.slice < depth; in.slice++) {
            int r = Addr2ComputeHtileAddrFromCoord(addrlib, &in, &out);
            if (r != ADDR_OK) {
               printf("%s addrlib error: %s\n", name, test);
               abort();
            }

            unsigned addr =
               gfx10_htile_addr_from_coord(info, hout.equation.gfx10_bits,
                                           hout.metaBlkWidth, hout.metaBlkHeight,
                                           hout.pitch, hout.sliceSize,
                                           in.x, in.y, in.slice, in.pipeXor);
            if (out.addr != addr) {
               printf("%s fail (%s) at %ux%ux%u: expected = %llu, got = %u\n",
                      name, test, in.x, in.y, in.slice, out.addr, addr);
               return false;
            }
         }
      }
   }

   return true;
}

static void run_htile_address_test(const char *name, const struct radeon_info *info, bool full)
{
   unsigned total = 0;
   unsigned fails = 0;
   unsigned first_size = 0, last_size = 6*6 - 1, max_bpp = 32;

   /* The test coverage is reduced for Gitlab CI because it timeouts. */
   if (!full) {
      first_size = last_size = 0;
   }

#ifdef HAVE_OPENMP
#pragma omp parallel for
#endif
   for (unsigned size = first_size; size <= last_size; size++) {
      unsigned width = 8 + 379 * (size % 6);
      unsigned height = 8 + 379 * (size / 6);

      struct ac_addrlib *ac_addrlib = ac_addrlib_create(info, NULL);
      ADDR_HANDLE addrlib = ac_addrlib_get_handle(ac_addrlib);

      for (unsigned depth = 1; depth <= 2; depth *= 2) {
         for (unsigned bpp = 16; bpp <= max_bpp; bpp *= 2) {
            if (one_htile_address_test(name, name, addrlib, info, width, height, depth,
                                       bpp, ADDR_SW_64KB_Z_X, 0, 0, 0)) {
            } else {
               p_atomic_inc(&fails);
            }
            p_atomic_inc(&total);
         }
      }

      ac_addrlib_destroy(ac_addrlib);
   }
   printf("%16s total: %u, fail: %u\n", name, total, fails);
}

int main(int argc, char **argv)
{
   bool full = false;

   if (argc == 2 && !strcmp(argv[1], "--full"))
      full = true;
   else
      puts("Specify --full to run the full test.");

   for (unsigned i = 0; i < ARRAY_SIZE(testcases); ++i) {
      struct radeon_info info = get_radeon_info(&testcases[i]);

      /* Only GFX10+ is currently supported. */
      if (info.chip_class < GFX10)
         continue;

      run_htile_address_test(testcases[i].name, &info, full);
   }

   return 0;
}
