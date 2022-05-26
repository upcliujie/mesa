/*
 * Copyright © 2022 Valve Corporation
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

#include "nir/nir.h"
#include "nir/nir_builder.h"
#include "nir/nir_deref.h"

#include "radv_private.h"
#include "radv_shader.h"

typedef struct {
   unsigned topology;
   bool provoking_vtx_last;
} lower_fs_barycentric_state;

static nir_deref_instr *
clone_deref_instr(nir_builder *b, nir_variable *var, nir_deref_instr *deref)
{
   if (deref->deref_type == nir_deref_type_var)
      return nir_build_deref_var(b, var);

   nir_deref_instr *parent_deref = nir_deref_instr_parent(deref);
   nir_deref_instr *parent = clone_deref_instr(b, var, parent_deref);

   /* Build array and struct deref instruction.
    * "deref" instr is sure to be direct (see is_direct_uniform_load()).
    */
   switch (deref->deref_type) {
   case nir_deref_type_array: {
      nir_load_const_instr *index = nir_instr_as_load_const(deref->arr.index.ssa->parent_instr);
      return nir_build_deref_array_imm(b, parent, index->value->i64);
   }
   case nir_deref_type_ptr_as_array: {
      nir_load_const_instr *index = nir_instr_as_load_const(deref->arr.index.ssa->parent_instr);
      nir_ssa_def *ssa = nir_imm_intN_t(b, index->value->i64, parent->dest.ssa.bit_size);
      return nir_build_deref_ptr_as_array(b, parent, ssa);
   }
   case nir_deref_type_struct:
      return nir_build_deref_struct(b, parent, deref->strct.index);
   default:
      unreachable("invalid type");
      return NULL;
   }
}

static nir_intrinsic_instr *
clone_load_deref_instr(nir_builder *b, nir_intrinsic_instr *intrin)
{
   nir_deref_instr *deref = nir_src_as_deref(intrin->src[0]);
   nir_variable *var = nir_intrinsic_get_var(intrin, 0);
   nir_deref_instr *new_deref = clone_deref_instr(b, var, deref);

   nir_ssa_def *new_def = nir_build_load_deref(b, intrin->num_components, intrin->dest.ssa.bit_size,
                                               &new_deref->dest.ssa);

   return nir_instr_as_intrinsic(new_def->parent_instr);
}

static unsigned
get_new_vertex_id(unsigned vertex_id, lower_fs_barycentric_state *state, bool even)
{
   switch (state->topology) {
   case V_008958_DI_PT_TRILIST: {
      uint8_t indices[3] = {2, 0, 1};
      return indices[vertex_id];
   }
   case V_008958_DI_PT_TRIFAN: {
      if (state->provoking_vtx_last) {
         uint32_t even_indices[3] = {2, 0, 1};
         uint32_t odd_indices[3] = {1, 2, 0};

         return even ? even_indices[vertex_id] : odd_indices[vertex_id];
      } else {
         uint32_t even_indices[3] = {0, 1, 2};
         uint32_t odd_indices[3] = {2, 0, 1};

         return even ? even_indices[vertex_id] : odd_indices[vertex_id];
      }
      break;
   }
   case V_008958_DI_PT_TRISTRIP:
   case V_008958_DI_PT_TRISTRIP_ADJ: {
      if (state->provoking_vtx_last) {
         uint32_t even_indices[3] = {0, 1, 2};
         uint32_t odd_indices[3] = {1, 2, 0};

         return even ? even_indices[vertex_id] : odd_indices[vertex_id];
      } else {
         uint32_t even_indices[3] = {0, 1, 2};
         uint32_t odd_indices[3] = {2, 0, 1};

         return even ? even_indices[vertex_id] : odd_indices[vertex_id];
      }
      break;
   }
   case V_008958_DI_PT_TRILIST_ADJ: {
      uint32_t even_indices[3] = {0, 1, 2};
      uint32_t odd_indices[3] = {1, 2, 0};

      return even ? even_indices[vertex_id] : odd_indices[vertex_id];
   }
   default:
      unreachable("Invalid primitive topology");
   }
}

