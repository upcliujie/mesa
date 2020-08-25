/*
 * Copyright © 2019 Valve Corporation
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

/* This pass optimizes GL access qualifiers. So far it does three things:
 *
 * - Infer readonly when it's missing.
 * - Infer writeonly when it's missing.
 * - Infer ACCESS_CAN_REORDER when the following are true:
 *   - Either there are no writes, or ACCESS_NON_WRITEABLE and ACCESS_RESTRICT
 *     are both set. In either case there are no writes to the underlying
 *     memory.
 *   - If ACCESS_COHERENT is set, then there must be no memory barriers
 *     involving the access. Coherent accesses may return different results
 *     before and after barriers.
 *   - ACCESS_VOLATILE is not set.
 *
 * If these conditions are true, then image and buffer reads may be treated as
 * if they were uniform buffer reads, i.e. they may be arbitrarily moved,
 * combined, rematerialized etc.
 */

struct access_state {
   nir_shader *shader;
   bool is_vulkan;
   bool infer_non_readable;

   struct set *vars_written;
   struct set *vars_read;
   bool images_written;
   bool buffers_written;
   bool images_read;
   bool buffers_read;
   bool image_barriers;
   bool buffer_barriers;
   bool make_visible;
};

static void
gather_buffer_access(struct access_state *state, nir_ssa_def *def, bool read, bool write)
{
   state->buffers_read |= read;
   state->buffers_written |= write;

   if (!def)
      return;

   const nir_variable *var = nir_get_binding_variable(
      state->shader, nir_chase_binding(nir_src_for_ssa(def)));
   if (var) {
      if (read)
         _mesa_set_add(state->vars_read, var);
      if (write)
         _mesa_set_add(state->vars_written, var);
   } else {
      nir_foreach_variable_with_modes(possible_var, state->shader, nir_var_mem_ssbo) {
         if (read)
            _mesa_set_add(state->vars_read, possible_var);
         if (write)
            _mesa_set_add(state->vars_written, possible_var);
      }
   }
}

static void
gather_intrinsic(struct access_state *state, nir_intrinsic_instr *instr)
{
   const nir_variable *var;
   bool read, write;
   switch (instr->intrinsic) {
   case nir_intrinsic_image_deref_load:
   case nir_intrinsic_image_deref_store:
   case nir_intrinsic_image_deref_atomic_add:
   case nir_intrinsic_image_deref_atomic_imin:
   case nir_intrinsic_image_deref_atomic_umin:
   case nir_intrinsic_image_deref_atomic_imax:
   case nir_intrinsic_image_deref_atomic_umax:
   case nir_intrinsic_image_deref_atomic_and:
   case nir_intrinsic_image_deref_atomic_or:
   case nir_intrinsic_image_deref_atomic_xor:
   case nir_intrinsic_image_deref_atomic_exchange:
   case nir_intrinsic_image_deref_atomic_comp_swap:
   case nir_intrinsic_image_deref_atomic_fadd:
      var = nir_intrinsic_get_var(instr, 0);
      read = instr->intrinsic != nir_intrinsic_image_deref_store;
      write = instr->intrinsic != nir_intrinsic_image_deref_load;

      /* In OpenGL, buffer images use normal buffer objects, whereas other
       * image types use textures which cannot alias with buffer objects.
       * Therefore we have to group buffer samplers together with SSBO's.
       */
      if (glsl_get_sampler_dim(glsl_without_array(var->type)) ==
          GLSL_SAMPLER_DIM_BUF) {
         state->buffers_read |= read;
         state->buffers_written |= write;
      } else {
         state->images_read |= read;
         state->images_written |= write;
      }

      if (var->data.mode == nir_var_uniform && read)
         _mesa_set_add(state->vars_read, var);
      if (var->data.mode == nir_var_uniform && write)
         _mesa_set_add(state->vars_written, var);
      break;

   case nir_intrinsic_bindless_image_load:
   case nir_intrinsic_bindless_image_store:
   case nir_intrinsic_bindless_image_atomic_add:
   case nir_intrinsic_bindless_image_atomic_imin:
   case nir_intrinsic_bindless_image_atomic_umin:
   case nir_intrinsic_bindless_image_atomic_imax:
   case nir_intrinsic_bindless_image_atomic_umax:
   case nir_intrinsic_bindless_image_atomic_and:
   case nir_intrinsic_bindless_image_atomic_or:
   case nir_intrinsic_bindless_image_atomic_xor:
   case nir_intrinsic_bindless_image_atomic_exchange:
   case nir_intrinsic_bindless_image_atomic_comp_swap:
   case nir_intrinsic_bindless_image_atomic_fadd:
      read = instr->intrinsic != nir_intrinsic_bindless_image_store;
      write = instr->intrinsic != nir_intrinsic_bindless_image_load;

      if (nir_intrinsic_image_dim(instr) == GLSL_SAMPLER_DIM_BUF) {
         state->buffers_read |= read;
         state->buffers_written |= write;
      } else {
         state->images_read |= read;
         state->images_written |= write;
      }
      break;

   case nir_intrinsic_load_deref:
   case nir_intrinsic_store_deref:
   case nir_intrinsic_deref_atomic_add:
   case nir_intrinsic_deref_atomic_imin:
   case nir_intrinsic_deref_atomic_umin:
   case nir_intrinsic_deref_atomic_imax:
   case nir_intrinsic_deref_atomic_umax:
   case nir_intrinsic_deref_atomic_and:
   case nir_intrinsic_deref_atomic_or:
   case nir_intrinsic_deref_atomic_xor:
   case nir_intrinsic_deref_atomic_exchange:
   case nir_intrinsic_deref_atomic_comp_swap:
   case nir_intrinsic_deref_atomic_fadd:
   case nir_intrinsic_deref_atomic_fmin:
   case nir_intrinsic_deref_atomic_fmax:
   case nir_intrinsic_deref_atomic_fcomp_swap: {
      nir_deref_instr *deref = nir_src_as_deref(instr->src[0]);
      if (!nir_deref_mode_may_be(deref, nir_var_mem_ssbo | nir_var_mem_global))
         break;

      bool ssbo = nir_deref_mode_is(deref, nir_var_mem_ssbo);
      gather_buffer_access(state, ssbo ? instr->src[0].ssa : NULL,
                           instr->intrinsic != nir_intrinsic_store_deref,
                           instr->intrinsic != nir_intrinsic_load_deref);
      break;
   }

   case nir_intrinsic_group_memory_barrier:
   case nir_intrinsic_memory_barrier:
      state->buffer_barriers = true;
      state->image_barriers = true;
      break;

   case nir_intrinsic_memory_barrier_buffer:
      state->buffer_barriers = true;
      break;

   case nir_intrinsic_memory_barrier_image:
      state->image_barriers = true;
      break;

   case nir_intrinsic_scoped_barrier:
      /* TODO: Could be more granular if we had nir_var_mem_image. */
      if (nir_intrinsic_memory_modes(instr) & (nir_var_mem_ubo |
                                               nir_var_mem_ssbo |
                                               nir_var_uniform |
                                               nir_var_mem_global)) {
         state->buffer_barriers = true;
         state->image_barriers = true;
      }

      if (nir_intrinsic_memory_semantics(instr) & NIR_MEMORY_MAKE_VISIBLE)
         state->make_visible = true;
      break;

   default:
      break;
   }
}

