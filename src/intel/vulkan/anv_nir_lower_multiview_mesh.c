/*
 * Copyright © 2021 Intel Corporation
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

#define DEBUG_MS_MV 0

#if DEBUG_MS_MV
#define msmv_printf(...) fprintf(stderr, __VA_ARGS__)
#else
static inline void
msmv_printf(const char *format, ...)
{
   (void)format;
}
#endif

struct lower_mesh_view_state {
   struct anv_graphics_pipeline *pipeline;
   unsigned view_indices[MAX_VIEWS];
   unsigned view_count;
};

static bool
anv_nir_lower_mesh_view_filter(const nir_instr *instr,
                               UNUSED const void *data)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   if (intrin->intrinsic == nir_intrinsic_load_mesh_view_count)
      return true;
   if (intrin->intrinsic == nir_intrinsic_load_mesh_view_indices) {
      const nir_src *src0 = &intrin->src[0];
      if (!src0->is_ssa)
         return false;
      nir_ssa_def *ssa = src0->ssa;
      nir_instr *instr = ssa->parent_instr;
      return instr->type == nir_instr_type_load_const;
   }

   return false;
}

static nir_ssa_def *
anv_nir_lower_mesh_view_instr(nir_builder *b, nir_instr *instr, void *data)
{
   assert(instr->type == nir_instr_type_intrinsic);

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   struct lower_mesh_view_state *state = data;

   if (intrin->intrinsic == nir_intrinsic_load_mesh_view_count) {
      return nir_imm_int(b, state->view_count);
   } else {
      assert(intrin->intrinsic == nir_intrinsic_load_mesh_view_indices);
      const nir_src *src0 = &intrin->src[0];
      assert(src0->is_ssa);
      nir_ssa_def *ssa = src0->ssa;
      nir_instr *instr = ssa->parent_instr;
      nir_load_const_instr *load_const_instr = nir_instr_as_load_const(instr);
      assert(load_const_instr->def.num_components == 1);
      assert(load_const_instr->def.bit_size == 32);
      uint32_t ind = load_const_instr->value[0].u32;

      assert(ind < ARRAY_SIZE(state->view_indices));
      return nir_imm_int(b, state->view_indices[ind]);
   }
}

bool
anv_nir_lower_mesh_view(nir_shader *nir,
                        struct anv_graphics_pipeline *pipeline)
{
   struct lower_mesh_view_state state;

   memset(&state, 0, sizeof(state));
   state.pipeline = pipeline;
   state.view_count = anv_gfx_pipeline_view_count(pipeline);

   unsigned i = 0;
   u_foreach_bit(view_idx, pipeline->view_mask)
      state.view_indices[i++] = view_idx;
   assert(i <= ARRAY_SIZE(state.view_indices));

   return nir_shader_lower_instructions(nir,
                                        anv_nir_lower_mesh_view_filter,
                                        anv_nir_lower_mesh_view_instr,
                                        &state);
}

struct lower_mesh_multiview_state {
   struct anv_graphics_pipeline *pipeline;
   nir_shader *nir;
   nir_variable *primitive_indices;
   nir_variable *position;
   nir_variable *clip_distance;
   nir_variable *var[32][4];
   nir_variable *view_var;
   nir_variable *viewport_mask;
   nir_variable *layer;
   unsigned view_indices[MAX_VIEWS];
};

static void
handle_primitive_count(struct nir_builder *b,
                       nir_instr *instr,
                       nir_intrinsic_instr *intrin,
                       unsigned view_count,
                       struct lower_mesh_multiview_state *state)
{
   /*
    * for (int prim = 0; prim < gl_PrimitiveCountNV; ++prim)
    *    for (int view = 0; view < gl_MeshViewCountNV; ++view)
    *       ViewID[view * gl_PrimitiveCountNV + prim] := gl_MeshViewIndicesNV[view];
    * gl_PrimitiveCountNV *= gl_MeshViewCountNV;
    */

   b->cursor = nir_before_instr(instr);
   nir_ssa_def *new_primitive_count =
         nir_imul_imm(b, intrin->src[1].ssa, view_count);

   unsigned prim_count;
   if (nir_src_is_const(intrin->src[1])) {
      prim_count = nir_src_as_uint(intrin->src[1]);
   } else {
      /* TODO: optimize? */
      prim_count = state->nir->info.mesh.max_primitives_out;
   }

   const struct glsl_type *view_id_type =
         glsl_array_type(glsl_int_type(), prim_count * view_count, 0);

   nir_variable *viewId =
         nir_variable_create(b->shader,
                             nir_var_shader_out,
                             view_id_type,
                             "GeneratedViewID");
   viewId->data.location = VARYING_SLOT_VIEW_INDEX;
   viewId->data.interpolation = INTERP_MODE_NONE;
   viewId->data.per_primitive = 1;

   nir_deref_instr *viewId_deref = nir_build_deref_var(b, viewId);

   for (unsigned view = 0; view < view_count; ++view) {
      for (unsigned prim = 0; prim < prim_count; ++prim) {
         nir_ssa_def *new_prim_idx = nir_imm_int(b, prim * view_count + view);

         nir_deref_instr *viewId_indexed =
               nir_build_deref_array(b, viewId_deref, new_prim_idx);

         assert(view < ARRAY_SIZE(state->view_indices));
         nir_ssa_def *view_def = nir_imm_int(b, state->view_indices[view]);

         nir_store_deref(b, viewId_indexed, view_def, 1);
      }
   }

   /* must be modified after src[1] is read */
   nir_instr_rewrite_src_ssa(instr, &intrin->src[1], new_primitive_count);
}

