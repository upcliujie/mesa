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
static nir_ssa_def *
nir_lower_image_oob_access_impl(nir_builder *b, nir_instr *instr, void *_options)
{
   nir_ssa_def *result = NIR_LOWER_INSTR_PROGRESS_REPLACE;
   nir_ssa_def *default_result = NULL;

   b->cursor = nir_before_instr(instr);
   nir_intrinsic_instr *ir = nir_instr_as_intrinsic(instr);

   bool returns_value = ir->intrinsic != nir_intrinsic_image_store;

   if (returns_value)
      default_result = nir_imm_zero(b, nir_dest_num_components(ir->dest),
                                    nir_dest_bit_size(ir->dest));

   /* Use an unsigned compare, with that a negative index will become a very large number
    * and the comparison should return false */
   nir_ssa_def *image_exists = nir_ult(b, ir->src[0].ssa, nir_imm_int(b, b->shader->info.num_images));

   /* If the image exists, re-emit the original instruction, otherwise return the default result */
   nir_if *if_exists = nir_push_if(b, image_exists);
   nir_instr *image_instr = nir_instr_clone(b->shader, instr);
   nir_builder_instr_insert(b, image_instr);
   nir_if *else_exists = nir_push_else(b, if_exists);
   nir_pop_if(b, else_exists);

   if (returns_value) {
      nir_intrinsic_instr *image_intr = nir_instr_as_intrinsic(image_instr);
      result = nir_if_phi(b, &image_intr->dest.ssa, default_result);
      b->cursor = nir_after_instr(result->parent_instr);
   } else {
      b->cursor = nir_after_cf_node(&else_exists->cf_node);
   }
   return result;
}

static bool
nir_lower_image_oob_access_filter(const nir_instr *instr, const void *_options)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *ir = nir_instr_as_intrinsic(instr);
   switch (ir->intrinsic) {
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
      return true;
   default:
      return false;
   }
}

bool nir_lower_image_oob_access(nir_shader *sh)
{
   return nir_shader_lower_instructions(sh,
                                        nir_lower_image_oob_access_filter,
                                        nir_lower_image_oob_access_impl,
                                        NULL);
}
