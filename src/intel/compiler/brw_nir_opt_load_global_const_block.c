/*
 * Copyright © 2022 Intel Corporation
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

#include "brw_nir.h"
#include "compiler/nir/nir_builder.h"

/**
 * This trims block constant loads. Those are usually vec8 or vec16 loads. If
 * we're not using either the top or bottom components, we can trim the load
 * and drop an unused vec8.
 */

static void
reswizzle_alu_uses(nir_ssa_def *def, uint8_t *reswizzle)
{
   nir_foreach_use(use_src, def) {
      /* all uses must be ALU instructions */
      assert(use_src->parent_instr->type == nir_instr_type_alu);
      nir_alu_src *alu_src = (nir_alu_src*)use_src;

      /* reswizzle ALU sources */
      for (unsigned i = 0; i < NIR_MAX_VEC_COMPONENTS; i++)
         alu_src->swizzle[i] = reswizzle[alu_src->swizzle[i]];
   }
}

static bool
is_only_used_by_alu(nir_ssa_def *def)
{
   nir_foreach_use(use_src, def) {
      if (use_src->parent_instr->type != nir_instr_type_alu)
         return false;
   }

   return true;
}

static bool
brw_nir_opt_load_global_const_block_instr(nir_builder *b,
                                          nir_instr *instr,
                                          UNUSED void *cb_data)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   if (intrin->intrinsic != nir_intrinsic_load_global_const_block_intel)
      return false;

   b->cursor = nir_before_instr(instr);

   nir_ssa_def *def = &intrin->dest.ssa;
   assert(def->bit_size == 32);
   unsigned load_size_b = def->num_components * 4;

   /* The minimum load size for this intrinsic is 32bytes. */
   if (load_size_b == 32)
      return false;

   /* don't remove any channels if used by non-ALU */
   if (!is_only_used_by_alu(def))
      return false;

   unsigned mask = nir_ssa_def_components_read(def);

   /* If nothing was read, leave it up to DCE. */
   if (!mask)
      return false;

   bool progress = false;
   uint8_t reswizzle[NIR_MAX_VEC_COMPONENTS] = { 0 };
   for (unsigned c = 0; c < def->num_components; c++)
      reswizzle[c] = c;

   /* Trim the top components */
   assert(def->num_components >= 8);
   const unsigned top_mask = 0xff << (def->num_components - 8);
   if ((mask & top_mask) == 0) {
      def->num_components -= 8;
      intrin->num_components -= 8;
      progress = true;
   }

   /* Trim the bottom components */
   if ((mask & 0xff) == 0) {
      for (unsigned c = 8; c < def->num_components; c++)
         reswizzle[c] = c - 8;

      nir_ssa_def *addr = intrin->src[0].ssa;
      nir_instr_rewrite_src_ssa(instr,
                                &intrin->src[0],
                                nir_iadd_imm(b, addr, 32));
      def->num_components -= 8;
      intrin->num_components -= 8;

      mask >>= 8;
      progress = true;
   }

   if (progress)
      reswizzle_alu_uses(def, reswizzle);

   return progress;

}

bool
brw_nir_opt_load_global_const_block(nir_shader *shader)
{
   return nir_shader_instructions_pass(shader,
                                       brw_nir_opt_load_global_const_block_instr,
                                       nir_metadata_block_index |
                                       nir_metadata_dominance,
                                       NULL);
}
