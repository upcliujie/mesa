/*
 * Copyright © 2021 Valve Corporation
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
#include "nir_builder.h"
#include "ac_nir.h"
#include "radv_constants.h"
#include "radv_private.h"
#include "radv_shader.h"
#include "radv_shader_args.h"

struct radv_nir_abi_state
{
   gl_shader_stage stage;
   const struct radv_shader_args *args;
   const struct radv_shader_info *info;
   const struct radv_pipeline_key *pl_key;
   const nir_shader *nir;
};

static nir_ssa_def *
radv_nir_load_tess_offchip_descriptor(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   nir_ssa_def *ring_offsets = ac_nir_load_arg(b, &st->args->ac, st->args->ring_offsets);
   ring_offsets = nir_pack_64_2x32_split(b, nir_channel(b, ring_offsets, 0), nir_channel(b, ring_offsets, 1));
   return nir_build_load_smem_amd(b, 4, ring_offsets, nir_imm_int(b, RING_HS_TESS_OFFCHIP * 16u), .align_mul = 4u);
}

static nir_ssa_def *
radv_nir_load_tess_factors_descriptor(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   nir_ssa_def *ring_offsets = ac_nir_load_arg(b, &st->args->ac, st->args->ring_offsets);
   ring_offsets = nir_pack_64_2x32_split(b, nir_channel(b, ring_offsets, 0), nir_channel(b, ring_offsets, 1));
   return nir_build_load_smem_amd(b, 4, ring_offsets, nir_imm_int(b, RING_HS_TESS_FACTOR * 16u), .align_mul = 4u);
}

static nir_ssa_def *
radv_nir_load_tcs_num_patches(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   return nir_imm_int(b, st->info->num_tess_patches);
}

static nir_ssa_def *
radv_nir_load_tcs_in_patch_size(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   return nir_imm_int(b, st->pl_key->tcs.tess_input_vertices);
}

static nir_ssa_def *
radv_nir_load_tcs_out_patch_size(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   return nir_imm_int(b, st->nir->info.tess.tcs_vertices_out);
}

static nir_ssa_def *
radv_nir_load_esgs_ring_descriptor(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   nir_ssa_def *ring_offsets = ac_nir_load_arg(b, &st->args->ac, st->args->ring_offsets);
   ring_offsets = nir_pack_64_2x32_split(b, nir_channel(b, ring_offsets, 0), nir_channel(b, ring_offsets, 1));
   unsigned ring = st->stage == MESA_SHADER_GEOMETRY ? RING_ESGS_GS : RING_ESGS_VS;
   return nir_build_load_smem_amd(b, 4, ring_offsets, nir_imm_int(b, ring * 16u), .align_mul = 4u);
}

static nir_ssa_def *
radv_nir_shader_query_enabled(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   nir_ssa_def *ngg_gs_state = ac_nir_load_arg(b, &st->args->ac, st->args->ngg_gs_state);
   return nir_ieq_imm(b, ngg_gs_state, 1);
}

static nir_ssa_def *
radv_nir_nggc_bool_setting(nir_builder *b, const void *user, unsigned pattern)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   nir_ssa_def *settings = ac_nir_load_arg(b, &st->args->ac, st->args->ngg_culling_settings);
   nir_ssa_def *x = nir_iand_imm(b, settings, pattern);
   return nir_ine(b, x, nir_imm_int(b, 0));
}

static nir_ssa_def *
radv_nir_cull_any_enabled(nir_builder *b, const void *user)
{
   unsigned mask = radv_nggc_front_face | radv_nggc_back_face | radv_nggc_small_primitives;
   return radv_nir_nggc_bool_setting(b, user, mask);
}

static nir_ssa_def *
radv_nir_cull_front_face_enabled(nir_builder *b, const void *user)
{
   return radv_nir_nggc_bool_setting(b, user, radv_nggc_front_face);
}

static nir_ssa_def *
radv_nir_cull_back_face_enabled(nir_builder *b, const void *user)
{
   return radv_nir_nggc_bool_setting(b, user, radv_nggc_back_face);
}

static nir_ssa_def *
radv_nir_ccw(nir_builder *b, const void *user)
{
   return radv_nir_nggc_bool_setting(b, user, radv_nggc_face_is_ccw);
}

static nir_ssa_def *
radv_nir_cull_small_primitives_enabled(nir_builder *b, const void *user)
{
   return radv_nir_nggc_bool_setting(b, user, radv_nggc_small_primitives);
}

static nir_ssa_def *
radv_nir_small_primitive_precision(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;

   /* To save space, only the exponent is stored in the high 8 bits.
    * We calculate the precision from those 8 bits:
    * exponent = nggc_settings >> 24
    * 1.0 * 2 ^ exponent
    */
   nir_ssa_def *settings = ac_nir_load_arg(b, &st->args->ac, st->args->ngg_culling_settings);
   nir_ssa_def *exponent = nir_ishr_imm(b, settings, 24u);
   return nir_ldexp(b, nir_imm_float(b, 1.0f), exponent);
}

