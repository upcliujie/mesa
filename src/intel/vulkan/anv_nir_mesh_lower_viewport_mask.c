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

#include "anv_nir.h"
#include "nir_builder.h"

#define DEBUG_MS_VPM 0

#if DEBUG_MS_VPM
#define msvpm_printf(...) fprintf(stderr, __VA_ARGS__)
#else
static inline void
msvpm_printf(const char *format, ...)
{
   (void)format;
}
#endif

struct mesh_lower_viewport_mask_state {
   struct anv_graphics_pipeline *pipeline;
   nir_shader *nir;
   nir_variable *primitive_indices;
   nir_variable *view_indices;
   nir_variable *viewport_index;
   nir_variable *cull_primitive_mask;
   unsigned viewport_count;
};

static void
handle_primitive_count(struct nir_builder *b,
                       nir_instr *instr,
                       nir_intrinsic_instr *intrin,
                       struct mesh_lower_viewport_mask_state *state)
{
   /*
    * gl_PrimitiveCountNV *= viewport_count;
    */

   b->cursor = nir_before_instr(instr);
   nir_ssa_def *new_primitive_count =
         nir_imul_imm(b, intrin->src[1].ssa, state->viewport_count);

   nir_instr_rewrite_src_ssa(instr, &intrin->src[1], new_primitive_count);
}

static void
handle_primitive_indices(struct nir_builder *b,
                         nir_instr *instr,
                         nir_intrinsic_instr *intrin,
                         struct mesh_lower_viewport_mask_state *state,
                         nir_ssa_def *ind,
                         const nir_variable *var)
{
   /*
    * Replace:
    * gl_PrimitiveIndicesNV[i] := vtx;
    *
    * By:
    * for (int viewport = 0; viewport < viewport_count; ++viewport) {
    *     gl_PrimitiveIndicesNV[i / VERT_PER_PRIM * viewport_count * VERT_PER_PRIM +
    *                           i % VERT_PER_PRIM +
    *                           viewport * VERT_PER_PRIM] := vtx;
    * }
    *
    * Note: new index math is so complex because indices from each primitive
    * must be close to each other.
    */

   unsigned writemask = nir_intrinsic_write_mask(intrin);

   unsigned vertices_per_primitive =
         num_mesh_vertices_per_primitive(state->nir->info.mesh.primitive_type);

   b->cursor = nir_before_instr(instr);

   if (!state->primitive_indices) {
      const struct glsl_type *type =
            glsl_array_type(glsl_uint_type(),
                            state->nir->info.mesh.max_primitives_out *
                            state->viewport_count * vertices_per_primitive,
                            0);

      state->primitive_indices =
            nir_variable_create(b->shader,
                                nir_var_shader_out,
                                type,
                                "gl_PrimitiveIndicesNV");
      state->primitive_indices->data.location = var->data.location;
      state->primitive_indices->data.interpolation = var->data.interpolation;
   }

   nir_deref_instr *primitive_indices_deref =
         nir_build_deref_var(b, state->primitive_indices);

   nir_ssa_def *viewport_count_def = nir_imm_int(b, state->viewport_count);
   nir_ssa_def *vert_per_prim = nir_imm_int(b, vertices_per_primitive);
   nir_ssa_def *ind_div = nir_idiv(b, ind, vert_per_prim);
   nir_ssa_def *ind_mod = nir_umod(b, ind, vert_per_prim);

   for (unsigned viewport = 0; viewport < state->viewport_count; ++viewport) {
      nir_ssa_def *viewport_def = nir_imm_int(b, viewport);

      nir_ssa_def *new_idx =
            nir_iadd3(b,
                      nir_imul(b,
                               nir_imul(b, ind_div, viewport_count_def),
                               vert_per_prim),
                      ind_mod,
                      nir_imul(b, viewport_def, vert_per_prim)
                     );

      nir_deref_instr *reindexed_deref =
            nir_build_deref_array(b, primitive_indices_deref, new_idx);

      nir_store_deref(b, reindexed_deref, intrin->src[1].ssa, writemask);
   }

   nir_instr_remove(instr);
}