static bool
process_variable(struct access_state *state, nir_variable *var)
{
   if (var->data.mode != nir_var_mem_ssbo &&
       !(var->data.mode == nir_var_uniform &&
         glsl_type_is_image(var->type)))
      return false;

   /* Ignore variables we've already marked */
   if (var->data.access & ACCESS_CAN_REORDER)
      return false;

   bool restrict_or_gl = (var->data.access & ACCESS_RESTRICT) ||
                         !state->is_vulkan;

   if (!(var->data.access & ACCESS_NON_WRITEABLE) && restrict_or_gl &&
       !_mesa_set_search(state->vars_written, var)) {
      var->data.access |= ACCESS_NON_WRITEABLE;
      return true;
   }

   if (state->infer_non_readable && restrict_or_gl &&
       !(var->data.access & ACCESS_NON_READABLE) &&
       !_mesa_set_search(state->vars_read, var)) {
      var->data.access |= ACCESS_NON_READABLE;
      return true;
   }

   return false;
}

static bool
update_access(struct access_state *state, nir_intrinsic_instr *instr, bool is_image, bool is_buffer)
{
   enum gl_access_qualifier access = nir_intrinsic_access(instr);

   bool is_restrict = access & ACCESS_RESTRICT;
   bool is_var_readonly = access & ACCESS_NON_WRITEABLE;
   bool is_var_writeonly = access & ACCESS_NON_READABLE;

   if (instr->intrinsic == nir_intrinsic_bindless_image_load ||
       instr->intrinsic == nir_intrinsic_bindless_image_store) {
      /* We have less information about bindless intrinsics, since we can't
       * always trace uses back to the variable. Don't try and infer if it's
       * read-only, unless there are no image writes at all.
       */
      assert(!state->is_vulkan);
      is_var_readonly |=
         is_buffer ? !state->buffers_written : !state->images_written;
      is_var_writeonly |=
         is_buffer ? !state->buffers_read : !state->images_read;
   } else {
      const nir_variable *var = nir_get_binding_variable(
         state->shader, nir_chase_binding(instr->src[0]));
      is_restrict |= var && (var->data.access & ACCESS_RESTRICT);
      is_var_readonly |= var && (var->data.access & ACCESS_NON_WRITEABLE);
      is_var_writeonly |= var && (var->data.access & ACCESS_NON_READABLE);
   }

   /* In Vulkan, ACCESS_NON_WRITEABLE means that the memory is
    * non-writeable while in GL it means that the variable is non-writeable.
    */
   bool is_memory_readonly = state->is_vulkan && (access & ACCESS_NON_WRITEABLE);
   is_memory_readonly |= is_var_readonly && is_restrict;
   if (state->is_vulkan)
      is_memory_readonly |= !state->buffers_written && !state->images_written;
   else
      is_memory_readonly |= is_buffer ? !state->buffers_written : !state->images_written;

   bool is_memory_writeonly = state->is_vulkan && (access & ACCESS_NON_READABLE);
   is_memory_writeonly |= is_var_writeonly && is_restrict;
   if (state->is_vulkan)
      is_memory_writeonly |= !state->buffers_read && !state->images_read;
   else
      is_memory_writeonly |= is_buffer ? !state->buffers_read : !state->images_read;

   /* Note: memoryBarrierBuffer() is only guaranteed to flush buffer
    * variables and not imageBuffer's, so we only consider the GL-level
    * type here.
    */
   bool is_any_barrier = is_image ?
      state->image_barriers : state->buffer_barriers;
   /* TODO: SPIR-V has a private qualifier that we could use here */
   bool coherent = (access & ACCESS_COHERENT) || state->make_visible;
   if ((!is_any_barrier || !coherent) &&
       !(access & ACCESS_VOLATILE) &&
       is_memory_readonly)
      access |= ACCESS_CAN_REORDER;

   if (state->is_vulkan ? is_memory_readonly : is_var_readonly)
      access |= ACCESS_NON_WRITEABLE;
   if (state->is_vulkan ? is_memory_writeonly : is_var_writeonly)
      access |= ACCESS_NON_READABLE;

   bool progress = nir_intrinsic_access(instr) != access;
   nir_intrinsic_set_access(instr, access);
   return progress;
}