static void
rewrite_vertex_id(nir_builder *b, nir_intrinsic_instr *intrin, lower_fs_barycentric_state *state,
                  bool even)
{
   nir_deref_instr *deref = nir_src_as_deref(intrin->src[0]);

   nir_deref_path path;
   nir_deref_path_init(&path, deref, NULL);

   assert(path.path[0]->deref_type == nir_deref_type_var);
   nir_deref_instr **p = &path.path[1];

   assert((*p)->deref_type == nir_deref_type_array);
   nir_ssa_def *array_index = nir_ssa_for_src(b, (*p)->arr.index, 1);

   assert(array_index->parent_instr->type == nir_instr_type_load_const);
   nir_load_const_instr *load_const = nir_instr_as_load_const(array_index->parent_instr);
   unsigned vertex_id = load_const->value[0].u32;

   b->cursor = nir_before_instr(&(*p)->instr);

   unsigned new_vertex_id = get_new_vertex_id(vertex_id, state, even);

   nir_deref_instr *new_def = nir_build_deref_array(b, path.path[0], nir_imm_int(b, new_vertex_id));

   nir_instr *instr = &intrin->instr;
   nir_src *src = &intrin->src[0];
   if (path.path[2]) {
      instr = &path.path[2]->instr;
      src = &path.path[2]->parent;
   }

   nir_instr_rewrite_src(instr, src, nir_src_for_ssa(&new_def->dest.ssa));

   nir_deref_path_finish(&path);
}

static nir_variable *
get_primitive_id_var(nir_shader *shader)
{
   nir_variable *var =
      nir_find_variable_with_location(shader, nir_var_shader_in, VARYING_SLOT_PRIMITIVE_ID);
   if (!var) {
      var = nir_variable_create(shader, nir_var_shader_in, glsl_int_type(), "prim id");
      var->data.per_primitive = shader->info.stage == MESA_SHADER_MESH;
      var->data.location = VARYING_SLOT_PRIMITIVE_ID;
      var->data.interpolation = INTERP_MODE_FLAT;

      /* Update inputs_read to reflect that the pass added a new input. */
      shader->info.inputs_read |= VARYING_BIT_PRIMITIVE_ID;
   }

   return var;
}

static bool
lower_load_deref(nir_builder *b, lower_fs_barycentric_state *state, nir_intrinsic_instr *intrin)
{
   nir_variable *var = nir_intrinsic_get_var(intrin, 0);
   if (var->data.mode != nir_var_shader_in)
      return false;

   if (!var->data.per_vertex)
      return false;

   if (state->topology == V_008958_DI_PT_TRILIST) {
      rewrite_vertex_id(b, intrin, state, true);
   } else if (state->topology == V_008958_DI_PT_TRIFAN ||
              state->topology == V_008958_DI_PT_TRISTRIP ||
              state->topology == V_008958_DI_PT_TRISTRIP_ADJ ||
              state->topology == V_008958_DI_PT_TRILIST_ADJ) {
      b->cursor = nir_before_instr(&intrin->instr);

      nir_intrinsic_instr *cloned_intrin = clone_load_deref_instr(b, intrin);

      rewrite_vertex_id(b, intrin, state, true);
      rewrite_vertex_id(b, cloned_intrin, state, false);

      b->cursor = nir_after_instr(&intrin->instr);

      nir_variable *prim_id_var = get_primitive_id_var(b->shader);
      nir_ssa_def *prim_id = nir_load_var(b, prim_id_var);

      /* result = (prim_id % 2) == 0 ? even_res : odd_res */
      nir_ssa_def *cond = nir_ieq(b, nir_iand(b, prim_id, nir_imm_int(b, 1)), nir_imm_int(b, 0));
      nir_ssa_def *new_dest = nir_bcsel(b, cond, &intrin->dest.ssa, &cloned_intrin->dest.ssa);

      nir_ssa_def_rewrite_uses_after(&intrin->dest.ssa, new_dest, new_dest->parent_instr);
   }

   return true;
}

