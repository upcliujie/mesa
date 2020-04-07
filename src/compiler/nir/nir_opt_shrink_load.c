/*
 * Copyright © 2018 Valve Corporation
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

#include "nir.h"
#include "nir_builder.h"

static nir_variable_mode
load_intrin_to_mode(nir_intrinsic_op op)
{
   switch (op) {
   case nir_intrinsic_load_ubo:
      return nir_var_mem_ubo;
   case nir_intrinsic_load_ssbo:
      return nir_var_mem_ssbo;
   case nir_intrinsic_load_shared:
      return nir_var_mem_shared;
   case nir_intrinsic_load_global:
      return nir_var_mem_global;
   case nir_intrinsic_load_push_constant:
      return nir_var_mem_push_const;
   default:
      return 0;
   }
}

static bool
opt_shrink_load_impl(nir_function_impl *impl, nir_variable_mode modes)
{
   bool progress = false;

   nir_builder b;
   nir_builder_init(&b, impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *load = nir_instr_as_intrinsic(instr);
         if (!(modes & load_intrin_to_mode(load->intrinsic)))
            continue;

         nir_component_mask_t read =
            nir_ssa_def_components_read(&load->dest.ssa);
         int first = ffs(read) - 1;
         int last = util_last_bit(read) - 1;
         if (first == 0 && last == load->dest.ssa.num_components - 1)
            continue;

         assert(last >= first);
         unsigned new_num_comps = last - first + 1;

         if (first > 0) {
            b.cursor = nir_before_instr(&load->instr);

            assert(load->dest.ssa.bit_size % 8 == 0);
            unsigned comp_size = load->dest.ssa.bit_size / 8;
            unsigned comp_offset = first * comp_size;

            /* load_push_constant doesn't have alignment information */
            if (load->intrinsic != nir_intrinsic_load_push_constant) {
               unsigned align_mul = nir_intrinsic_align_mul(load);
               unsigned align_offset = nir_intrinsic_align_offset(load);
               assert(align_mul >= comp_size);
               align_offset = (align_offset + comp_offset) % align_mul;
               nir_intrinsic_set_align(load, align_mul, align_offset);
            }

            nir_src *offset_src = nir_get_io_offset_src(load);
            assert(offset_src->is_ssa);
            nir_ssa_def *new_offset = nir_iadd_imm(&b, offset_src->ssa,
                                                       comp_offset);
            nir_instr_rewrite_src(&load->instr, offset_src,
                                  nir_src_for_ssa(new_offset));

            b.cursor = nir_after_instr(&load->instr);

            nir_ssa_def *comps[NIR_MAX_VEC_COMPONENTS];
            nir_ssa_def *undef = nir_ssa_undef(&b, 1, load->dest.ssa.bit_size);
            for (unsigned i = 0; i < load->num_components; i++) {
               if (read & (1 << i)) {
                  assert(i >= first);
                  assert(i - first < new_num_comps);
                  comps[i] = nir_channel(&b, &load->dest.ssa, i - first);
               } else {
                  comps[i] = undef;
               }
            }
            nir_ssa_def *vec = nir_vec(&b, comps, load->num_components);

            nir_ssa_def_rewrite_uses_after(&load->dest.ssa,
                                           nir_src_for_ssa(vec),
                                           vec->parent_instr);
         }

         load->num_components = new_num_comps;
         load->dest.ssa.num_components = new_num_comps;
         progress = true;
      }
   }

   if (progress) {
      nir_metadata_preserve(impl, nir_metadata_block_index |
                                  nir_metadata_dominance);
   } else {
#ifndef NDEBUG
      impl->valid_metadata &= ~nir_metadata_not_properly_reset;
#endif
   }

   return progress;
}

bool
nir_opt_shrink_load(nir_shader *shader, nir_variable_mode modes)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (!function->impl && opt_shrink_load_impl(function->impl, modes))
         progress = true;
   }

   return progress;
}