static void
handle_view_indices(struct nir_builder *b,
                    nir_instr *instr,
                    nir_intrinsic_instr *intrin,
                    struct mesh_lower_viewport_mask_state *state,
                    nir_ssa_def *prim,
                    const nir_variable *var)
{
   /*
    * Replace:
    * ViewID[prim] := view;
    *
    * By:
    * for (int viewport = 0; viewport < viewport_count; ++viewport)
    *     ViewID[prim * numViewports + viewport] := view;
    *
    */

   unsigned writemask = nir_intrinsic_write_mask(intrin);

   b->cursor = nir_before_instr(instr);

   if (!state->view_indices) {
      const struct glsl_type *type =
            glsl_array_type(glsl_uint_type(),
                            state->nir->info.mesh.max_primitives_out *
                            state->viewport_count,
                            0);

      state->view_indices =
            nir_variable_create(b->shader,
                                nir_var_shader_out,
                                type,
                                "GeneratedViewID2");
      state->view_indices->data.location = var->data.location;
      state->view_indices->data.interpolation = var->data.interpolation;
      state->view_indices->data.per_primitive = var->data.per_primitive;
   }

   nir_deref_instr *view_indices_deref =
         nir_build_deref_var(b, state->view_indices);

   nir_ssa_def *viewport_count_def = nir_imm_int(b, state->viewport_count);

   for (unsigned viewport = 0; viewport < state->viewport_count; ++viewport) {
      nir_ssa_def *viewport_def = nir_imm_int(b, viewport);

      nir_ssa_def *new_idx =
            nir_iadd(b, nir_imul(b, prim, viewport_count_def), viewport_def);

      nir_deref_instr *reindexed_deref =
            nir_build_deref_array(b, view_indices_deref, new_idx);

      nir_store_deref(b, reindexed_deref, intrin->src[1].ssa, writemask);
   }

   nir_instr_remove(instr);
}

static void
handle_viewport_mask(struct nir_builder *b,
                     nir_instr *instr,
                     nir_intrinsic_instr *intrin,
                     struct mesh_lower_viewport_mask_state *state,
                     nir_ssa_def *ind,
                     nir_ssa_def *prim,
                     const nir_variable *var)
{
   /*
    * Replace:
    * gl_MeshPrimitivesNV[prim].gl_ViewportMask[ind] := VAL
    *
    * By:
    * for (int viewport = 0; viewport < numViewports; ++viewport)
    *   if ((1 << viewport) & VAL)
    *      gl_MeshPrimitivesNV[prim * numViewports + viewport].gl_ViewportIndex := viewport;
    *   else
    *      gl_MeshPrimitivesNV[prim * numViewports + viewport].CullPrimitiveMask := 1;
    */

   b->cursor = nir_before_instr(instr);

   if (!state->viewport_index) {
      const struct glsl_type *type =
            glsl_array_type(glsl_int_type(),
                            state->nir->info.mesh.max_primitives_out *
                            state->viewport_count,
                            0);

      state->viewport_index =
            nir_variable_create(b->shader,
                                nir_var_shader_out,
                                type,
                                "gl_MeshPrimitivesNV[*].gl_ViewportIndex");
      state->viewport_index->data.location = VARYING_SLOT_VIEWPORT;
      state->viewport_index->data.interpolation = INTERP_MODE_NONE;
      state->viewport_index->data.per_primitive = 1;

      state->cull_primitive_mask =
            nir_variable_create(b->shader,
                                nir_var_shader_out,
                                type,
                                "CullPrimitiveMask");
      state->cull_primitive_mask->data.location = VARYING_SLOT_CULL_PRIMITIVE_MASK_INTEL;
      state->cull_primitive_mask->data.interpolation = INTERP_MODE_NONE;
      state->cull_primitive_mask->data.per_primitive = 1;
   }

   nir_ssa_def *zero = nir_imm_int(b, 0);
   nir_ssa_def *one = nir_imm_int(b, 1);

   nir_ssa_def *viewport_count = nir_imm_int(b, state->viewport_count);
   nir_ssa_def *start = nir_imul(b, prim, viewport_count);
   nir_deref_instr *viewport_index_deref =
         nir_build_deref_var(b, state->viewport_index);
   nir_deref_instr *cull_primitive_mask_deref =
         nir_build_deref_var(b, state->cull_primitive_mask);

   nir_function_impl *entry = nir_shader_get_entrypoint(state->nir);
   nir_variable *viewport_var =
         nir_local_variable_create(entry, glsl_uint_type(), "viewport");
   nir_deref_instr *viewport_deref = nir_build_deref_var(b, viewport_var);
   nir_store_deref(b, viewport_deref, zero, 1);

   nir_loop *loop = nir_push_loop(b);
   {
      nir_ssa_def *viewport = nir_load_deref(b, viewport_deref);
      nir_ssa_def *cmp = nir_ige(b, viewport, viewport_count);
      nir_if *loop_check = nir_push_if(b, cmp);
      nir_jump(b, nir_jump_break);
      nir_pop_if(b, loop_check);

      nir_ssa_def *new_idx = nir_iadd(b, start, viewport);

      nir_ssa_def *viewport_shifted = nir_ishl(b, one, viewport);
      nir_ssa_def *mask = nir_iand(b, viewport_shifted, intrin->src[1].ssa);
      nir_ssa_def *mask_zero = nir_ine(b, mask, zero);

      nir_if *mask_check = nir_push_if(b, mask_zero);
      {
         nir_deref_instr *indexed_viewport_index_deref =
               nir_build_deref_array(b, viewport_index_deref, new_idx);
         nir_store_deref(b, indexed_viewport_index_deref, viewport, 1);
      }
      nir_push_else(b, mask_check);
      {
         nir_deref_instr *indexed_cull_primitive_mask_deref =
               nir_build_deref_array(b, cull_primitive_mask_deref, new_idx);
         nir_store_deref(b, indexed_cull_primitive_mask_deref, one, 1);
      }
      nir_pop_if(b, mask_check);

      nir_store_deref(b, viewport_deref, nir_iadd_imm(b, viewport, 1), 1);
   }
   nir_pop_loop(b, loop);

   nir_instr_remove(instr);
}

