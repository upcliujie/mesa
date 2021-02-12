/*
 * Copyright (C) 2021 Collabora, Ltd.
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
 *
 */

#include "nir.h"
#include "nir/nir_builder.h"

/* Lower GLSL style barriers to scoped_barrier intrinsics, after which
 * nir_opt_barriers can combine adjacent barriers. */

static bool
lower_to_scoped_impl(nir_builder *b, nir_instr *instr, UNUSED void *data)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   b->cursor = nir_before_instr(instr);

   nir_memory_semantics sem =
      NIR_MEMORY_ACQ_REL | NIR_MEMORY_MAKE_AVAILABLE | NIR_MEMORY_MAKE_VISIBLE;

   switch (intrin->intrinsic) {
   case nir_intrinsic_control_barrier:
      nir_scoped_barrier(b,
            .execution_scope = NIR_SCOPE_WORKGROUP,
            .memory_scope = NIR_SCOPE_NONE);
      break;

   case nir_intrinsic_memory_barrier:
      nir_scoped_memory_barrier(b, NIR_SCOPE_DEVICE, sem, nir_var_all);
      break;
 
   case nir_intrinsic_group_memory_barrier:
      nir_scoped_memory_barrier(b, NIR_SCOPE_WORKGROUP, sem, nir_var_all);
      break;
 
   case nir_intrinsic_memory_barrier_atomic_counter:
   case nir_intrinsic_memory_barrier_buffer:
       nir_scoped_memory_barrier(b, NIR_SCOPE_DEVICE, sem, nir_var_mem_ssbo);
       break;

       /* TODO: what should this be */
  case nir_intrinsic_memory_barrier_image:
      nir_scoped_memory_barrier(b, NIR_SCOPE_DEVICE, sem, nir_var_mem_generic);
      break;

   case nir_intrinsic_memory_barrier_shared:
      nir_scoped_memory_barrier(b, NIR_SCOPE_WORKGROUP, sem,
            nir_var_mem_shared);
      break;
 
   case nir_intrinsic_memory_barrier_tcs_patch:
      nir_scoped_memory_barrier(b, NIR_SCOPE_DEVICE, sem, nir_var_shader_out);
      break;

   default:
      return false;
   }

   nir_instr_remove(instr);
   return true;
}

bool
nir_lower_barriers(nir_shader *shader)
{
   return nir_shader_instructions_pass(shader,
                                       lower_to_scoped_impl,
                                       nir_metadata_block_index |
                                       nir_metadata_dominance,
                                       NULL);
}