static void
handle_primitive_indices(struct nir_builder *b,
                         nir_instr *instr,
                         nir_intrinsic_instr *intrin,
                         unsigned view_count,
                         struct lower_mesh_multiview_state *state,
                         nir_ssa_def *ind,
                         const nir_variable *var)
{
   /*
    * Replace:
    * gl_PrimitiveIndicesNV[i] := vtx;
    *
    * By:
    * for (int view = 0; view < gl_MeshViewCountNV; ++view) {
    *     gl_PrimitiveIndicesNV[i / VERT_PER_PRIM * gl_MeshViewCountNV * VERT_PER_PRIM +
    *                           i % VERT_PER_PRIM +
    *                           view * VERT_PER_PRIM] := vtx * gl_MeshViewCountNV + view;
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
                            view_count * vertices_per_primitive,
                            0);

      state->primitive_indices =
            nir_variable_create(b->shader,
                                nir_var_shader_out,
                                type,
                                "gl_PrimitiveIndicesNV");
      state->primitive_indices->data.location = var->data.location;
      state->primitive_indices->data.interpolation = var->data.interpolation;
   }

   nir_deref_instr *primitive_indices_deref = nir_build_deref_var(b, state->primitive_indices);

   nir_ssa_def *view_count_def = nir_imm_int(b, view_count);
   nir_ssa_def *vert_per_prim = nir_imm_int(b, vertices_per_primitive);
   nir_ssa_def *ind_div = nir_idiv(b, ind, vert_per_prim);
   nir_ssa_def *ind_mod = nir_umod(b, ind, vert_per_prim);

   for (unsigned view = 0; view < view_count; ++view) {
      nir_ssa_def *view_def = nir_imm_int(b, view);

      nir_ssa_def *new_idx =
            nir_iadd3(b,
                      nir_imul(b,
                               nir_imul(b, ind_div, view_count_def),
                               vert_per_prim),
                      ind_mod,
                      nir_imul(b, view_def, vert_per_prim)
                     );

      nir_ssa_def *new_primitive_index =
            nir_iadd(b,
                     nir_imul(b, intrin->src[1].ssa, view_count_def),
                     view_def);

      nir_deref_instr *reindexed_deref =
            nir_build_deref_array(b, primitive_indices_deref, new_idx);

      nir_store_deref(b, reindexed_deref, new_primitive_index, writemask);
   }

   nir_instr_remove(instr);
}

static void
handle_var_lvl1(struct nir_builder *b,
                nir_instr *instr,
                nir_intrinsic_instr *intrin,
                unsigned view_count,
                struct lower_mesh_multiview_state *state,
                nir_ssa_def *ind,
                const nir_variable *var,
                unsigned var_ind)
{
   /*
    * Replace:
    * var[ind] := value;
    *
    * By:
    * for (int view = 0; view < gl_MeshViewCountNV; ++view)
    *    var[ind * gl_MeshViewCountNV + view] := value;
    *
    */
   unsigned writemask = nir_intrinsic_write_mask(intrin);
   unsigned loc_frac = var->data.location_frac;
   assert(loc_frac < 4);

   b->cursor = nir_before_instr(instr);

   nir_ssa_def *view_count_def = nir_imm_int(b, view_count);

   nir_variable **new_var_loc = &state->var[var_ind][loc_frac];
   nir_variable *new_var;

   if (*new_var_loc) {
      new_var = *new_var_loc;
   } else {
      const struct glsl_type *type =
            glsl_array_type(glsl_without_array(var->type),
                            state->nir->info.mesh.max_vertices_out * view_count,
                            0);

      *new_var_loc = nir_variable_create(b->shader,
                                         nir_var_shader_out,
                                         type,
                                         var->name);
      new_var = *new_var_loc;

      new_var->data.location      = var->data.location;
      new_var->data.location_frac = var->data.location_frac;
      new_var->data.interpolation = var->data.interpolation;
      new_var->data.per_primitive = var->data.per_primitive;
   }

   nir_deref_instr *var_deref = nir_build_deref_var(b, new_var);

   for (unsigned view = 0; view < view_count; ++view) {
      nir_ssa_def *view_def = nir_imm_int(b, view);

      nir_ssa_def *new_idx =
            nir_iadd(b, nir_imul(b, ind, view_count_def), view_def);

      nir_deref_instr *reindexed_deref =
            nir_build_deref_array(b, var_deref, new_idx);

      nir_store_deref(b, reindexed_deref, intrin->src[1].ssa, writemask);
   }

   nir_instr_remove(instr);
}

