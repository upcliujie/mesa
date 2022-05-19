/**************************************************************************
 *
 * Copyright (C) 2022 Collabora Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "nir_builder.h"

/* This lowering replaces access to non-existing images with a no-op and
 * return zero if the image access is supposed to return a value.
 */


/* If we only handle constant offsets we already know that
 * the buffer or atomic is out of range, so we can just return
 * zero */
static nir_ssa_def *
nir_lower_memop_oob_access_const_offset(nir_builder *b,
                                        nir_intrinsic_instr *intr)
{
   switch (intr->intrinsic) {
   case nir_intrinsic_image_store:
   case nir_intrinsic_store_ssbo:
      return NIR_LOWER_INSTR_PROGRESS_REPLACE;
   default:
      return nir_imm_zero(b, nir_dest_num_components(intr->dest),
                          nir_dest_bit_size(intr->dest));
   }
}


static nir_ssa_def *
nir_lower_memop_oob_access_all(nir_builder *b, nir_intrinsic_instr *intr)
{
   bool returns_value = true;
   nir_ssa_def *location_exists = NULL;

   switch (intr->intrinsic) {
   case nir_intrinsic_image_store:
      returns_value = false;
      FALLTHROUGH;
   case nir_intrinsic_image_load:
   case nir_intrinsic_image_atomic_add:
   case nir_intrinsic_image_atomic_and:
   case nir_intrinsic_image_atomic_or:
   case nir_intrinsic_image_atomic_xor:
   case nir_intrinsic_image_atomic_exchange:
   case nir_intrinsic_image_atomic_comp_swap:
   case nir_intrinsic_image_atomic_umin:
   case nir_intrinsic_image_atomic_umax:
   case nir_intrinsic_image_atomic_imin:
   case nir_intrinsic_image_atomic_imax:
   case nir_intrinsic_image_size:
      location_exists = nir_ult(b, intr->src[0].ssa,
            nir_imm_int(b, b->shader->info.num_images));
      break;
   case nir_intrinsic_atomic_counter_read:
   case nir_intrinsic_atomic_counter_add:
   case nir_intrinsic_atomic_counter_and:
   case nir_intrinsic_atomic_counter_or:
   case nir_intrinsic_atomic_counter_xor:
   case nir_intrinsic_atomic_counter_exchange:
   case nir_intrinsic_atomic_counter_comp_swap:
   case nir_intrinsic_atomic_counter_min:
   case nir_intrinsic_atomic_counter_max:
   case nir_intrinsic_global_atomic_fmin_amd:
      location_exists = nir_ige(b, intr->src[0].ssa, nir_imm_int(b, 0));
      break;
   case nir_intrinsic_store_ssbo:
      returns_value = false;
      location_exists = nir_ult(b, intr->src[1].ssa,
            nir_imm_int(b, b->shader->info.num_ssbos));
      break;
   case nir_intrinsic_load_ssbo:
   case nir_intrinsic_ssbo_atomic_add:
   case nir_intrinsic_ssbo_atomic_and:
   case nir_intrinsic_ssbo_atomic_or:
   case nir_intrinsic_ssbo_atomic_xor:
   case nir_intrinsic_ssbo_atomic_exchange:
   case nir_intrinsic_ssbo_atomic_comp_swap:
   case nir_intrinsic_ssbo_atomic_umin:
   case nir_intrinsic_ssbo_atomic_umax:
   case nir_intrinsic_ssbo_atomic_imin:
   case nir_intrinsic_ssbo_atomic_imax:
   case nir_intrinsic_get_ssbo_size:
      location_exists = nir_ult(b, intr->src[0].ssa,
            nir_imm_int(b, b->shader->info.num_ssbos));
      break;
   default:
      ;
   }

   nir_ssa_def *result = nir_lower_memop_oob_access_const_offset(b, intr);

   /* If the memory location exists, re-emit the original instruction,
    * otherwise return the default result */
   nir_if *if_exists = nir_push_if(b, location_exists);
   nir_instr *mem_instr = nir_instr_clone(b->shader, &intr->instr);
   nir_builder_instr_insert(b, mem_instr);
   nir_if *else_exists = nir_push_else(b, if_exists);
   nir_pop_if(b, else_exists);

   if (returns_value) {
      nir_intrinsic_instr *image_intr = nir_instr_as_intrinsic(mem_instr);
      result = nir_if_phi(b, &image_intr->dest.ssa, result);
      b->cursor = nir_after_instr(result->parent_instr);
   } else {
      b->cursor = nir_after_cf_node(&else_exists->cf_node);
   }
   return result;
}