static bool
anv_nir_mesh_lower_viewport_mask_instr(struct nir_builder *b,
                                       nir_instr *instr,
                                       void *cb_data)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   if (intrin->intrinsic != nir_intrinsic_store_deref)
      return false;

   nir_deref_instr *lvl1_deref = nir_src_as_deref(intrin->src[0]);

   struct mesh_lower_viewport_mask_state *state = cb_data;
   bool progress = false;

   switch (lvl1_deref->deref_type) {
   case nir_deref_type_var: {
      const nir_variable *var = lvl1_deref->var;
      msvpm_printf("location: %d", var->data.location);

      if (var->data.location == VARYING_SLOT_PRIMITIVE_COUNT) {
         msvpm_printf(", VARYING_SLOT_PRIMITIVE_COUNT");

         handle_primitive_count(b, instr, intrin, state);

         progress = true;
      } else {
         assert(!"unhandled lvl1 var location");
      }

      msvpm_printf("\n");
      break;
   }
   case nir_deref_type_array: {
      nir_src lvl1_index = lvl1_deref->arr.index;

      if (nir_src_is_const(lvl1_index))
         msvpm_printf("array index: %" PRIu64, nir_src_as_uint(lvl1_index));
      else
         msvpm_printf("non-const array index");

      nir_deref_instr *lvl2_deref = nir_src_as_deref(lvl1_deref->parent);
      if (lvl2_deref->deref_type == nir_deref_type_var) {
         const nir_variable *var = lvl2_deref->var;
         int location = var->data.location;

         msvpm_printf(", location: %d", location);

         if (var->data.per_view)
            msvpm_printf(", per_view");
         if (var->data.per_primitive)
            msvpm_printf(", per_primitive");

         if (location == VARYING_SLOT_PRIMITIVE_INDICES) {
            msvpm_printf(", VARYING_SLOT_PRIMITIVE_INDICES");

            handle_primitive_indices(b, instr, intrin, state,
                                     lvl1_index.ssa, var);

            progress = true;
         } else if (location == VARYING_SLOT_VIEW_INDEX) {
            msvpm_printf(", VARYING_SLOT_VIEW_INDEX");

            handle_view_indices(b, instr, intrin, state, lvl1_index.ssa, var);

            progress = true;
         } else if (location >= VARYING_SLOT_VAR0 && location <= VARYING_SLOT_VAR31) {
            msvpm_printf(", VARYING_SLOT_VAR%d", location - VARYING_SLOT_VAR0);
         } else if (location == VARYING_SLOT_POS) {
            msvpm_printf(", VARYING_SLOT_POS");
         }
      } else if (lvl2_deref->deref_type == nir_deref_type_array) {
         nir_src lvl2_index = lvl2_deref->arr.index;

         if (nir_src_is_const(lvl2_index)) {
            msvpm_printf(", inner array index: %" PRIu64,
                  nir_src_as_uint(lvl2_index));
         } else {
            msvpm_printf(", non-const inner array index");
         }

         nir_deref_instr *lvl3_deref = nir_src_as_deref(lvl2_deref->parent);
         if (lvl3_deref->deref_type == nir_deref_type_var) {
            const nir_variable *var = lvl3_deref->var;
            int location = var->data.location;

            msvpm_printf(", location: %d", location);
            if (var->data.per_view)
               msvpm_printf(", per_view");
            if (var->data.per_primitive)
               msvpm_printf(", per_primitive");

            if (location == VARYING_SLOT_POS) {
               msvpm_printf(", VARYING_SLOT_POS");
            } else if (location >= VARYING_SLOT_VAR0 &&
                       location <= VARYING_SLOT_VAR31) {
               unsigned var_ind = location - VARYING_SLOT_VAR0;
               msvpm_printf(", VARYING_SLOT_VAR%d", var_ind);
            } else if (location == VARYING_SLOT_VIEWPORT_MASK) {
               msvpm_printf(", VARYING_SLOT_VIEWPORT_MASK");
               handle_viewport_mask(b, instr, intrin, state,
                                    lvl1_index.ssa,
                                    lvl2_index.ssa,
                                    var);

               progress = true;
            } else {
               assert(!"unhandled lvl3 var location");
            }
         } else if (lvl3_deref->deref_type == nir_deref_type_array) {
            nir_src lvl3_index = lvl3_deref->arr.index;

            if (nir_src_is_const(lvl3_index)) {
               msvpm_printf(", inner array index: %" PRIu64,
                     nir_src_as_uint(lvl3_index));
            } else {
               msvpm_printf(", non-const inner array index");
            }

            nir_deref_instr *lvl4_deref = nir_src_as_deref(lvl3_deref->parent);
            if (lvl4_deref->deref_type == nir_deref_type_var) {
               const nir_variable *var = lvl4_deref->var;
               int location = var->data.location;

               msvpm_printf(", location: %d", location);
               if (var->data.per_view)
                  msvpm_printf(", per_view");
               if (var->data.per_primitive)
                  msvpm_printf(", per_primitive");

               if (location == VARYING_SLOT_CLIP_DIST0) {
                  msvpm_printf(", VARYING_SLOT_CLIP_DIST0");
               } else {
                  assert(!"unhandled lvl4 var location");
               }
            } else {
               assert(!"unhandled lvl4 deref type");
            }
         } else {
            assert(!"unhandled lvl3 deref type");
         }
      } else {
         assert(!"unhandled lvl2 deref type");
      }
      msvpm_printf("\n");
      break;
   }
   default:
      return false;
   }

   return progress;
}

