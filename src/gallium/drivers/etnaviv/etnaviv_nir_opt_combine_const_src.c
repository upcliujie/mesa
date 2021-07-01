/*
 * Copyright (c) 2021 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#include "etnaviv_nir.h"

#include <util/bitscan.h>

/*
 * Pass to combine const alu sources to a single const src.
 */

struct state {
   unsigned const_bitmask;
   unsigned max_swizzle;
   nir_const_value value[4];
   uint8_t swizzle[4][4];
};

static int
const_add(uint64_t *c, uint64_t value)
{
   for (unsigned i = 0; i < 4; i++) {
      if (c[i] == value || !c[i]) {
         c[i] = value;
         return i;
      }
   }

   return -1;
}

static bool
alu_has_combinable_const_srcs(const nir_instr *instr, const void *_data)
{
   struct state *s = (struct state *)_data;
   memset(s, 0, sizeof(struct state));

   if (instr->type != nir_instr_type_alu)
      return false;

   nir_alu_instr *alu = nir_instr_as_alu(instr);
   const nir_op_info *info = &nir_op_infos[alu->op];

   /* leave vec ops untouched */
   if (nir_op_is_vec(alu->op))
      return false;

   for (unsigned i = 0; i < info->num_inputs; i++) {
      nir_alu_src *src = &alu->src[i];
      nir_const_value *cv = nir_src_as_const_value(src->src);

      if (!cv)
         continue;

      const unsigned int comps = nir_ssa_alu_instr_src_components(alu, i);

      for (unsigned j = 0; j < comps; j++) {
         int idx = const_add(&s->value[0].u64, cv[src->swizzle[j]].u64);
         s->swizzle[i][j] = idx;
         s->max_swizzle = MAX2(s->max_swizzle, (unsigned)idx);
      }

      s->const_bitmask |= (1U << i);
   }

   /* we need at least tow const sources */
   if (util_bitcount(s->const_bitmask) <= 1)
      return false;

   return (s->max_swizzle < 4);
}

static nir_ssa_def *
alu_combine_const_src(nir_builder *b, nir_instr *instr, void *_data)
{
   nir_alu_instr *alu = nir_instr_as_alu(instr);
   struct state *s = (struct state *)_data;

   b->cursor = nir_before_instr(&alu->instr);
   nir_ssa_def *def = nir_build_imm(b, s->max_swizzle + 1, 32, s->value);

   while (s->const_bitmask ) {
      const int i = ffs(s->const_bitmask ) - 1;

      nir_instr_rewrite_src(&alu->instr, &alu->src[i].src, nir_src_for_ssa(def));
      for (unsigned j = 0; j < 4; j++)
         alu->src[i].swizzle[j] = s->swizzle[i][j];

      s->const_bitmask &= ~(1 << i);
   }

   return NIR_LOWER_INSTR_PROGRESS;
}

bool
etna_nir_alu_combine_const_src(nir_shader *shader)
{
   struct state s;

   return nir_shader_lower_instructions(shader,
                                        alu_has_combinable_const_srcs,
                                        alu_combine_const_src,
                                        &s);

}
