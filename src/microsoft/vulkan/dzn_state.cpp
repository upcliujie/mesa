/*
 * Copyright © Microsoft Corporation
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "dzn_private.h"

#include "vk_alloc.h"
#include "vk_debug_report.h"
#include "vk_util.h"

#include "util/macros.h"

D3D12_TEXTURE_ADDRESS_MODE
translate_addr_mode(VkSamplerAddressMode in)
{
   switch (in) {
   case VK_SAMPLER_ADDRESS_MODE_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
   case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
   case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
   case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
   default: unreachable("Invalid address mode");
   }
}

VkResult
dzn_CreateSampler(VkDevice _device,
                  const VkSamplerCreateInfo *pCreateInfo,
                  const VkAllocationCallbacks *pAllocator,
                  VkSampler *pSampler)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   dzn_sampler *sampler;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);

   const VkSamplerCustomBorderColorCreateInfoEXT *pBorderColor = (const VkSamplerCustomBorderColorCreateInfoEXT *)
      vk_find_struct_const(pCreateInfo->pNext, SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT);

   sampler = (dzn_sampler *)
      vk_object_zalloc(&device->vk, pAllocator, sizeof(*sampler),
                       VK_OBJECT_TYPE_SAMPLER);
   if (!sampler)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   /* TODO: have a sampler pool to allocate shader-invisible descs which we
    * can copy to the desc_set when UpdateDescriptorSets() is called.
    */
   sampler->desc.Filter = dzn_translate_sampler_filter(pCreateInfo);
   sampler->desc.AddressU = translate_addr_mode(pCreateInfo->addressModeU);
   sampler->desc.AddressV = translate_addr_mode(pCreateInfo->addressModeV);
   sampler->desc.AddressW = translate_addr_mode(pCreateInfo->addressModeW);
   sampler->desc.MipLODBias = pCreateInfo->mipLodBias;
   sampler->desc.MaxAnisotropy = pCreateInfo->maxAnisotropy;
   sampler->desc.MinLOD = pCreateInfo->minLod;
   sampler->desc.MaxLOD = pCreateInfo->maxLod;

   if (pCreateInfo->compareEnable)
      sampler->desc.ComparisonFunc = dzn_translate_compare_op(pCreateInfo->compareOp);

   switch (pCreateInfo->borderColor) {
   case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
   case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
      sampler->desc.BorderColor[0] = sampler->desc.BorderColor[1] = sampler->desc.BorderColor[2] = 0.0f;
      sampler->desc.BorderColor[3] =
         pCreateInfo->borderColor == VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK ? 0.0f : 1.0f;
      break;
   case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
      sampler->desc.BorderColor[0] = sampler->desc.BorderColor[1] =
         sampler->desc.BorderColor[2] = sampler->desc.BorderColor[3] = 1.0f;
      break;
   case VK_BORDER_COLOR_FLOAT_CUSTOM_EXT:
      for (unsigned i = 0; i < ARRAY_SIZE(sampler->desc.BorderColor); i++)
         sampler->desc.BorderColor[i] = pBorderColor->customBorderColor.float32[i];
      break;
   default:
      unreachable("Unsupported border color");
   }

   *pSampler = dzn_sampler_to_handle(sampler);

   return VK_SUCCESS;
}

void
dzn_DestroySampler(VkDevice _device,
                   VkSampler _sampler,
                   const VkAllocationCallbacks *pAllocator)
{
   DZN_FROM_HANDLE(dzn_device, device, _device);
   DZN_FROM_HANDLE(dzn_sampler, sampler, _sampler);

   if (!sampler)
      return;

   vk_object_free(&device->vk, pAllocator, sampler);
}