static void
handle_position(struct nir_builder *b,
                nir_instr *instr,
                nir_intrinsic_instr *intrin,
                unsigned view_count,
                struct lower_mesh_multiview_state *state,
                nir_ssa_def *view,
                nir_ssa_def *vertex,
                const nir_variable *var)
{
   /*
    * Replace:
    * gl_MeshVerticesNV[vertex].gl_PositionPerViewNV[view] := XYZW
    *
    * By:
    * gl_MeshVerticesNV[vertex * gl_MeshViewCountNV + view].gl_Position := XYZW
    */

   b->cursor = nir_before_instr(instr);

   if (!state->position) {
      const struct glsl_type *type =
            glsl_array_type(glsl_vec4_type(),
                            state->nir->info.mesh.max_vertices_out * view_count,
                            0);

      state->position =
            nir_variable_create(b->shader,
                                nir_var_shader_out,
                                type,
                                "gl_MeshVerticesNV[*].gl_Position");
      state->position->data.location = var->data.location;
      state->position->data.interpolation = var->data.interpolation;
   }

   nir_deref_instr *position_deref = nir_build_deref_var(b, state->position);

   nir_ssa_def *new_vtx_idx =
         nir_iadd(b, nir_imul(b, vertex, nir_imm_int(b, view_count)), view);

   nir_deref_instr *position_vtx_indexed =
         nir_build_deref_array(b, position_deref, new_vtx_idx);

   nir_instr_rewrite_src_ssa(instr,
                             &intrin->src[0],
                             &position_vtx_indexed->dest.ssa);
}

