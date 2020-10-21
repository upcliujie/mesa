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

#ifndef GEN_L3CONTROL_H
#define GEN_L3CONTROL_H

#include "util/bitscan.h"
#include "util/u_math.h"
#include "intel/common/gen_gem.h"

static inline void
gen_calculate_l3_address_and_mask(uint64_t addr, uint64_t size,
                                  uint64_t *base_addr, uint32_t *addr_mask)
{
   uint64_t address = gen_48b_address(addr);
   uint64_t start = ROUND_DOWN_TO(address, 4096);
   uint64_t end   = ALIGN(address + size, 4096) - 1;

   /* XOR to find where difference in address bits starts */
   uint64_t diff = start ^ end;

   /* index of the bit where difference starts */
   uint32_t diff_index = util_last_bit64(diff) - 1;

   /* bitmask with all '0' on bits starting from (diffIndex - 1) to 0 */
   uint64_t diff_mask= ~(((uint64_t)1 << diff_index) - 1);

   /* begin of the range masked with bitmask */
   *base_addr = start & diff_mask;

   /* Address mask can be computed by decremented diffIndex by 11
    * (alignment in bits - 1)
    */
   *addr_mask = diff_index - (12 - 1);
}

#endif
