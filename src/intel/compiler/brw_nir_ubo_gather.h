/*
 * Copyright © 2020 Intel Corporation
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

#ifndef BRW_NIR_GATHER_H
#define BRW_NIR_GATHER_H

#include "compiler/nir/nir.h"

#ifdef __cplusplus
extern "C" {
#endif

struct brw_compiler;

struct brw_ubo_gather {
   /** Binding table index for the gathered UBO */
   uint8_t block;

   uint8_t pad[3];

   /** Offset at which to start gathering data */
   uint32_t start;

   /** Bitset of which dwords (starting at start) should be included */
   uint32_t dwords;
};

#define BRW_NIR_GATHER_VS_ENTRY_SIZE 16

/** Packs the HW version of a brw_ubo_gather into a uvec4 */
static inline void
brw_nir_pack_gather_vs_entry(uint32_t *entry, uint64_t dst_addr_u64,
                             uint64_t src_addr_u64, uint32_t dwords)
{
   entry[0] = dst_addr_u64;
   entry[1] = (dwords << 16) | ((dst_addr_u64 >> 32) & 0x0000ffff);
   entry[2] = src_addr_u64;
   entry[3] = (dwords & 0xffff0000) | ((src_addr_u64 >> 32) & 0x0000ffff);
}

nir_shader *brw_nir_create_gather_vs(const struct brw_compiler *compiler,
                                     void *mem_ctx);

struct brw_ubo_gather *
brw_nir_gather_ubo_loads(nir_shader *nir, unsigned max_gather_size,
                         unsigned *gather_count, void *mem_ctx);

void brw_nir_lower_gathered_ubo_loads(nir_shader *nir,
                                      unsigned gather_start,
                                      unsigned gather_count,
                                      struct brw_ubo_gather *gathers);

#ifdef __cplusplus
}
#endif

#endif /* BRW_NIR_H */