static void
handle_layer_lvl1(struct nir_builder *b,
                nir_instr *instr,
                nir_intrinsic_instr *intrin,
                unsigned view_count,
                struct lower_mesh_multiview_state *state,
                nir_ssa_def *prim,
                const nir_variable *var)
{
   /*
    * Replace:
    * gl_MeshPrimitivesNV[prim].gl_Layer := layer
    *
    * By:
    * for (int view = 0; view < gl_MeshViewCountNV; ++view)
    *     gl_MeshPrimitivesNV[prim * gl_MeshViewCountNV + view].gl_Layer := layer
    */

   b->cursor = nir_before_instr(instr);

   if (!state->layer) {
      const struct glsl_type *type =
            glsl_array_type(glsl_int_type(),
                            state->nir->info.mesh.max_primitives_out * view_count,
                            0);

      state->layer =
            nir_variable_create(b->shader,
                                nir_var_shader_out,
                                type,
                                "gl_MeshPrimitivesNV[*].gl_Layer");
      state->layer->data.location = var->data.location;
      state->layer->data.interpolation = var->data.interpolation;
      state->layer->data.per_primitive = 1;
   }

   nir_deref_instr *layer_deref = nir_build_deref_var(b, state->layer);
   nir_ssa_def *view_count_def = nir_imm_int(b, view_count);
   unsigned writemask = nir_intrinsic_write_mask(intrin);

   for (unsigned view = 0; view < view_count; ++view) {
      nir_ssa_def *view_def = nir_imm_int(b, view);

      nir_ssa_def *new_prim_idx =
            nir_iadd(b, nir_imul(b, prim, view_count_def), view_def);

      nir_deref_instr *layer_prim_indexed =
            nir_build_deref_array(b, layer_deref, new_prim_idx);

      nir_store_deref(b, layer_prim_indexed, intrin->src[1].ssa, writemask);
   }

   nir_instr_remove(instr);
}

static void
handle_layer_lvl2(struct nir_builder *b,
                nir_instr *instr,
                nir_intrinsic_instr *intrin,
                unsigned view_count,
                struct lower_mesh_multiview_state *state,
                nir_ssa_def *view,
                nir_ssa_def *prim,
                const nir_variable *var)
{
   /*
    * Replace:
    * gl_MeshPrimitivesNV[prim].gl_LayerPerViewNV[view] := layer
    *
    * By:
    * gl_MeshPrimitivesNV[prim * gl_MeshViewCountNV + view].gl_Layer := layer
    */

   b->cursor = nir_before_instr(instr);

   if (!state->layer) {
      const struct glsl_type *type =
            glsl_array_type(glsl_int_type(),
                            state->nir->info.mesh.max_primitives_out * view_count,
                            0);

      state->layer =
            nir_variable_create(b->shader,
                                nir_var_shader_out,
                                type,
                                "gl_MeshPrimitivesNV[*].gl_Layer");
      state->layer->data.location = var->data.location;
      state->layer->data.interpolation = var->data.interpolation;
      state->layer->data.per_primitive = 1;
   }

   nir_deref_instr *layer_deref = nir_build_deref_var(b, state->layer);

   nir_ssa_def *new_prim_idx =
         nir_iadd(b, nir_imul(b, prim, nir_imm_int(b, view_count)), view);

   nir_deref_instr *layer_prim_indexed =
         nir_build_deref_array(b, layer_deref, new_prim_idx);

   nir_instr_rewrite_src_ssa(instr,
                             &intrin->src[0],
                             &layer_prim_indexed->dest.ssa);
}

