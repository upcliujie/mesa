/*
 * Copyright © 2021 Raspberry Pi
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

#include "v3dv_private.h"
#include "broadcom/common/v3d_macros.h"
#include "broadcom/cle/v3dx_pack.h"
#include "broadcom/compiler/v3d_compiler.h"

/* The following v3dv_xxx_descriptor structs represent descriptor info that we
 * upload to a bo, specifically a subregion of the descriptor pool bo.
 *
 * The general rule that we apply right now to decide which info goes to such
 * bo is that we upload those that are referenced by an address when emitting
 * a packet, so needed to be uploaded to an bo in any case.
 *
 * Note that these structs are mostly helpers that improve the semantics when
 * doing all that, but we could do as other mesa vulkan drivers and just
 * upload the info we know it is expected based on the context.
 *
 * Also note that the sizes are aligned, as there is an alignment requirement
 * for addresses.
 */
struct v3dv_sampled_image_descriptor {
   uint8_t texture_state[cl_aligned_packet_length(TEXTURE_SHADER_STATE, 32)];
};

struct v3dv_sampler_descriptor {
   uint8_t sampler_state[cl_aligned_packet_length(SAMPLER_STATE, 32)];
};

struct v3dv_combined_image_sampler_descriptor {
   uint8_t texture_state[cl_aligned_packet_length(TEXTURE_SHADER_STATE, 32)];
   uint8_t sampler_state[cl_aligned_packet_length(SAMPLER_STATE, 32)];
};

/*
 * Returns how much space a given descriptor type needs on a bo (GPU
 * memory).
 */
uint32_t
v3dX(descriptor_bo_size)(VkDescriptorType type)
{
   switch(type) {
   case VK_DESCRIPTOR_TYPE_SAMPLER:
      return cl_aligned_packet_length(SAMPLER_STATE, 32);
   case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      return cl_aligned_packet_length(SAMPLER_STATE, 32) +
         cl_aligned_packet_length(TEXTURE_SHADER_STATE, 32);
   case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
   case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
   case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
   case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
   case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      return cl_aligned_packet_length(TEXTURE_SHADER_STATE, 32);
   default:
      return 0;
   }
}

uint32_t
v3dX(v3dv_max_descriptor_bo_size)(void)
{
   /* We now that the bigger bo_size comes from the combined image sampler.
    * FIXME: perhaps it is safer to just iterate through all the descriptor
    * types
    */
   return v3dX(descriptor_bo_size)(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}


uint32_t
v3dX(offsetof_texture_state_on_combined)(void)
{
   return 0;
}

uint32_t
v3dX(offsetof_sampler_state_on_combined)(void)
{
   return cl_aligned_packet_length(TEXTURE_SHADER_STATE, 32);
}
