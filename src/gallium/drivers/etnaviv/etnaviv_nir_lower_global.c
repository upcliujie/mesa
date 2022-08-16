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

#include "nir.h"
#include "nir_builder.h"
#include "etnaviv_nir.h"

static bool
lower_global(nir_builder *b, nir_instr *instr, UNUSED void *unused)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
   switch (intr->intrinsic) {
   case nir_intrinsic_load_global:
   case nir_intrinsic_store_global:
      break;
   default:
      return false;
   }

   b->cursor = nir_before_instr(instr);

   nir_src *addr_src = nir_get_io_offset_src(intr);
   assert(addr_src->is_ssa);
   nir_ssa_scalar addr = nir_get_ssa_scalar(addr_src->ssa, 0);
   addr = nir_ssa_scalar_chase_movs(addr);

   nir_ssa_scalar addr_base, addr_off;
   if (nir_ssa_scalar_is_alu(addr) &&
       nir_ssa_scalar_alu_op(addr) == nir_op_iadd) {
      addr_base = nir_ssa_scalar_chase_alu_src(addr, 0);
      addr_off = nir_ssa_scalar_chase_alu_src(addr, 1);
   } else {
      addr_base = addr;
      addr_off = nir_get_ssa_scalar(nir_imm_int(b, 0), 0);
   }

   if (intr->intrinsic == nir_intrinsic_store_global) {
      assert(nir_intrinsic_src_components(intr, 1) == 1);

      unsigned num_comp = nir_intrinsic_src_components(intr, 0);

      nir_ssa_def *value = nir_ssa_for_src(b, intr->src[0], num_comp);
      nir_ssa_def *v = nir_channels(b, value, BITFIELD_MASK(num_comp));
      nir_build_store_global_etna(b, v,
                                  nir_ssa_for_scalar(b, addr_base),
                                  nir_ssa_for_scalar(b, addr_off));
   } else {
      assert(nir_intrinsic_src_components(intr, 0) == 1);

      unsigned num_comp = nir_dest_num_components(intr->dest);
      unsigned bitsize = nir_dest_bit_size(intr->dest);

      nir_ssa_def *load =
         nir_build_load_global_etna(b, num_comp, bitsize,
                                    nir_ssa_for_scalar(b, addr_base),
                                    nir_ssa_for_scalar(b, addr_off));
      nir_ssa_def_rewrite_uses(&intr->dest.ssa, load);
   }

   nir_instr_remove(instr);

   return true;
}

bool
etna_nir_lower_global(nir_shader *shader)
{
   return nir_shader_instructions_pass(shader, lower_global,
                                       nir_metadata_block_index |
                                       nir_metadata_dominance, NULL);
}