static nir_ssa_def *
radv_nir_viewport_x_scale(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   return ac_nir_load_arg(b, &st->args->ac, st->args->ngg_viewport_scale[0]);
}

static nir_ssa_def *
radv_nir_viewport_y_scale(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   return ac_nir_load_arg(b, &st->args->ac, st->args->ngg_viewport_scale[1]);
}

static nir_ssa_def *
radv_nir_viewport_x_offset(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   return ac_nir_load_arg(b, &st->args->ac, st->args->ngg_viewport_translate[0]);
}

static nir_ssa_def *
radv_nir_viewport_y_offset(nir_builder *b, const void *user)
{
   const struct radv_nir_abi_state *st = (const struct radv_nir_abi_state *) user;
   return ac_nir_load_arg(b, &st->args->ac, st->args->ngg_viewport_translate[1]);
}

static ac_nir_tess_io_abi radv_tess_io_abi = {
   .load_tess_offchip_descriptor = radv_nir_load_tess_offchip_descriptor,
   .load_tess_factors_descriptor = radv_nir_load_tess_factors_descriptor,
   .load_tcs_num_patches = radv_nir_load_tcs_num_patches,
   .load_tcs_in_patch_size = radv_nir_load_tcs_in_patch_size,
   .load_tcs_out_patch_size = radv_nir_load_tcs_out_patch_size,
};

static ac_nir_esgs_io_abi radv_esgs_io_abi = {
   .load_esgs_ring_descriptor = radv_nir_load_esgs_ring_descriptor,
};

static ac_nir_ngg_abi radv_ngg_abi = {
   .shader_query_enabled = radv_nir_shader_query_enabled,
   .cull = {
      .cull_front_face_enabled = radv_nir_cull_front_face_enabled,
      .cull_back_face_enabled = radv_nir_cull_back_face_enabled,
      .cull_small_primitives_enabled = radv_nir_cull_small_primitives_enabled,
      .cull_any_enabled = radv_nir_cull_any_enabled,
      .small_primitive_precision = radv_nir_small_primitive_precision,
      .ccw = radv_nir_ccw,
      .viewport_x_scale = radv_nir_viewport_x_scale,
      .viewport_y_scale = radv_nir_viewport_y_scale,
      .viewport_x_offset = radv_nir_viewport_x_offset,
      .viewport_y_offset = radv_nir_viewport_y_offset,
   },
};

bool
radv_lower_io_to_mem(struct radv_device *device, struct nir_shader *nir,
                     const struct radv_shader_info *info, const struct radv_pipeline_key *pl_key,
                     const struct radv_shader_args *args)
{
   struct radv_nir_abi_state abi_state = {
      .stage = nir->info.stage,
      .info = info,
      .args = args,
      .pl_key = pl_key,
      .nir = nir,
   };

   if (nir->info.stage == MESA_SHADER_VERTEX) {
      if (info->vs.as_ls) {
         ac_nir_lower_ls_outputs_to_mem(nir, info->vs.tcs_in_out_eq,
                                        info->vs.tcs_temp_only_input_mask,
                                        info->vs.num_linked_outputs,
                                        &args->ac, &radv_tess_io_abi, &abi_state);
         return true;
      } else if (info->vs.as_es) {
         ac_nir_lower_es_outputs_to_mem(nir, device->physical_device->rad_info.chip_class,
                                        info->vs.num_linked_outputs,
                                        &args->ac, &radv_esgs_io_abi, &abi_state);
         return true;
      }
   } else if (nir->info.stage == MESA_SHADER_TESS_CTRL) {
      ac_nir_lower_hs_inputs_to_mem(nir, info->vs.tcs_in_out_eq, info->tcs.num_linked_inputs,
                                    &args->ac, &radv_tess_io_abi, &abi_state);
      ac_nir_lower_hs_outputs_to_mem(
         nir, device->physical_device->rad_info.chip_class, info->tcs.tes_reads_tess_factors,
         info->tcs.tes_inputs_read, info->tcs.tes_patch_inputs_read, info->tcs.num_linked_inputs,
         info->tcs.num_linked_outputs, info->tcs.num_linked_patch_outputs, true,
         &args->ac, &radv_tess_io_abi, &abi_state);
      return true;
   } else if (nir->info.stage == MESA_SHADER_TESS_EVAL) {
      ac_nir_lower_tes_inputs_to_mem(nir, info->tes.num_linked_inputs,
                                     info->tes.num_linked_patch_inputs,
                                     &args->ac, &radv_tess_io_abi, &abi_state);
      if (info->tes.as_es) {
         ac_nir_lower_es_outputs_to_mem(nir, device->physical_device->rad_info.chip_class,
                                        info->tes.num_linked_outputs,
                                        &args->ac, &radv_esgs_io_abi, &abi_state);
      }

      return true;
   } else if (nir->info.stage == MESA_SHADER_GEOMETRY) {
      ac_nir_lower_gs_inputs_to_mem(nir, device->physical_device->rad_info.chip_class,
                                    info->gs.num_linked_inputs,
                                    &args->ac, &radv_esgs_io_abi, &abi_state);
      return true;
   }

   return false;
}

