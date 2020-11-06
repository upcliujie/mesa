/*
 * Copyright © 2020 Valve Corporation
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
 * Authors:
 *    Timur Kristóf <timur.kristof@gmail.com>
 */

#include "nir.h"
#include "nir_builder.h"
#include "nir_deref.h"

/** @file nir_lower_cross_invocation_tcs_io.c
 *
 * This pass tries to eliminate cross-invocation tessellation control
 * shader input/output reads, where possible.
 *
 * TCS can read their outputs, not only their inputs. In a backend, this may
 * typically be implemented by storing the output to a fast temporary storage
 * (such as LDS, on AMD GPUs) and loading them from said storage.
 * On the other hand, same-invocation reads don't need stores, and can use
 * registers to pass the TCS outputs.
 *
 * When the output is not accessed indirectly (such as through a non-const
 * array index), it is possible to replace the cross-invocation output read
 * with a same-invocation read combined with a subgroup operation, as long
 * as we know in advance that TCS patches are not broken up into multiple
 * subgroups.
 *
 * Additionally, some GPUs (such as newer AMD GPUs) merge the VS and TCS
 * stages into a single hardware stage. In this case, it is also beneficial
 * to eliminate cross-invocation input reads, in the same manner.
 *
 * NOTE: Consider the following caveats:
 *
 * For this to work, the caller MUST ensure that all invocations
 * that belong to the same patch fit into the same subgroup.
 *
 * For example, this is NOT valid if the subgroup size is 32 and the
 * output patch size is 3, when you have more than 10 patches, because
 * the 1st vertex of the 11th patch will be processed by another subgroup.
 */

typedef struct
{
   nir_shader *shader;
   nir_cross_invocation_tcs_io_options options;
} lower_cross_invocation_tcs_io_state;

