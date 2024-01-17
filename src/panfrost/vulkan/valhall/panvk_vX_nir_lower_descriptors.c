/*
 * Copyright © 2024 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "panvk_descriptor_set.h"
#include "panvk_descriptor_set_layout.h"
#include "panvk_shader.h"

#include "nir.h"
#include "nir_builder.h"

struct lower_descriptors_ctx {
   const struct panvk_pipeline_layout *layout;
   const struct panfrost_compile_inputs *compile_inputs;
   bool has_img_access;
};

static bool
lower_tex(nir_builder *b, nir_tex_instr *tex,
          const struct lower_descriptors_ctx *ctx)
{
   /* TODO */
   return false;
}

static bool
lower_res_intrin(nir_builder *b, nir_intrinsic_instr *intrin,
                 const struct lower_descriptors_ctx *ctx)
{
   /* TODO */
   return false;
}

static bool
lower_image_intrin(nir_builder *b, nir_intrinsic_instr *intrin,
                   const struct lower_descriptors_ctx *ctx)
{
   /* TODO */
   return false;
}

static bool
lower_intrinsic(nir_builder *b, nir_intrinsic_instr *intrin,
                struct lower_descriptors_ctx *ctx)
{
   switch (intrin->intrinsic) {
   case nir_intrinsic_vulkan_resource_index:
   case nir_intrinsic_vulkan_resource_reindex:
   case nir_intrinsic_load_vulkan_descriptor:
      return lower_res_intrin(b, intrin, ctx);

   case nir_intrinsic_image_deref_load:
   case nir_intrinsic_image_deref_store:
   case nir_intrinsic_image_deref_atomic:
   case nir_intrinsic_image_deref_atomic_swap:
   case nir_intrinsic_image_deref_size:
   case nir_intrinsic_image_deref_samples:
   case nir_intrinsic_image_deref_texel_address:
      return lower_image_intrin(b, intrin, ctx);

   default:
      return false;
   }
}

static bool
lower_descriptors_instr(nir_builder *b, nir_instr *instr, void *data)
{
   struct lower_descriptors_ctx *ctx = data;

   switch (instr->type) {
   case nir_instr_type_tex:
      return lower_tex(b, nir_instr_as_tex(instr), ctx);
   case nir_instr_type_intrinsic:
      return lower_intrinsic(b, nir_instr_as_intrinsic(instr), ctx);
   default:
      return false;
   }
}

bool
panvk_per_arch(nir_lower_descriptors)(
   nir_shader *nir, const struct panvk_lower_desc_inputs *inputs,
   bool *has_img_access_out)
{
   struct lower_descriptors_ctx ctx = {
      .layout = inputs->layout,
      .compile_inputs = inputs->compile_inputs,
   };

   bool progress = nir_shader_instructions_pass(
      nir, lower_descriptors_instr,
      nir_metadata_block_index | nir_metadata_dominance, (void *)&ctx);

   if (has_img_access_out)
      *has_img_access_out = ctx.has_img_access;

   return progress;
}
