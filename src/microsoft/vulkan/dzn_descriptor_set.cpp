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

#include <wrl/client.h>

#include "vk_alloc.h"
#include "vk_descriptors.h"
#include "vk_util.h"

using Microsoft::WRL::ComPtr;

static D3D12_SHADER_VISIBILITY
translate_desc_visibility(VkShaderStageFlags in)
{
   switch (in) {
   case VK_SHADER_STAGE_VERTEX_BIT: return D3D12_SHADER_VISIBILITY_VERTEX;
   case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return D3D12_SHADER_VISIBILITY_HULL;
   case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return D3D12_SHADER_VISIBILITY_DOMAIN;
   case VK_SHADER_STAGE_GEOMETRY_BIT: return D3D12_SHADER_VISIBILITY_GEOMETRY;
   case VK_SHADER_STAGE_FRAGMENT_BIT: return D3D12_SHADER_VISIBILITY_PIXEL;
   default: return D3D12_SHADER_VISIBILITY_ALL;
   }
}

static D3D12_DESCRIPTOR_RANGE_TYPE
desc_type_to_range_type(VkDescriptorType in)
{
   switch (in) {
   case VK_DESCRIPTOR_TYPE_SAMPLER: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
   case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
   case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
   case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
   case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
   default:
      unreachable("Unsupported desc type");
   }
}