static nir_ssa_def *
lower_load_deref(nir_builder *b, nir_instr *instr, void *st)
{
   const lower_cross_invocation_tcs_io_state *state =
      (const lower_cross_invocation_tcs_io_state *) st;

   nir_intrinsic_instr *old_intrin = nir_instr_as_intrinsic(instr);
   nir_deref_instr *old_deref = nir_src_as_deref(old_intrin->src[0]);

   nir_deref_path path;
   nir_deref_path_init(&path, old_deref, NULL);
   nir_deref_instr *deref_var = path.path[0];
   assert(deref_var->deref_type == nir_deref_type_var);
   nir_deref_instr **p = &path.path[1];

   /* Vertex index is the outermost array index. */
   assert((*p)->deref_type == nir_deref_type_array);
   nir_src vertex_index_src = (*p)->arr.index;
   bool vertex_index_is_const = nir_src_is_const(vertex_index_src);
   p++;

   /* Determine what we can do with it. */
   bool use_invocation_id =
      state->shader->info.tess.tcs_vertices_out == 1;
   bool use_quad_swizzle_amd =
      vertex_index_is_const && state->options.allow_quad_swizzle_amd &&
      state->shader->info.tess.tcs_vertices_out == 2;
   bool use_quad_broadcast =
      ((vertex_index_is_const && state->options.allow_const_quad_broadcast) ||
       state->options.allow_dynamic_quad_broadcast) &&
      state->shader->info.tess.tcs_vertices_out == 4;

   /* Don't emit dead IR */
   if (!use_invocation_id &&
       !use_quad_swizzle_amd &&
       !use_quad_broadcast &&
       !state->options.allow_shuffle) {
      nir_deref_path_finish(&path);
      return NULL;
   }

   /* Create new array deref for same-invocation vertex index. */
   nir_ssa_def *inv_in_patch = nir_load_invocation_id(b);
   nir_deref_instr *sameinv_vtx_arr_deref = nir_build_deref_array(b, deref_var, inv_in_patch);
   nir_deref_instr *deref = sameinv_vtx_arr_deref;

   /* Follow the deref chain and mimic it. */
   for (; *p; p++) {
      if ((*p)->deref_type == nir_deref_type_array)
         deref = nir_build_deref_array(b, deref, (*p)->arr.index.ssa);
      else if ((*p)->deref_type == nir_deref_type_struct)
         deref = nir_build_deref_struct(b, deref, (*p)->strct.index);
      else
         unreachable("Unsupported deref type");
   }

   nir_deref_path_finish(&path);

   /* Rebuild the load_deref */
   nir_ssa_def *loaded_sameinv_deref = nir_load_deref(b, deref);

   if (use_invocation_id) {
      /* The trivial case. Since we only have 1 vertex per patch,
       * we can always use the invocation id as the vertex index.
       */

      return loaded_sameinv_deref;

   } else if (use_quad_swizzle_amd) {
      /* When the vertex count is 2, and the vertex index is const,
       * we can use quad_swizze_amd, which allows an arbitrary swizzle
       * within 4 threads.
       */

      nir_intrinsic_instr *qsw =
         nir_intrinsic_instr_create(b->shader, nir_intrinsic_quad_swizzle_amd);

      unsigned vertex_index_const_value = nir_src_as_uint(vertex_index_src);
      unsigned mask = vertex_index_const_value |
                      vertex_index_const_value << 2 |
                      (vertex_index_const_value + 2) << 4 |
                      (vertex_index_const_value + 2) << 6;
      nir_intrinsic_set_swizzle_mask(qsw, mask);

      qsw->num_components = loaded_sameinv_deref->num_components;
      qsw->src[0] = nir_src_for_ssa(loaded_sameinv_deref);
      nir_ssa_dest_init(&qsw->instr, &qsw->dest,
                        loaded_sameinv_deref->num_components,
                        loaded_sameinv_deref->bit_size, NULL);

      nir_builder_instr_insert(b, &qsw->instr);
      return &qsw->dest.ssa;

   } else if (use_quad_broadcast) {
      /* When the vertex count is 4, we can trivially use quad broadcast,
       * to select any vertex index within 4 threads.
       * NOTE: if vertex index is non-const, this will result in a dynamic
       *       quad broadcast, which may not be worth doing if the backend
       *       can't compile that to something that is efficient on the GPU.
       */

      nir_intrinsic_instr *qbcst =
         nir_intrinsic_instr_create(b->shader, nir_intrinsic_quad_broadcast);

      qbcst->num_components = loaded_sameinv_deref->num_components;
      qbcst->src[0] = nir_src_for_ssa(loaded_sameinv_deref);
      qbcst->src[1] = vertex_index_src;
      nir_ssa_dest_init(&qbcst->instr, &qbcst->dest,
                        loaded_sameinv_deref->num_components,
                        loaded_sameinv_deref->bit_size, NULL);

      nir_builder_instr_insert(b, &qbcst->instr);
      return &qbcst->dest.ssa;

   } else if (state->options.allow_shuffle) {
      /* The "fallback" case. We simply load the same-invocation input,
       * and use shuffle to get the input we want from the given vertex index.
       */

      nir_ssa_def *inv_in_subgroup =
         nir_load_subgroup_invocation(b);
      nir_ssa_def *patch_vtx0_inv_in_subgrp =
         nir_isub(b, inv_in_subgroup, inv_in_patch);
      nir_ssa_def *other_vtx_inv_id_in_subgrp =
         nir_iadd(b, patch_vtx0_inv_in_subgrp, vertex_index_src.ssa);

      nir_intrinsic_instr *shuffle =
         nir_intrinsic_instr_create(b->shader, nir_intrinsic_shuffle);

      shuffle->num_components = loaded_sameinv_deref->num_components;
      shuffle->src[0] = nir_src_for_ssa(loaded_sameinv_deref);
      shuffle->src[1] = nir_src_for_ssa(other_vtx_inv_id_in_subgrp);
      nir_ssa_dest_init(&shuffle->instr, &shuffle->dest,
                        loaded_sameinv_deref->num_components,
                        loaded_sameinv_deref->bit_size, NULL);

      nir_builder_instr_insert(b, &shuffle->instr);
      return &shuffle->dest.ssa;

   } else {
      unreachable("We should have already returned without emitting dead IR.");
   }

   return NULL;
}