static void
handle_clip_distance(struct nir_builder *b,
                     nir_instr *instr,
                     nir_intrinsic_instr *intrin,
                     unsigned view_count,
                     struct lower_mesh_multiview_state *state,
                     nir_ssa_def *plane,
                     nir_ssa_def *view,
                     nir_ssa_def *vertex,
                     const nir_variable *var)
{
   /*
    * Replace:
    * gl_MeshVerticesNV[vertex].gl_ClipDistancePerViewNV[view][plane] := value
    *
    * By:
    * gl_MeshVerticesNV[vertex * gl_MeshViewCountNV + view].gl_ClipDistance[plane] := value
    */

   b->cursor = nir_before_instr(instr);

   if (!state->clip_distance) {
      unsigned clip_size = state->nir->info.clip_distance_array_size +
                           state->nir->info.cull_distance_array_size;

      const struct glsl_type *clip_dist_type =
            glsl_array_type(glsl_float_type(), clip_size, 0);

      const struct glsl_type *type =
            glsl_array_type(clip_dist_type,
                            state->nir->info.mesh.max_vertices_out * view_count,
                            0);

      state->clip_distance =
            nir_variable_create(b->shader,
                                nir_var_shader_out,
                                type,
                                "gl_MeshVerticesNV[*].gl_ClipDistance");
      state->clip_distance->data.location = var->data.location;
      state->clip_distance->data.interpolation = var->data.interpolation;
   }

   nir_deref_instr *clip_dist_deref = nir_build_deref_var(b, state->clip_distance);

   nir_ssa_def *new_vtx_idx =
         nir_iadd(b, nir_imul(b, vertex, nir_imm_int(b, view_count)), view);

   nir_deref_instr *clip_dist_vtx_indexed =
         nir_build_deref_array(b, clip_dist_deref, new_vtx_idx);

   nir_deref_instr *clip_dist_indexed =
         nir_build_deref_array(b, clip_dist_vtx_indexed, plane);

   nir_instr_rewrite_src_ssa(instr,
                             &intrin->src[0],
                             &clip_dist_indexed->dest.ssa);
}

static void
handle_viewport_mask(struct nir_builder *b,
                     nir_instr *instr,
                     nir_intrinsic_instr *intrin,
                     unsigned view_count,
                     struct lower_mesh_multiview_state *state,
                     nir_ssa_def *ind,
                     nir_ssa_def *view,
                     nir_ssa_def *prim,
                     const nir_variable *var,
                     unsigned viewport_mask_length)
{
   /*
    * Replace:
    * gl_MeshPrimitivesNV[prim].gl_ViewportMaskPerViewNV[view][ind] := value
    *
    * By:
    * gl_MeshPrimitivesNV[prim * gl_MeshViewCountNV + view].gl_ViewportMask[ind] := value
    */

   b->cursor = nir_before_instr(instr);

   if (!state->viewport_mask) {
      const struct glsl_type *viewport_mask_type =
            glsl_array_type(glsl_int_type(), viewport_mask_length, 0);

      const struct glsl_type *type =
            glsl_array_type(viewport_mask_type,
                            state->nir->info.mesh.max_primitives_out * view_count,
                            0);

      state->viewport_mask =
            nir_variable_create(b->shader,
                                nir_var_shader_out,
                                type,
                                "gl_MeshPrimitivesNV[*].gl_ViewportMask");
      state->viewport_mask->data.location = var->data.location;
      state->viewport_mask->data.interpolation = var->data.interpolation;
      state->viewport_mask->data.per_primitive = 1;
   }

   nir_deref_instr *viewport_mask_deref = nir_build_deref_var(b, state->viewport_mask);

   nir_ssa_def *new_prim_idx =
         nir_iadd(b, nir_imul(b, prim, nir_imm_int(b, view_count)), view);

   nir_deref_instr *viewport_mask_prim_indexed =
         nir_build_deref_array(b, viewport_mask_deref, new_prim_idx);

   nir_deref_instr *viewport_mask_indexed =
         nir_build_deref_array(b, viewport_mask_prim_indexed, ind);

   nir_instr_rewrite_src_ssa(instr,
                             &intrin->src[0],
                             &viewport_mask_indexed->dest.ssa);
}