static uint32_t
num_descs_for_type(VkDescriptorType type, bool immutable_samplers)
{
   unsigned num_descs = 1;

   /* There's no combined SRV+SAMPLER type in d3d12, we need an descriptor
    * for the sampler.
    */
   if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
      num_descs++;

   /* Don't count immutable samplers, they have their own descriptor. */
   if (immutable_samplers &&
       (type == VK_DESCRIPTOR_TYPE_SAMPLER ||
        type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
      num_descs--;

   return num_descs;
}

dzn_descriptor_set_layout::dzn_descriptor_set_layout(dzn_device *device,
                                                     const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator)
{
   VkDescriptorSetLayoutBinding *ordered_bindings;
   VkResult ret =
      vk_create_sorted_bindings(pCreateInfo->pBindings,
                                pCreateInfo->bindingCount,
                                &ordered_bindings);
   if (ret != VK_SUCCESS)
      throw vk_error(device, ret);

   struct dzn_descriptor_set_layout_binding *binfos =
      (struct dzn_descriptor_set_layout_binding *)bindings;

   assert(binding_count ==
          (pCreateInfo->bindingCount ?
	   (ordered_bindings[pCreateInfo->bindingCount - 1].binding + 1) : 0));

   uint32_t sampler_range_idx[MAX_SHADER_VISIBILITIES] = {};
   uint32_t view_range_idx[MAX_SHADER_VISIBILITIES] = {};
   uint32_t static_sampler_idx = 0;
   uint32_t base_register = 0;

   for (uint32_t i = 0; i < binding_count; i++) {
      binfos[i].static_sampler_idx = ~0;
      binfos[i].sampler_range_idx = ~0;
      binfos[i].view_range_idx = ~0;
   }

   view_desc_count = 0;
   sampler_desc_count = 0;

   for (uint32_t i = 0; i < pCreateInfo->bindingCount; i++) {
      VkDescriptorType desc_type = ordered_bindings[i].descriptorType;
      uint32_t binding = ordered_bindings[i].binding;
      bool has_sampler =
         desc_type == VK_DESCRIPTOR_TYPE_SAMPLER ||
	 desc_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      bool immutable_samplers =
         has_sampler &&
         ordered_bindings[i].pImmutableSamplers != NULL;

      D3D12_SHADER_VISIBILITY visibility =
         translate_desc_visibility(ordered_bindings[i].stageFlags);
      binfos[binding].visibility = visibility;
      binfos[binding].base_shader_register = base_register;
      assert(base_register + ordered_bindings[i].descriptorCount >= base_register);
      base_register += ordered_bindings[i].descriptorCount;

      if (immutable_samplers) {
	 binfos[binding].static_sampler_idx = static_sampler_idx;
         D3D12_STATIC_SAMPLER_DESC *sampler = (D3D12_STATIC_SAMPLER_DESC *)
            &static_samplers[static_sampler_idx];
         static_sampler_idx++;
         assert(0);
         /* FIXME: parse samplers */
      }

      unsigned num_descs =
         num_descs_for_type(desc_type, immutable_samplers);
      if (!num_descs) continue;

      D3D12_DESCRIPTOR_RANGE1 *range;

      assert(visibility < ARRAY_SIZE(ranges));

      if (has_sampler && !immutable_samplers) {
         assert(sampler_range_idx[visibility] < ranges[visibility].sampler_count);
         uint32_t range_idx = sampler_range_idx[visibility]++;

         binfos[binding].sampler_range_idx = range_idx;
         range = (D3D12_DESCRIPTOR_RANGE1 *)
            &ranges[visibility].samplers[range_idx];
         range->RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
         range->NumDescriptors = ordered_bindings[i].descriptorCount;
         range->BaseShaderRegister = binfos[binding].base_shader_register;
         range->Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
         range->OffsetInDescriptorsFromTableStart = sampler_desc_count;
         sampler_desc_count += range->NumDescriptors;
      }

      if (desc_type != VK_DESCRIPTOR_TYPE_SAMPLER) {
         assert(view_range_idx[visibility] < ranges[visibility].view_count);
         uint32_t range_idx = view_range_idx[visibility]++;

         binfos[binding].view_range_idx = range_idx;
         range = (D3D12_DESCRIPTOR_RANGE1 *)
            &ranges[visibility].views[range_idx];
         range->RangeType =
            desc_type_to_range_type(ordered_bindings[i].descriptorType);
         range->NumDescriptors = ordered_bindings[i].descriptorCount;
         range->BaseShaderRegister = binfos[binding].base_shader_register;
         range->Flags =
            D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS;
         range->OffsetInDescriptorsFromTableStart = view_desc_count;
         view_desc_count += range->NumDescriptors;
      }
   }

   free(ordered_bindings);

   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
}

dzn_descriptor_set_layout::~dzn_descriptor_set_layout()
{
   vk_object_base_finish(&base);
}

dzn_descriptor_set_layout *
dzn_descriptor_set_layout_factory::allocate(dzn_device *device,
                                            const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator)
{
   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);

   const VkDescriptorSetLayoutBinding *bindings = pCreateInfo->pBindings;
   uint32_t binding_count = 0, immutable_sampler_count = 0, total_ranges = 0;
   uint32_t sampler_ranges[MAX_SHADER_VISIBILITIES] = {};
   uint32_t view_ranges[MAX_SHADER_VISIBILITIES] = {};

   for (uint32_t i = 0; i < pCreateInfo->bindingCount; i++) {
      D3D12_SHADER_VISIBILITY visibility =
         translate_desc_visibility(bindings[i].stageFlags);
      VkDescriptorType desc_type = bindings[i].descriptorType;
      bool has_sampler =
         desc_type == VK_DESCRIPTOR_TYPE_SAMPLER ||
	 desc_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

      /* From the Vulkan 1.1.97 spec for VkDescriptorSetLayoutBinding:
       *
       *    "If descriptorType specifies a VK_DESCRIPTOR_TYPE_SAMPLER or
       *    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER type descriptor, then
       *    pImmutableSamplers can be used to initialize a set of immutable
       *    samplers. [...]  If descriptorType is not one of these descriptor
       *    types, then pImmutableSamplers is ignored.
       *
       * We need to be careful here and only parse pImmutableSamplers if we
       * have one of the right descriptor types.
       */
      bool immutable_samplers =
         has_sampler &&
         bindings[i].pImmutableSamplers != NULL;

      if (immutable_samplers) {
         immutable_sampler_count += bindings[i].descriptorCount;
      } else if (has_sampler) {
         sampler_ranges[visibility]++;
      }

      if (desc_type != VK_DESCRIPTOR_TYPE_SAMPLER) {
         view_ranges[visibility]++;
      }

      total_ranges += sampler_ranges[visibility] + view_ranges[visibility];
      binding_count = MAX2(binding_count, bindings[i].binding + 1);
   }

   /* We need to allocate decriptor set layouts off the device allocator
    * with DEVICE scope because they are reference counted and may not be
    * destroyed when vkDestroyDescriptorSetLayout is called.
    */
   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, struct dzn_descriptor_set_layout, set_layout, 1);
   VK_MULTIALLOC_DECL(&ma, D3D12_DESCRIPTOR_RANGE1,
                      ranges, total_ranges);
   VK_MULTIALLOC_DECL(&ma, D3D12_STATIC_SAMPLER_DESC, static_samplers,
                      immutable_sampler_count);
   VK_MULTIALLOC_DECL(&ma, dzn_descriptor_set_layout_binding, binfos,
                      binding_count);

   if (!vk_multialloc_zalloc(&ma, &device->vk.alloc,
                             VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
      return NULL;

   set_layout->static_samplers = static_samplers;
   set_layout->static_sampler_count = immutable_sampler_count;
   set_layout->bindings = binfos;
   set_layout->binding_count = binding_count;

   for (uint32_t i = 0; i < ARRAY_SIZE(set_layout->ranges); i++) {
      if (sampler_ranges[i]) {
         set_layout->ranges[i].samplers = ranges;
         set_layout->ranges[i].sampler_count = sampler_ranges[i];
         ranges += sampler_ranges[i];
      }

      if (view_ranges[i]) {
         set_layout->ranges[i].views = ranges;
         set_layout->ranges[i].view_count = view_ranges[i];
         ranges += view_ranges[i];
      }
   }

   return set_layout;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateDescriptorSetLayout(VkDevice device,
                              const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                              const VkAllocationCallbacks *pAllocator,
                              VkDescriptorSetLayout *pSetLayout)
{
   return dzn_descriptor_set_layout_factory::create(device, pCreateInfo,
                                                    pAllocator, pSetLayout);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyDescriptorSetLayout(VkDevice device,
                               VkDescriptorSetLayout descriptorSetLayout,
                               const VkAllocationCallbacks *pAllocator)
{
   dzn_descriptor_set_layout_factory::destroy(device, descriptorSetLayout, pAllocator);
}

// Reserve two root parameters for the push constants and sysvals CBVs.
#define MAX_INTERNAL_ROOT_PARAMS 2

// One root parameter for samplers and the other one for views, multiplied by
// the number of visibility combinations, plus the internal root parameters.
#define MAX_ROOT_PARAMS ((MAX_SHADER_VISIBILITIES * 2) + MAX_INTERNAL_ROOT_PARAMS)

// Maximum number of DWORDS (32-bit words) that can be used for a root signature
#define MAX_ROOT_DWORDS 64

dzn_pipeline_layout::dzn_pipeline_layout(dzn_device *device,
                                         const VkPipelineLayoutCreateInfo *pCreateInfo,
                                         const VkAllocationCallbacks *pAllocator)
{
   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);

   uint32_t range_desc_count = 0, static_sampler_count = 0;
   uint32_t view_desc_count = 0, sampler_desc_count = 0;

   root.param_count = 0;

   set_count = pCreateInfo->setLayoutCount;
   for (uint32_t j = 0; j < set_count; j++) {
      VK_FROM_HANDLE(dzn_descriptor_set_layout, set_layout, pCreateInfo->pSetLayouts[j]);

      static_sampler_count += set_layout->static_sampler_count;
      for (uint32_t i = 0; i < MAX_SHADER_VISIBILITIES; i++) {
         range_desc_count += set_layout->ranges[i].sampler_count +
                             set_layout->ranges[i].view_count;
      }

      sets[j].heap_offsets[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = view_desc_count;
      sets[j].heap_offsets[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = sampler_desc_count;
      view_desc_count += set_layout->view_desc_count;
      sampler_desc_count += set_layout->sampler_desc_count;

      sets[j].layout = set_layout;
   }

   auto range_descs =
      dzn_transient_zalloc<D3D12_DESCRIPTOR_RANGE1>(range_desc_count,
                                                    &device->vk.alloc,
                                                    pAllocator);
   if (range_desc_count && !range_descs.get())
      throw vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   auto static_sampler_descs =
      dzn_transient_zalloc<D3D12_STATIC_SAMPLER_DESC>(range_desc_count,
                                                      &device->vk.alloc,
                                                      pAllocator);
   if (static_sampler_count && !static_sampler_descs.get())
      throw vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   D3D12_ROOT_PARAMETER1 root_params[MAX_ROOT_PARAMS] = {};
   D3D12_DESCRIPTOR_RANGE1 *range_ptr = range_descs.get();
   D3D12_ROOT_PARAMETER1 *root_param;
   uint32_t root_dwords = 0;

   for (uint32_t i = 0; i < MAX_SHADER_VISIBILITIES; i++) {
      uint32_t range_count = 0;

      root_param = &root_params[root.param_count];
      root_param->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      root_param->DescriptorTable.pDescriptorRanges = range_ptr;
      root_param->DescriptorTable.NumDescriptorRanges = 0;
      root_param->ShaderVisibility = (D3D12_SHADER_VISIBILITY)i;

      for (uint32_t j = 0; j < pCreateInfo->setLayoutCount; j++) {
         VK_FROM_HANDLE(dzn_descriptor_set_layout, set_layout, pCreateInfo->pSetLayouts[j]);

         memcpy(range_ptr, set_layout->ranges[i].views,
                set_layout->ranges[i].view_count * sizeof(D3D12_DESCRIPTOR_RANGE1));
         for (uint32_t k = 0; k < set_layout->ranges[i].view_count; k++) {
            range_ptr[k].RegisterSpace = j;
            range_ptr[k].OffsetInDescriptorsFromTableStart +=
               sets[j].heap_offsets[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
         }
         root_param->DescriptorTable.NumDescriptorRanges += set_layout->ranges[i].view_count;
	 range_ptr += set_layout->ranges[i].view_count;
      }

      if (root_param->DescriptorTable.NumDescriptorRanges) {
         root.type[root.param_count++] = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
         root_dwords++;
      }

      root_param = &root_params[root.param_count];
      root_param->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      root_param->DescriptorTable.pDescriptorRanges = range_ptr;
      root_param->DescriptorTable.NumDescriptorRanges = 0;
      root_param->ShaderVisibility = (D3D12_SHADER_VISIBILITY)i;

      for (uint32_t j = 0; j < pCreateInfo->setLayoutCount; j++) {
         VK_FROM_HANDLE(dzn_descriptor_set_layout, set_layout, pCreateInfo->pSetLayouts[j]);

         memcpy(range_ptr, set_layout->ranges[i].samplers,
                set_layout->ranges[i].sampler_count * sizeof(D3D12_DESCRIPTOR_RANGE1));
         for (uint32_t k = 0; k < set_layout->ranges[i].sampler_count; k++) {
            range_ptr[k].RegisterSpace = j;
            range_ptr[k].OffsetInDescriptorsFromTableStart +=
               sets[j].heap_offsets[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
         }
         root_param->DescriptorTable.NumDescriptorRanges += set_layout->ranges[i].sampler_count;
	 range_ptr += set_layout->ranges[i].sampler_count;
      }

      if (root_param->DescriptorTable.NumDescriptorRanges) {
         root.type[root.param_count++] = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
         root_dwords++;
      }
   }

   root.sets_param_count = root.param_count;

   /* Add our sysval CBV, and make it visible to all shaders */
   root.sysval_cbv_param_idx = root.param_count;
   root_param = &root_params[root.param_count++];
   root_param->ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
   root_param->Descriptor.RegisterSpace = DZN_REGISTER_SPACE_SYSVALS;
   root_param->Constants.ShaderRegister = 0;
   root_param->Constants.Num32BitValues =
       DIV_ROUND_UP(MAX2(sizeof(struct dxil_spirv_vertex_runtime_data),
                         sizeof(struct dxil_spirv_compute_runtime_data)),
                    4);
   root_param->ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
   root_dwords += root_param->Constants.Num32BitValues;

   D3D12_STATIC_SAMPLER_DESC *static_sampler_ptr = static_sampler_descs.get();
   for (uint32_t j = 0; j < pCreateInfo->setLayoutCount; j++) {
      VK_FROM_HANDLE(dzn_descriptor_set_layout, set_layout, pCreateInfo->pSetLayouts[j]);

      memcpy(static_sampler_ptr, set_layout->static_samplers,
             set_layout->static_sampler_count * sizeof(*set_layout->static_samplers));
      if (j > 0) {
         for (uint32_t k = 0; k < set_layout->static_sampler_count; k++)
            static_sampler_ptr[k].RegisterSpace = j;
      }
      static_sampler_ptr += set_layout->static_sampler_count;
   }

   uint32_t push_constant_size = 0;
   uint32_t push_constant_flags = 0;
   for (uint32_t j = 0; j < pCreateInfo->pushConstantRangeCount; j++) {
      const VkPushConstantRange* range = pCreateInfo->pPushConstantRanges + j;
      push_constant_size = MAX2(push_constant_size, range->offset + range->size);
      push_constant_flags |= range->stageFlags;
   }

   if (push_constant_size > 0) {
      root.push_constant_cbv_param_idx = root.param_count;
      D3D12_ROOT_PARAMETER1 *root_param = &root_params[root.param_count++];

      root_param->ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
      root_param->Constants.ShaderRegister = 0;
      root_param->Constants.Num32BitValues = ALIGN(push_constant_size, 4) / 4;
      root_param->Constants.RegisterSpace = DZN_REGISTER_SPACE_PUSH_CONSTANT;
      root_param->ShaderVisibility = translate_desc_visibility(push_constant_flags);
      root_dwords += root_param->Constants.Num32BitValues;
   }

   assert(root.param_count <= ARRAY_SIZE(root_params));
   assert(root_dwords <= MAX_ROOT_DWORDS);

   D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc = {
      .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
      .Desc_1_1 = {
         .NumParameters = root.param_count,
         .pParameters = root.param_count ? root_params : NULL,
         .NumStaticSamplers = static_sampler_count,
         .pStaticSamplers = static_sampler_descs.get(),
         /* TODO Only enable this flag when needed (optimization) */
         .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
      },
   };

   root.sig = device->create_root_sig(root_sig_desc);
   if (!root.sig.Get())
      throw vk_error(device, VK_ERROR_UNKNOWN);

   desc_count[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = view_desc_count;
   desc_count[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = sampler_desc_count;

   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_PIPELINE_LAYOUT);
}

dzn_pipeline_layout::~dzn_pipeline_layout()
{
   vk_object_base_finish(&base);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreatePipelineLayout(VkDevice device,
                         const VkPipelineLayoutCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         VkPipelineLayout *pPipelineLayout)
{
   return dzn_pipeline_layout_factory::create(device, pCreateInfo,
                                              pAllocator, pPipelineLayout);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyPipelineLayout(VkDevice device,
                          VkPipelineLayout layout,
                          const VkAllocationCallbacks *pAllocator)
{
   dzn_pipeline_layout_factory::destroy(device, layout, pAllocator);
}

static D3D12_DESCRIPTOR_HEAP_TYPE
desc_type_to_heap_type(VkDescriptorType in)
{
   switch (in) {
   case VK_DESCRIPTOR_TYPE_SAMPLER:
     return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
   case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
   case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
   case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
   case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
   case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
     return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
   case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
   default:
      unreachable("Unsupported desc type");
   }
}

dzn_descriptor_pool::dzn_descriptor_pool(dzn_device *device,
                                         const VkDescriptorPoolCreateInfo *pCreateInfo,
                                         const VkAllocationCallbacks *pAllocator)
{
   alloc = pAllocator ? *pAllocator : device->vk.alloc;
   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
}

dzn_descriptor_pool::~dzn_descriptor_pool()
{
   vk_object_base_finish(&base);
}

VkResult
dzn_descriptor_pool::allocate_sets(VkDevice device,
                                   const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                   VkDescriptorSet *pDescriptorSets)
{
   VkResult result;
   unsigned i;

   for (i = 0; i < pAllocateInfo->descriptorSetCount; i++) {
      result = dzn_descriptor_set_factory::create(device, this,
                                                  pAllocateInfo->pSetLayouts[i],
                                                  &alloc,
                                                  &pDescriptorSets[i]);
      if (result != VK_SUCCESS)
         goto err_free_sets;
   }

   return VK_SUCCESS;

err_free_sets:
   free_sets(device, i, pDescriptorSets);
   for (i = 0; i < pAllocateInfo->descriptorSetCount; i++)
      pDescriptorSets[i] = VK_NULL_HANDLE;

   return result;
}

VkResult
dzn_descriptor_pool::free_sets(VkDevice device,
                               uint32_t count,
                               const VkDescriptorSet *pDescriptorSets)
{
   for (uint32_t i = 0; i < count; i++)
      dzn_descriptor_set_factory::destroy(device, pDescriptorSets[i], &alloc);

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateDescriptorPool(VkDevice device,
                         const VkDescriptorPoolCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         VkDescriptorPool *pDescriptorPool)
{
   return dzn_descriptor_pool_factory::create(device,
                                              pCreateInfo,
                                              pAllocator,
                                              pDescriptorPool);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyDescriptorPool(VkDevice device,
                          VkDescriptorPool descriptorPool,
                          const VkAllocationCallbacks *pAllocator)
{
   return dzn_descriptor_pool_factory::destroy(device,
                                               descriptorPool,
                                               pAllocator);
}

dzn_descriptor_set::dzn_descriptor_set(dzn_device *device,
                                       dzn_descriptor_pool *pool,
                                       VkDescriptorSetLayout l,
                                       const VkAllocationCallbacks *pAllocator)
{
   assert(bindings);
   layout = dzn_descriptor_set_layout_from_handle(l);

   uint32_t view_desc_sz =
      device->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
   uint32_t sampler_desc_sz =
      device->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

   SIZE_T view_desc_base = 0, sampler_desc_base = 0;

   if (layout->view_desc_count) {
      D3D12_DESCRIPTOR_HEAP_DESC desc = {
         .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
         .NumDescriptors = layout->view_desc_count,
         .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
      };

      HRESULT ret;
      ComPtr<ID3D12DescriptorHeap> heap;
      ret = device->dev->CreateDescriptorHeap(&desc,
                                              IID_PPV_ARGS(&heap));
      assert(!FAILED(ret));
      view_desc_base = heap->GetCPUDescriptorHandleForHeapStart().ptr;
      heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = heap;
   }

   if (layout->sampler_desc_count) {
      D3D12_DESCRIPTOR_HEAP_DESC desc = {
         .Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
         .NumDescriptors = layout->sampler_desc_count,
         .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
      };

      HRESULT ret;
      ComPtr<ID3D12DescriptorHeap> heap;
      ret = device->dev->CreateDescriptorHeap(&desc,
                                              IID_PPV_ARGS(&heap));
      assert(!FAILED(ret));
      sampler_desc_base = heap->GetCPUDescriptorHandleForHeapStart().ptr;
      heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = heap;
   }

   for (uint32_t i = 0; i < layout->binding_count; i++) {
      D3D12_SHADER_VISIBILITY visibility = layout->bindings[i].visibility;
      uint32_t view_range_idx = layout->bindings[i].view_range_idx;
      uint32_t sampler_range_idx = layout->bindings[i].sampler_range_idx;
      struct dzn_descriptor_set_binding *b =
         (struct dzn_descriptor_set_binding *)&bindings[i];

      assert(visibility < ARRAY_SIZE(layout->ranges));
      assert(view_range_idx == ~0 || view_range_idx < layout->ranges[visibility].view_count);
      assert(sampler_range_idx == ~0 || sampler_range_idx < layout->ranges[visibility].sampler_count);

      if (view_range_idx != ~0) {
	 const D3D12_DESCRIPTOR_RANGE1 *range =
            &layout->ranges[visibility].views[view_range_idx];

         b->views.ptr =
            view_desc_base + (range->OffsetInDescriptorsFromTableStart * view_desc_sz);
      }

      if (sampler_range_idx != ~0) {
	 const D3D12_DESCRIPTOR_RANGE1 *range =
            &layout->ranges[visibility].samplers[sampler_range_idx];

         b->samplers.ptr =
            sampler_desc_base + (range->OffsetInDescriptorsFromTableStart * sampler_desc_sz);
      }
   }

   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_DESCRIPTOR_SET);
}

dzn_descriptor_set::~dzn_descriptor_set()
{
   vk_object_base_finish(&base);
}

dzn_descriptor_set *
dzn_descriptor_set_factory::allocate(dzn_device *device,
                                     dzn_descriptor_pool *pool,
                                     VkDescriptorSetLayout l,
                                     const VkAllocationCallbacks *alloc)
{
   VK_FROM_HANDLE(dzn_descriptor_set_layout, layout, l);

   /* TODO: Allocate from the pool! */
   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, dzn_descriptor_set, set, 1);
   VK_MULTIALLOC_DECL(&ma, dzn_descriptor_set_binding,
                      bindings, layout->binding_count);

   if (!vk_multialloc_zalloc(&ma, &device->vk.alloc,
                             VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
      return NULL;

   set->bindings = bindings;
   return set;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_AllocateDescriptorSets(VkDevice device,
                           const VkDescriptorSetAllocateInfo *pAllocateInfo,
                           VkDescriptorSet *pDescriptorSets)
{
   VK_FROM_HANDLE(dzn_descriptor_pool, pool, pAllocateInfo->descriptorPool);

   return pool->allocate_sets(device, pAllocateInfo, pDescriptorSets);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_FreeDescriptorSets(VkDevice device,
                       VkDescriptorPool descriptorPool,
                       uint32_t count,
                       const VkDescriptorSet *pDescriptorSets)
{
   VK_FROM_HANDLE(dzn_descriptor_pool, pool, descriptorPool);

   return pool->free_sets(device, count, pDescriptorSets);
}

static void
dzn_write_descriptor_set(struct dzn_device *dev,
                         const VkWriteDescriptorSet *pDescriptorWrite)
{
   VK_FROM_HANDLE(dzn_descriptor_set, set, pDescriptorWrite->dstSet);
   const dzn_descriptor_set_layout *layout = set->layout;

   uint32_t view_desc_sz =
      dev->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
   uint32_t sampler_desc_sz =
      dev->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
   uint32_t b = pDescriptorWrite->dstBinding;
   uint32_t offset = pDescriptorWrite->dstArrayElement;
   uint32_t d = 0;

   for (uint32_t d = 0; d < pDescriptorWrite->descriptorCount;) {
      if (b >= layout->binding_count)
         break;

      D3D12_SHADER_VISIBILITY visibility =
         layout->bindings[b].visibility;
      uint32_t view_range_idx = layout->bindings[b].view_range_idx;
      uint32_t sampler_range_idx = layout->bindings[b].sampler_range_idx;
      unsigned desc_count;
      assert(visibility < ARRAY_SIZE(layout->ranges));
      assert(view_range_idx == ~0 || view_range_idx < layout->ranges[visibility].view_count);
      assert(sampler_range_idx == ~0 || sampler_range_idx < layout->ranges[visibility].sampler_count);
      if (view_range_idx != ~0)
         desc_count = layout->ranges[visibility].views[view_range_idx].NumDescriptors;
      else if (sampler_range_idx != ~0)
         desc_count = layout->ranges[visibility].samplers[sampler_range_idx].NumDescriptors;
      else
         desc_count = 0;

      if (offset >= desc_count) {
         offset -= desc_count;
         b++;
         continue;
      }

      D3D12_CPU_DESCRIPTOR_HANDLE view_handle = {
         set->bindings[b].views.ptr ?
         set->bindings[b].views.ptr + (offset * view_desc_sz) :
         0
      };
      D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle = {
         set->bindings[b].samplers.ptr ?
         set->bindings[b].samplers.ptr + (offset * sampler_desc_sz) :
         0
      };

      if (sampler_handle.ptr && pDescriptorWrite->pImageInfo) {
         VK_FROM_HANDLE(dzn_sampler, sampler, pDescriptorWrite->pImageInfo->sampler);
         dev->dev->CreateSampler(&sampler->desc, sampler_handle);
      }

      if (view_handle.ptr) {
         switch (pDescriptorWrite->descriptorType) {
         case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
         case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            if (pDescriptorWrite->pImageInfo) {
               VK_FROM_HANDLE(dzn_image_view, iview, pDescriptorWrite->pImageInfo->imageView);
               dev->dev->CreateShaderResourceView(iview->image->res.Get(), &iview->desc, view_handle);
            }
            break;
         case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
               VK_FROM_HANDLE(dzn_buffer, buf, pDescriptorWrite->pBufferInfo->buffer);
	       uint32_t size = pDescriptorWrite->pBufferInfo->range == VK_WHOLE_SIZE ?
	                       buf->size - pDescriptorWrite->pBufferInfo->offset :
                               pDescriptorWrite->pBufferInfo->range;

               D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {
                 .BufferLocation = buf->res->GetGPUVirtualAddress() +
                                   pDescriptorWrite->pBufferInfo->offset,
                 .SizeInBytes = ALIGN_POT(size, 256),
               };

               dev->dev->CreateConstantBufferView(&cbv_desc, view_handle);
            }
            break;
         case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
            VK_FROM_HANDLE(dzn_buffer, buf, pDescriptorWrite->pBufferInfo->buffer);
            uint32_t size = pDescriptorWrite->pBufferInfo->range == VK_WHOLE_SIZE ?
                            buf->size - pDescriptorWrite->pBufferInfo->offset :
                            pDescriptorWrite->pBufferInfo->range;

            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
              .Format = DXGI_FORMAT_R32_TYPELESS,
              .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
              .Buffer = {
                 .FirstElement = pDescriptorWrite->pBufferInfo->offset / sizeof(uint32_t),
                 .NumElements = size / sizeof(uint32_t),
                 .Flags = D3D12_BUFFER_UAV_FLAG_RAW,
              },
            };

            dev->dev->CreateUnorderedAccessView(buf->res.Get(), NULL, &uav_desc, view_handle);
            break;
         }
         default:
            // TODO: support all types
            unreachable("Unsupported descriptor type\n");
         }
      }

      d++;
   }
}

static void
dzn_copy_descriptor_set(struct dzn_device *dev,
                        const VkCopyDescriptorSet *pDescriptorCopy)
{
   // TODO: support copies
   assert(0);
}

VKAPI_ATTR void VKAPI_CALL
dzn_UpdateDescriptorSets(VkDevice _device,
                         uint32_t descriptorWriteCount,
                         const VkWriteDescriptorSet *pDescriptorWrites,
                         uint32_t descriptorCopyCount,
                         const VkCopyDescriptorSet *pDescriptorCopies)
{
   VK_FROM_HANDLE(dzn_device, dev, _device);

   for (unsigned i = 0; i < descriptorWriteCount; i++)
      dzn_write_descriptor_set(dev, &pDescriptorWrites[i]);

   for (unsigned i = 0; i < descriptorCopyCount; i++)
      dzn_copy_descriptor_set(dev, &pDescriptorCopies[i]);
}