static bool
filter_load_deref(const nir_instr *instr, const void *st)
{
   const lower_cross_invocation_tcs_io_state *state =
      (const lower_cross_invocation_tcs_io_state *) st;

   /* Only intrinsics are affected */
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   /* Ignore unsupported bit sizes */
   if (intrin->dest.ssa.bit_size > state->options.max_bit_size)
      return false;

   /* Only load intrinsics are affected */
   if (intrin->intrinsic != nir_intrinsic_load_deref)
      return false;

   nir_deref_instr *deref = nir_src_as_deref(intrin->src[0]);

   /* We always care about outputs, but inputs only when we are told so. */
   if (!(nir_deref_mode_is(deref, nir_var_shader_out) ||
         (nir_deref_mode_is(deref, nir_var_shader_in) && state->options.merged_vs_tcs)))
      return false;

   nir_variable *var = nir_deref_instr_get_variable(deref);
   const bool per_vertex = nir_is_per_vertex_io(var, state->shader->info.stage);

   /* Only per-vertex I/O is affected. */
   if (!per_vertex)
      return false;

   nir_deref_path path;
   nir_deref_path_init(&path, deref, NULL);
   assert(path.path[0]->deref_type == nir_deref_type_var);
   nir_deref_instr **p = &path.path[1];

   /* Vertex index is the outermost array index. */
   assert((*p)->deref_type == nir_deref_type_array);
   nir_instr *vertex_index_instr = (*p)->arr.index.ssa->parent_instr;
   bool indirect = false;
   bool cross_invocation =
      vertex_index_instr->type != nir_instr_type_intrinsic ||
      nir_instr_as_intrinsic(vertex_index_instr)->intrinsic !=
         nir_intrinsic_load_invocation_id;
   p++;

   /* We only want to lower cross-invocation loads. */
   if (!cross_invocation) {
      nir_deref_path_finish(&path);
      return false;
   }

   /* We always lower indirect dereferences for "compact" array vars. */
   if (!path.path[0]->var->data.compact) {
      /* Non-compact array vars: find out if they are indirect. */
      for (; *p; p++) {
         if ((*p)->deref_type == nir_deref_type_array) {
            indirect |= !nir_src_is_const((*p)->arr.index);
         } else if ((*p)->deref_type == nir_deref_type_struct) {
            /* Struct indices are always constant. */
         } else {
            unreachable("Unsupported deref type");
         }
      }
   }

   nir_deref_path_finish(&path);

   /* We can't do anything about indirect indices, sadly. */
   if (indirect)
      return false;

   return true;
}

static bool
filter_uniform_blocks(const nir_block *block, const void *st)
{
   if (block->cf_node.type == nir_cf_node_if)
      return !nir_if_is_divergent(nir_cf_node_as_if(&block->cf_node));
   else if (block->cf_node.type == nir_cf_node_loop)
      return !nir_loop_is_divergent(nir_cf_node_as_loop(&block->cf_node));

   return true;
}

bool
nir_lower_cross_invocation_tcs_io(nir_shader *shader,
                                  nir_cross_invocation_tcs_io_options options)
{
   /* This pass is for tess control shaders only. */
   if (shader->info.stage != MESA_SHADER_TESS_CTRL)
      return false;

   lower_cross_invocation_tcs_io_state state = {
      .shader = shader,
      .options = options,
   };

   return nir_shader_filter_blocks_lower_instructions(shader,
                                                      filter_uniform_blocks,
                                                      filter_load_deref,
                                                      lower_load_deref,
                                                      (void *) &state);
}
