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

bool
radv_lower_io_to_mem(struct radv_device *device, struct nir_shader *nir,
                     const struct radv_shader_info *info, const struct radv_pipeline_key *pl_key)
{
   if (nir->info.stage == MESA_SHADER_VERTEX) {
      if (info->vs.as_ls) {
         ac_nir_lower_ls_outputs_to_mem(nir, info->vs.tcs_in_out_eq,
                                        info->vs.tcs_temp_only_input_mask,
                                        info->vs.num_linked_outputs);
         return true;
      } else if (info->vs.as_es) {
         ac_nir_lower_es_outputs_to_mem(nir, device->physical_device->rad_info.chip_class,
                                        info->vs.num_linked_outputs);
         return true;
      }
   } else if (nir->info.stage == MESA_SHADER_TESS_CTRL) {
      ac_nir_lower_hs_inputs_to_mem(nir, info->vs.tcs_in_out_eq, info->tcs.num_linked_inputs);
      ac_nir_lower_hs_outputs_to_mem(
         nir, device->physical_device->rad_info.chip_class, info->tcs.tes_reads_tess_factors,
         info->tcs.tes_inputs_read, info->tcs.tes_patch_inputs_read, info->tcs.num_linked_inputs,
         info->tcs.num_linked_outputs, info->tcs.num_linked_patch_outputs, true);
      ac_nir_lower_tess_to_const(nir, pl_key->tcs.tess_input_vertices, info->num_tess_patches,
                                 ac_nir_lower_patch_vtx_in | ac_nir_lower_num_patches);

      return true;
   } else if (nir->info.stage == MESA_SHADER_TESS_EVAL) {
      ac_nir_lower_tes_inputs_to_mem(nir, info->tes.num_linked_inputs,
                                     info->tes.num_linked_patch_inputs);
      ac_nir_lower_tess_to_const(nir, nir->info.tess.tcs_vertices_out, info->num_tess_patches,
                                 ac_nir_lower_patch_vtx_in | ac_nir_lower_num_patches);

      if (info->tes.as_es) {
         ac_nir_lower_es_outputs_to_mem(nir, device->physical_device->rad_info.chip_class,
                                        info->tes.num_linked_outputs);
      }

      return true;
   } else if (nir->info.stage == MESA_SHADER_GEOMETRY) {
      ac_nir_lower_gs_inputs_to_mem(nir, device->physical_device->rad_info.chip_class,
                                    info->gs.num_linked_inputs);
      return true;
   }

   return false;
}