static void
handle_var_lvl2(struct nir_builder *b,
                nir_instr *instr,
                nir_intrinsic_instr *intrin,
                unsigned view_count,
                struct lower_mesh_multiview_state *state,
                nir_ssa_def *view,
                nir_ssa_def *ind,
                const nir_variable *var,
                unsigned var_ind)
{
   /*
    * Replace:
    * var[ind][view] := value
    *
    * By:
    * var[ind * gl_MeshViewCountNV + view] := value
    */

   unsigned loc_frac = var->data.location_frac;
   assert(loc_frac < 4);

   b->cursor = nir_before_instr(instr);

   nir_variable **new_var_loc = &state->var[var_ind][loc_frac];
   nir_variable *new_var;

   if (*new_var_loc) {
      new_var = *new_var_loc;
   } else {
      const struct glsl_type *type =
            glsl_array_type(glsl_without_array(var->type),
                            state->nir->info.mesh.max_vertices_out * view_count,
                            0);

      *new_var_loc = nir_variable_create(b->shader,
                                         nir_var_shader_out,
                                         type,
                                         var->name);
      new_var = *new_var_loc;

      new_var->data.location      = var->data.location;
      new_var->data.location_frac = var->data.location_frac;
      new_var->data.interpolation = var->data.interpolation;
      new_var->data.per_primitive = var->data.per_primitive;
   }

   nir_deref_instr *var_deref = nir_build_deref_var(b, new_var);

   nir_ssa_def *new_vtx_idx =
         nir_iadd(b, nir_imul(b, ind, nir_imm_int(b, view_count)), view);

   nir_deref_instr *var_vtx_indexed =
         nir_build_deref_array(b, var_deref, new_vtx_idx);

   nir_instr_rewrite_src_ssa(instr,
                             &intrin->src[0],
                             &var_vtx_indexed->dest.ssa);
}