static bool
process_intrinsic(struct access_state *state, nir_intrinsic_instr *instr)
{
   switch (instr->intrinsic) {
   case nir_intrinsic_bindless_image_load:
   case nir_intrinsic_bindless_image_store:
      return update_access(state, instr, true,
                           nir_intrinsic_image_dim(instr) == GLSL_SAMPLER_DIM_BUF);

   case nir_intrinsic_load_deref:
   case nir_intrinsic_store_deref: {
      if (!nir_deref_mode_is(nir_src_as_deref(instr->src[0]), nir_var_mem_ssbo))
         return false;

      return update_access(state, instr, false, true);
   }

   case nir_intrinsic_image_deref_load:
   case nir_intrinsic_image_deref_store: {
      nir_variable *var = nir_intrinsic_get_var(instr, 0);

      bool is_buffer =
         glsl_get_sampler_dim(glsl_without_array(var->type)) == GLSL_SAMPLER_DIM_BUF;

      return update_access(state, instr, true, is_buffer);
   }

   default:
      return false;
   }
}

static bool
opt_access_impl(struct access_state *state,
                nir_function_impl *impl)
{
   bool progress = false;

   nir_foreach_block(block, impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type == nir_instr_type_intrinsic)
            progress |= process_intrinsic(state,
                                          nir_instr_as_intrinsic(instr));
      }
   }

   if (progress) {
      nir_metadata_preserve(impl,
                            nir_metadata_block_index |
                            nir_metadata_dominance |
                            nir_metadata_live_ssa_defs |
                            nir_metadata_loop_analysis);
   }


   return progress;
}

bool
nir_opt_access(nir_shader *shader, const nir_opt_access_options *options)
{
   struct access_state state = {
      .shader = shader,
      .infer_non_readable = options->infer_non_readable,
      .is_vulkan = options->is_vulkan,
      .vars_written = _mesa_pointer_set_create(NULL),
      .vars_read = _mesa_pointer_set_create(NULL),
   };

   bool var_progress = false;
   bool progress = false;

   nir_foreach_function(func, shader) {
      if (func->impl) {
         nir_foreach_block(block, func->impl) {
            nir_foreach_instr(instr, block) {
               if (instr->type == nir_instr_type_intrinsic)
                  gather_intrinsic(&state, nir_instr_as_intrinsic(instr));
            }
         }
      }
   }

   nir_foreach_variable_with_modes(var, shader, nir_var_uniform |
                                                nir_var_mem_ubo |
                                                nir_var_mem_ssbo)
      var_progress |= process_variable(&state, var);

   nir_foreach_function(func, shader) {
      if (func->impl) {
         progress |= opt_access_impl(&state, func->impl);

         /* If we make a change to the uniforms, update all the impls. */
         if (var_progress) {
            nir_metadata_preserve(func->impl,
                                  nir_metadata_block_index |
                                  nir_metadata_dominance |
                                  nir_metadata_live_ssa_defs |
                                  nir_metadata_loop_analysis);
         }
      }
   }

   progress |= var_progress;

   _mesa_set_destroy(state.vars_read, NULL);
   _mesa_set_destroy(state.vars_written, NULL);
   return progress;
}
