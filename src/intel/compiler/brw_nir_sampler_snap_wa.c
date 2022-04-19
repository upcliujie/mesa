/*
 * Copyright © 2022 Collabora Ltd.
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

#include "brw_nir.h"

static bool
apply_sampler_snap_wa(nir_builder *b, nir_instr *instr, UNUSED void *_data)
{
   if (instr->type != nir_instr_type_tex)
      return false;

   nir_tex_instr *tex = nir_instr_as_tex(instr);
   if (tex->op != nir_texop_tex && tex->op != nir_texop_txl)
      return false;

   b->cursor = nir_before_instr(&tex->instr);
   nir_ssa_def *needs_snap_wa = nir_load_samplers_need_snap_wa_intel(b);

   int sampler_src_idx =
      nir_tex_instr_src_index(tex, nir_tex_src_sampler_offset);
   if (sampler_src_idx >= 0) {
      assert(tex->src[sampler_src_idx].src.is_ssa);
      nir_ssa_def *sampler_index =
         nir_iadd_imm(b, tex->src[sampler_src_idx].src.ssa,
                         tex->sampler_index);
      needs_snap_wa = nir_i2b(b, nir_iand_imm(b, nir_ushr(b, needs_snap_wa,
                                                             sampler_index),
                                                 0x1));
   } else {
      assert(tex->sampler_index < 32);
      needs_snap_wa = nir_i2b(b, nir_iand_imm(b, needs_snap_wa,
                                                 1u << tex->sampler_index));
   }

   int coord_src_idx = nir_tex_instr_src_index(tex, nir_tex_src_coord);
   assert(coord_src_idx >= 0 && tex->src[coord_src_idx].src.is_ssa);
   nir_ssa_def *comps[3];
   for (unsigned i = 0; i < tex->coord_components; i++)
      comps[i] = nir_channel(b, tex->src[coord_src_idx].src.ssa, i);
   for (unsigned i = 0; i < tex->coord_components - tex->is_array; i++) {
      nir_ssa_def *is_neg = nir_flt(b, comps[i], nir_imm_float(b, 0.0f));
      comps[i] = nir_bcsel(b, nir_iand(b, is_neg, needs_snap_wa),
                              nir_imm_float(b, -1.0f), comps[i]);
   }
   nir_ssa_def *coord = nir_vec(b, comps, tex->coord_components);
   nir_instr_rewrite_src_ssa(&tex->instr, &tex->src[coord_src_idx].src, coord);

   return true;
}

/** Applies the sampler snap workaround.
 *
 * This is required to get enough precision with CL_ADDRESS_CLAMP_TO_EDGE.
 * The compute-runtime driver implements it as follows:
 *
 *    float4 ImageSampleExplicitLod(__spirv_SampledImage_2D SampledImage,
 *                                  float2 Coordinate, int ImageOperands,
 *                                  float Lod)
 *    {
 *        int image_id = (int)__builtin_IB_get_image(SampledImage);
 *        int sampler_id = (int)__builtin_IB_get_sampler(SampledImage);
 *
 *        float2 snappedCoords = Coordinate;
 *
 *        if (__builtin_IB_get_snap_wa_reqd(sampler_id) != 0)
 *        {
 *            snappedCoords.x = (Coordinate.x < 0) ? -1.0f : Coordinate.x;
 *            snappedCoords.y = (Coordinate.y < 0) ? -1.0f : Coordinate.y;
 *        }
 *
 *        return __builtin_IB_OCL_2d_sample_l(image_id, sampler_id,
 *                                            snappedCoords, Lod);
 *    }
 *
 * This does the same but where __builtin_IB_get_snap_wa_reqd is replace by a
 * magic system value with one bit per sampler for when this workaround is
 * needed.
 */
bool
brw_nir_apply_sampler_snap_wa(nir_shader *nir)
{
   return nir_shader_instructions_pass(nir, apply_sampler_snap_wa,
                                       nir_metadata_block_index |
                                       nir_metadata_dominance,
                                       NULL);
}