void
radv_lower_ngg(struct radv_device *device, struct nir_shader *nir,
               const struct radv_shader_info *info,
               const struct radv_pipeline_key *pl_key,
               const struct radv_shader_args *args)
{
   /* TODO: support the LLVM backend with the NIR lowering */
   assert(!radv_use_llvm_for_stage(device, nir->info.stage));

   assert(nir->info.stage == MESA_SHADER_VERTEX ||
          nir->info.stage == MESA_SHADER_TESS_EVAL ||
          nir->info.stage == MESA_SHADER_GEOMETRY);

   struct radv_nir_abi_state abi_state = {
      .stage = nir->info.stage,
      .info = info,
      .args = args,
      .pl_key = pl_key,
      .nir = nir,
   };

   const struct gfx10_ngg_info *ngg_info = &info->ngg_info;
   unsigned num_vertices_per_prim = 3;

   /* Get the number of vertices per input primitive */
   if (nir->info.stage == MESA_SHADER_TESS_EVAL) {
      if (nir->info.tess.point_mode)
         num_vertices_per_prim = 1;
      else if (nir->info.tess.primitive_mode == GL_ISOLINES)
         num_vertices_per_prim = 2;

      /* Manually mark the primitive ID used, so the shader can repack it. */
      if (info->tes.outinfo.export_prim_id)
         BITSET_SET(nir->info.system_values_read, SYSTEM_VALUE_PRIMITIVE_ID);

   } else if (nir->info.stage == MESA_SHADER_VERTEX) {
      /* Need to add 1, because: V_028A6C_POINTLIST=0, V_028A6C_LINESTRIP=1, V_028A6C_TRISTRIP=2, etc. */
      num_vertices_per_prim = si_conv_prim_to_gs_out(pl_key->vs.topology) + 1;

      /* Manually mark the instance ID used, so the shader can repack it. */
      if (pl_key->vs.instance_rate_inputs)
         BITSET_SET(nir->info.system_values_read, SYSTEM_VALUE_INSTANCE_ID);

   } else if (nir->info.stage == MESA_SHADER_GEOMETRY) {
      num_vertices_per_prim = nir->info.gs.vertices_in;
   } else {
      unreachable("NGG needs to be VS, TES or GS.");
   }

   /* Invocations that process an input vertex */
   unsigned max_vtx_in = MIN2(256, ngg_info->enable_vertex_grouping ? ngg_info->hw_max_esverts : num_vertices_per_prim * ngg_info->max_gsprims);

   if (nir->info.stage == MESA_SHADER_VERTEX ||
       nir->info.stage == MESA_SHADER_TESS_EVAL) {
      bool export_prim_id;

      assert(info->is_ngg);

      if (info->has_ngg_culling)
         radv_optimize_nir_algebraic(nir, false);

      if (nir->info.stage == MESA_SHADER_VERTEX) {
         export_prim_id = info->vs.outinfo.export_prim_id;
      } else {
         export_prim_id = info->tes.outinfo.export_prim_id;
      }

      ac_nir_lower_ngg_nogs(
         nir,
         max_vtx_in,
         num_vertices_per_prim,
         info->workgroup_size,
         info->wave_size,
         info->has_ngg_culling,
         info->has_ngg_early_prim_export,
         info->is_ngg_passthrough,
         export_prim_id,
         pl_key->vs.provoking_vtx_last,
         false,
         pl_key->vs.instance_rate_inputs,
         &args->ac, &radv_ngg_abi, &abi_state);
   } else if (nir->info.stage == MESA_SHADER_GEOMETRY) {
      assert(info->is_ngg);
      ac_nir_lower_ngg_gs(
         nir, info->wave_size, info->workgroup_size,
         info->ngg_info.esgs_ring_size,
         info->gs.gsvs_vertex_size,
         info->ngg_info.ngg_emit_size * 4u,
         pl_key->vs.provoking_vtx_last,
         &args->ac, &radv_ngg_abi, &abi_state);
   } else {
      unreachable("invalid SW stage passed to radv_lower_ngg");
   }
}
