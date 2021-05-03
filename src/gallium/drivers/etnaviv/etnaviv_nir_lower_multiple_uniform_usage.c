/*
 * Copyright (c) 2020 Etnaviv Project
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

/*
 * The hardware does not allow two or more different uniform registers to be used as
 * sources in the same ALU instruction. Emit mov instructions to registers for all
 * but one uniform register in this case.
 */

struct state {
   int bitmask; // invalid uniform usage mask
};

static int
invalid_uniform_usage(const nir_alu_instr *alu)
{
   const nir_op_info *info = &nir_op_infos[alu->op];
   int invalid = 0;
   int first_uniform = -1;

   for (unsigned i = 0; i < info->num_inputs; i++) {
      nir_const_value *cv = nir_src_as_const_value(alu->src[i].src);

      if (!cv)
         continue;

      if (first_uniform == -1) {
         first_uniform = i;
         continue;
      }

      if (!nir_srcs_equal(alu->src[first_uniform].src, alu->src[i].src))
         invalid |= 1 << i;
   }

   return invalid;
}

static bool
has_multiple_uniforms(const nir_instr *instr, const void *_data)
{
   struct state *s = (struct state *)_data;
   memset(s, 0, sizeof(struct state));

   if (instr->type != nir_instr_type_alu)
      return false;

   nir_alu_instr *alu = nir_instr_as_alu(instr);

   if (nir_op_is_vec(alu->op))
      return false;

   s->bitmask = invalid_uniform_usage(alu);

   return !!s->bitmask;
}

static nir_ssa_def *
lower_multiple_uniform_usage(nir_builder *b, nir_instr *instr, void *_data)
{
   struct state *s = (struct state *)_data;
   nir_alu_instr *alu = nir_instr_as_alu(instr);

   b->cursor = nir_before_instr(&alu->instr);
   b->exact = alu->exact;

   assert(alu->dest.dest.is_ssa);
   assert(alu->dest.write_mask != 0);
   assert(s->bitmask);

   int mask = s->bitmask;

   while (mask) {
      const int i = ffs(mask) - 1;

      nir_ssa_def *mov = nir_mov(b, alu->src[i].src.ssa);
      nir_instr_rewrite_src(&alu->instr, &alu->src[i].src, nir_src_for_ssa(mov));

      mask &= ~(1 << i);
   }

   return NIR_LOWER_INSTR_PROGRESS;
}

bool
etna_nir_lower_multiple_uniform_usage(nir_shader *shader)
{
   struct state s;

   return nir_shader_lower_instructions(shader,
                                        has_multiple_uniforms,
                                        lower_multiple_uniform_usage,
                                        &s);
}