static nir_ssa_def *
nir_lower_memop_oob_access_impl(nir_builder *b, nir_instr *instr, void *_options)
{
   b->cursor = nir_before_instr(instr);
   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

   bool only_handle_const_offsets = *(bool *)_options;
   if (only_handle_const_offsets)
      return nir_lower_memop_oob_access_const_offset(b, intr);
   else
      return nir_lower_memop_oob_access_all(b, intr);
}

static bool
nir_lower_memop_oob_access_filter(const nir_instr *instr, const void *_options)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   bool only_handle_const_offsets = *(bool *)_options;

   int sidx = 0;

   nir_intrinsic_instr *ir = nir_instr_as_intrinsic(instr);
   switch (ir->intrinsic) {
   case nir_intrinsic_store_ssbo:
      sidx = 1;
      FALLTHROUGH;
   case nir_intrinsic_image_store:
   case nir_intrinsic_image_load:
   case nir_intrinsic_image_atomic_add:
   case nir_intrinsic_image_atomic_and:
   case nir_intrinsic_image_atomic_or:
   case nir_intrinsic_image_atomic_xor:
   case nir_intrinsic_image_atomic_exchange:
   case nir_intrinsic_image_atomic_comp_swap:
   case nir_intrinsic_image_atomic_umin:
   case nir_intrinsic_image_atomic_umax:
   case nir_intrinsic_image_atomic_imin:
   case nir_intrinsic_image_atomic_imax:
   case nir_intrinsic_image_size:

   case nir_intrinsic_atomic_counter_read:
   case nir_intrinsic_atomic_counter_add:
   case nir_intrinsic_atomic_counter_and:
   case nir_intrinsic_atomic_counter_or:
   case nir_intrinsic_atomic_counter_xor:
   case nir_intrinsic_atomic_counter_exchange:
   case nir_intrinsic_atomic_counter_comp_swap:
   case nir_intrinsic_atomic_counter_min:
   case nir_intrinsic_atomic_counter_max:
   case nir_intrinsic_global_atomic_fmin_amd:

   case nir_intrinsic_load_ssbo:
   case nir_intrinsic_ssbo_atomic_add:
   case nir_intrinsic_ssbo_atomic_and:
   case nir_intrinsic_ssbo_atomic_or:
   case nir_intrinsic_ssbo_atomic_xor:
   case nir_intrinsic_ssbo_atomic_exchange:
   case nir_intrinsic_ssbo_atomic_comp_swap:
   case nir_intrinsic_ssbo_atomic_umin:
   case nir_intrinsic_ssbo_atomic_umax:
   case nir_intrinsic_ssbo_atomic_imin:
   case nir_intrinsic_ssbo_atomic_imax:
      if (only_handle_const_offsets) {
         nir_const_value *offset = nir_src_as_const_value(ir->src[sidx]);
         return offset != NULL ? offset->i32 < 0 : false;
      } else {
         return true;
      }
   default:
      return false;
   }
}

bool nir_lower_memop_oob_access(nir_shader *sh, bool only_const_offsets)
{
   return nir_shader_lower_instructions(sh,
                                        nir_lower_memop_oob_access_filter,
                                        nir_lower_memop_oob_access_impl,
                                        &only_const_offsets);
}
