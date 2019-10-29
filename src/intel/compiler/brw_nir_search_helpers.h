/*
 * Copyright © 2019 Intel Corporation
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

#ifndef BRW_NIR_SEARCH_HELPERS
#define BRW_NIR_SEARCH_HELPERS

#include "nir.h"
#include "util/bitscan.h"
#include "nir_range_analysis.h"

static inline bool
front_face(UNUSED struct hash_table *ht, nir_alu_instr *instr, unsigned src,
           UNUSED unsigned num_components, UNUSED const uint8_t *swizzle)
{
   if (!instr->src[src].src.is_ssa ||
       instr->src[src].src.ssa->parent_instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *const src0 =
      nir_instr_as_intrinsic(instr->src[src].src.ssa->parent_instr);

   return src0->intrinsic == nir_intrinsic_load_front_face;
}

static inline bool
no_src_mod(UNUSED struct hash_table *ht, nir_alu_instr *instr, unsigned src,
           UNUSED unsigned num_components, UNUSED const uint8_t *swizzle)
{
   return !instr->src[src].abs && !instr->src[src].negate;
}

static inline bool
any_src_mod(UNUSED struct hash_table *ht, nir_alu_instr *instr, unsigned src,
            UNUSED unsigned num_components, UNUSED const uint8_t *swizzle)
{
   return instr->src[src].abs || instr->src[src].negate;
}

static inline bool
abs_src_mod(UNUSED struct hash_table *ht, nir_alu_instr *instr, unsigned src,
            UNUSED unsigned num_components, UNUSED const uint8_t *swizzle)
{
   return instr->src[src].abs && !instr->src[src].negate;
}

static inline bool
neg_abs_src_mod(UNUSED struct hash_table *ht, nir_alu_instr *instr, unsigned src,
                UNUSED unsigned num_components, UNUSED const uint8_t *swizzle)
{
   return instr->src[src].abs && instr->src[src].negate;
}

static inline bool
neg_src_mod(UNUSED struct hash_table *ht, nir_alu_instr *instr, unsigned src,
            UNUSED unsigned num_components, UNUSED const uint8_t *swizzle)
{
   return instr->src[src].negate;
}

static inline bool
is_not_const_and_no_int_src_mod(struct hash_table *ht, nir_alu_instr *instr,
                                unsigned src, unsigned num_components,
                                const uint8_t *swizzle)
{
   if (nir_src_is_const(instr->src[src].src))
      return false;

   if (any_src_mod(ht, instr, src, num_components, swizzle)) {
      const nir_alu_type type = (nir_alu_type)
         nir_alu_type_get_base_type(nir_op_infos[instr->op].input_types[src]);

      assert(type != nir_type_invalid);
      return type == nir_type_float;
   }

   return true;
}

#endif /* BRW_NIR_SEARCH_HELPERS */