static bool
anv_nir_lower_mesh_multiview_instr(struct nir_builder *b,
                                    nir_instr *instr,
                                    void *cb_data)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   if (intrin->intrinsic != nir_intrinsic_store_deref)
      return false;

   nir_deref_instr *lvl1_deref = nir_src_as_deref(intrin->src[0]);

   struct lower_mesh_multiview_state *state = cb_data;
   const unsigned view_count = anv_gfx_pipeline_view_count(state->pipeline);
   bool progress = false;

   switch (lvl1_deref->deref_type) {
   case nir_deref_type_var: {
      const nir_variable *var = lvl1_deref->var;
      msmv_printf("location: %d", var->data.location);

      if (var->data.location == VARYING_SLOT_PRIMITIVE_COUNT) {
         msmv_printf(", VARYING_SLOT_PRIMITIVE_COUNT");

         handle_primitive_count(b, instr, intrin, view_count, state);

         progress = true;
      } else {
         assert(!"unhandled lvl1 var location");
      }

      msmv_printf("\n");
      break;
   }
   case nir_deref_type_array: {
      nir_src lvl1_index = lvl1_deref->arr.index;

      if (nir_src_is_const(lvl1_index))
         msmv_printf("array index: %" PRIu64, nir_src_as_uint(lvl1_index));
      else
         msmv_printf("non-const array index");

      nir_deref_instr *lvl2_deref = nir_src_as_deref(lvl1_deref->parent);
      if (lvl2_deref->deref_type == nir_deref_type_var) {
         const nir_variable *var = lvl2_deref->var;
         int location = var->data.location;

         msmv_printf(", location: %d", location);

         if (var->data.per_view)
            msmv_printf(", per_view");
         if (var->data.per_primitive)
            msmv_printf(", per_primitive");

         if (location == VARYING_SLOT_PRIMITIVE_INDICES) {
            msmv_printf(", VARYING_SLOT_PRIMITIVE_INDICES");

            handle_primitive_indices(b, instr, intrin, view_count, state,
                                     lvl1_index.ssa, var);

            progress = true;
         } else if (location >= VARYING_SLOT_VAR0 &&
                    location <= VARYING_SLOT_VAR31) {
            unsigned var_ind = location - VARYING_SLOT_VAR0;
            msmv_printf(", VARYING_SLOT_VAR%d", var_ind);

            handle_var_lvl1(b, instr, intrin, view_count, state,
                            lvl1_index.ssa, var, var_ind);

            progress = true;
         } else if (location == VARYING_SLOT_LAYER) {
            msmv_printf(", VARYING_SLOT_LAYER");

            handle_layer_lvl1(b, instr, intrin, view_count, state,
                              lvl1_index.ssa,
                              var);

            progress = true;
         } else {
            assert(!"unhandled lvl2 var location");
         }
      } else if (lvl2_deref->deref_type == nir_deref_type_array) {
         nir_src lvl2_index = lvl2_deref->arr.index;

         if (nir_src_is_const(lvl2_index)) {
            msmv_printf(", inner array index: %" PRIu64,
                  nir_src_as_uint(lvl2_index));
         } else {
            msmv_printf(", non-const inner array index");
         }

         nir_deref_instr *lvl3_deref = nir_src_as_deref(lvl2_deref->parent);
         if (lvl3_deref->deref_type == nir_deref_type_var) {
            const nir_variable *var = lvl3_deref->var;
            int location = var->data.location;

            msmv_printf(", location: %d", location);
            if (var->data.per_view)
               msmv_printf(", per_view");
            if (var->data.per_primitive)
               msmv_printf(", per_primitive");

            if (location == VARYING_SLOT_POS) {
               msmv_printf(", VARYING_SLOT_POS");

               handle_position(b, instr, intrin, view_count, state,
                               lvl1_index.ssa,
                               lvl2_index.ssa,
                               var);

               progress = true;
            } else if (location >= VARYING_SLOT_VAR0 &&
                       location <= VARYING_SLOT_VAR31) {
               unsigned var_ind = location - VARYING_SLOT_VAR0;
               msmv_printf(", VARYING_SLOT_VAR%d", var_ind);

               handle_var_lvl2(b, instr, intrin, view_count, state,
                               lvl1_index.ssa,
                               lvl2_index.ssa,
                               var,
                               var_ind);

               progress = true;
            } else if (location == VARYING_SLOT_LAYER) {
               msmv_printf(", VARYING_SLOT_LAYER");

               handle_layer_lvl2(b, instr, intrin, view_count, state,
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
               msmv_printf(", inner array index: %" PRIu64,
                     nir_src_as_uint(lvl3_index));
            } else {
               msmv_printf(", non-const inner array index");
            }

            nir_deref_instr *lvl4_deref = nir_src_as_deref(lvl3_deref->parent);
            if (lvl4_deref->deref_type == nir_deref_type_var) {
               const nir_variable *var = lvl4_deref->var;
               int location = var->data.location;

               msmv_printf(", location: %d", location);
               if (var->data.per_view)
                  msmv_printf(", per_view");
               if (var->data.per_primitive)
                  msmv_printf(", per_primitive");

               if (location == VARYING_SLOT_CLIP_DIST0) {
                  msmv_printf(", VARYING_SLOT_CLIP_DIST0");

                  handle_clip_distance(b, instr, intrin, view_count, state,
                                       lvl1_index.ssa,
                                       lvl2_index.ssa,
                                       lvl3_index.ssa,
                                       var);

                  progress = true;
               } else if (location == VARYING_SLOT_VIEWPORT_MASK) {
                  msmv_printf(", VARYING_SLOT_VIEWPORT_MASK");

                  handle_viewport_mask(b, instr, intrin, view_count, state,
                                       lvl1_index.ssa,
                                       lvl2_index.ssa,
                                       lvl3_index.ssa,
                                       var,
                                       glsl_get_length(lvl2_deref->type));

                  progress = true;
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

      msmv_printf("\n");
      break;
   }
   default:
      return false;
   }

   return progress;
}

/*
 * Since we don't have hardware support for per-view data in per-vertex
 * and per-primitive arrays (with one exception), we have to duplicate
 * vertices and primitives gl_MeshViewCountNV times, spread the data
 * among those vertices & primitives, and set View Id for each primitive
 * so that it looks like those per-view arrays actually exist.
 *
 * The only per-view array we have is for gl_PositionPerViewNV, but once
 * we decide to do this lowering, we are not going to use it.
 */
void
anv_nir_lower_mesh_multiview(nir_shader *nir,
                             struct anv_graphics_pipeline *pipeline)
{
   struct lower_mesh_multiview_state state;

   memset(&state, 0, sizeof(state));
   state.pipeline = pipeline;
   state.nir = nir;

   unsigned i = 0;
   u_foreach_bit(view_idx, pipeline->view_mask)
      state.view_indices[i++] = view_idx;
   assert(i <= ARRAY_SIZE(state.view_indices));

   nir_shader_instructions_pass(nir, anv_nir_lower_mesh_multiview_instr,
                                nir_metadata_none, &state);
   nir_shader_gather_info(nir, nir_shader_get_entrypoint(nir));
}