static bool
lower_load_barycentric_coord(nir_builder *b, lower_fs_barycentric_state *state,
                             nir_intrinsic_instr *intrin)
{
   enum glsl_interp_mode mode = (enum glsl_interp_mode)nir_intrinsic_interp_mode(intrin);
   bool linear_interp = mode == INTERP_MODE_NOPERSPECTIVE;

   nir_ssa_def *prim_id = NULL, *p1 = NULL, *p2 = NULL;
   nir_ssa_def *coords[3];

   b->cursor = nir_after_instr(&intrin->instr);

   /* Triangle primitive topologies need to load the primitive ID to compute the barycentric
    * coordinates.
    */
   if (state->topology == V_008958_DI_PT_TRIFAN || state->topology == V_008958_DI_PT_TRILIST_ADJ ||
       state->topology == V_008958_DI_PT_TRISTRIP ||
       state->topology == V_008958_DI_PT_TRISTRIP_ADJ) {
      nir_variable *prim_id_var = get_primitive_id_var(b->shader);
      prim_id = nir_load_var(b, prim_id_var);
   }

   /* All primitive topologies (except POINT_LIST) need to load the linear/perspective center
    * interpolate shader argument to compute the barycentric coordinates.
    */
   if (state->topology != V_008958_DI_PT_POINTLIST) {
      nir_ssa_def *interp =
         linear_interp ? nir_load_linear_center_interp_amd(b) : nir_load_persp_center_interp_amd(b);
      p1 = nir_channel(b, interp, 0);
      p2 = nir_channel(b, interp, 1);
   }

   switch (state->topology) {
   case V_008958_DI_PT_POINTLIST:
      coords[0] = nir_imm_float(b, 1.0f);
      coords[1] = nir_imm_float(b, 0.0f);
      coords[2] = nir_imm_float(b, 0.0f);
      break;
   case V_008958_DI_PT_LINELIST:
   case V_008958_DI_PT_LINELIST_ADJ:
   case V_008958_DI_PT_LINESTRIP_ADJ:
   case V_008958_DI_PT_LINESTRIP:
      coords[0] = nir_fsub(b, nir_fsub(b, nir_imm_float(b, 1.0f), p1), p2);
      coords[1] = nir_fadd(b, p1, p2);
      coords[2] = nir_imm_float(b, 0.0f);
      break;
   case V_008958_DI_PT_TRILIST:
      coords[0] = p2;
      coords[1] = nir_fsub(b, nir_fsub(b, nir_imm_float(b, 1.0f), p1), p2);
      coords[2] = p1;
      break;
   case V_008958_DI_PT_TRILIST_ADJ:
   case V_008958_DI_PT_TRIFAN:
   case V_008958_DI_PT_TRISTRIP:
   case V_008958_DI_PT_TRISTRIP_ADJ: {
      nir_ssa_def *k_coord = nir_fsub(b, nir_fsub(b, nir_imm_float(b, 1.0f), p1), p2);
      nir_ssa_def *odd_coords[3], *even_coords[3];

      if (state->topology == V_008958_DI_PT_TRIFAN || state->topology == V_008958_DI_PT_TRISTRIP ||
          state->topology == V_008958_DI_PT_TRISTRIP_ADJ) {
         if (state->provoking_vtx_last) {
            odd_coords[0] = p1;
            odd_coords[1] = p2;
            odd_coords[2] = k_coord;
            if (state->topology == V_008958_DI_PT_TRISTRIP ||
                state->topology == V_008958_DI_PT_TRISTRIP_ADJ) {
               even_coords[0] = k_coord;
               even_coords[1] = p1;
               even_coords[2] = p2;
            } else {
               assert(state->topology == V_008958_DI_PT_TRIFAN);
               even_coords[0] = p2;
               even_coords[1] = k_coord;
               even_coords[2] = p1;
            }
         } else {
            odd_coords[0] = p2;
            odd_coords[1] = k_coord;
            odd_coords[2] = p1;
            even_coords[0] = k_coord;
            even_coords[1] = p1;
            even_coords[2] = p2;
         }
      } else {
         assert(state->topology == V_008958_DI_PT_TRILIST_ADJ);
         odd_coords[0] = p1;
         odd_coords[1] = p2;
         odd_coords[2] = k_coord;
         even_coords[0] = k_coord;
         even_coords[1] = p1;
         even_coords[2] = p2;
      }

      /* result = (prim_id % 2) == 0 ? even_coords : odd_coords */
      nir_ssa_def *cond = nir_ieq(b, nir_iand(b, prim_id, nir_imm_int(b, 1)), nir_imm_int(b, 0));
      for (unsigned i = 0; i < 3; i++) {
         coords[i] = nir_bcsel(b, cond, even_coords[i], odd_coords[i]);
      }
      break;
   }
   default:
      unreachable("Invalid primitive topology");
   }

   nir_ssa_def *res = nir_vec(b, coords, 3);

   nir_ssa_def_rewrite_uses(&intrin->dest.ssa, res);
   nir_instr_remove(&intrin->instr);

   return true;
}

bool
radv_nir_lower_fs_barycentric(nir_shader *shader, const struct radv_pipeline_key *pipeline_key)
{
   nir_function_impl *impl = nir_shader_get_entrypoint(shader);
   bool progress = false;

   nir_builder b;

   lower_fs_barycentric_state state = {
      .topology = pipeline_key->vs.topology,
      .provoking_vtx_last = pipeline_key->vs.provoking_vtx_last,
   };

   nir_foreach_function (function, shader) {
      if (!function->impl)
         continue;

      nir_builder_init(&b, function->impl);

      nir_foreach_block (block, impl) {
         nir_foreach_instr_safe (instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
            if (intrin->intrinsic == nir_intrinsic_load_deref) {
               progress |= lower_load_deref(&b, &state, intrin);
            } else if (intrin->intrinsic == nir_intrinsic_load_barycentric_coord) {
               progress |= lower_load_barycentric_coord(&b, &state, intrin);
            }
         }
      }
   }

   if (progress)
      nir_metadata_preserve(impl, nir_metadata_block_index | nir_metadata_dominance);
   else
      nir_metadata_preserve(impl, nir_metadata_all);

   return progress;
}