/*
 * We don't have hardware support for ViewportMask, so to support it we have
 * to duplicate primitives num_viewports times, spread the data among those
 * primitives, set ViewportIndex for primitives that have corresponding bit
 * set in ViewportMask and somehow disable primitives that don't have
 * a corresponding bit set in ViewportMask.
 *
 * To disable primitives, we use Cull Primitive Mask field in the MUE
 * Primitive Header, which contains bitmask of primitives to remove when
 * Primitive Replication is used. We don't use the full feature here, but we
 * can use bit 0 of this mask to cull unneeded primitives.
 *
 * Unfortunately we don't know how many viewports are enabled (they can be
 * changed after shader is compiled), so we have to assume it has a max value.

 * TODO: Pass the number of viewports using push constants, if this feature
 * is actually used.
 */
bool
anv_nir_mesh_lower_viewport_mask(nir_shader *nir,
                                 struct anv_graphics_pipeline *pipeline)
{
   if (!(nir->info.outputs_written & BITFIELD64_BIT(VARYING_SLOT_VIEWPORT_MASK)))
      return false;

   struct mesh_lower_viewport_mask_state state;

   memset(&state, 0, sizeof(state));
   state.pipeline = pipeline;
   state.nir = nir;

   nir->info.mesh.max_primitives_out *= MAX_VIEWPORTS;
   state.viewport_count = MAX_VIEWPORTS;

   return nir_shader_instructions_pass(nir,
                                       anv_nir_mesh_lower_viewport_mask_instr,
                                       nir_metadata_none,
                                       &state);
}
