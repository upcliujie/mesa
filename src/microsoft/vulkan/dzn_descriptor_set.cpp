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
   case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
   case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   default:
      unreachable("Unsupported desc type");
   }
}

static bool
is_dynamic_desc_type(VkDescriptorType desc_type)
{
   return (desc_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
           desc_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
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

   uint32_t range_idx[MAX_SHADER_VISIBILITIES][NUM_POOL_TYPES] = {};
   uint32_t static_sampler_idx = 0;
   uint32_t dynamic_buffer_idx = 0;
   uint32_t base_register = 0;

   for (uint32_t i = 0; i < binding_count; i++) {
      binfos[i].static_sampler_idx = ~0;
      binfos[i].dynamic_buffer_idx = ~0;
      dzn_foreach_pool_type (type)
         binfos[i].range_idx[type] = ~0;
   }

   for (uint32_t i = 0; i < pCreateInfo->bindingCount; i++) {
      VkDescriptorType desc_type = ordered_bindings[i].descriptorType;
      uint32_t binding = ordered_bindings[i].binding;
      bool has_sampler =
         desc_type == VK_DESCRIPTOR_TYPE_SAMPLER ||
         desc_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      bool immutable_samplers =
         has_sampler &&
         ordered_bindings[i].pImmutableSamplers != NULL;
      bool is_dynamic = is_dynamic_desc_type(desc_type);

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

      if (is_dynamic) {
         binfos[binding].dynamic_buffer_idx = dynamic_buffer_idx;
         for (uint32_t d = 0; d < ordered_bindings[i].descriptorCount; d++)
            dynamic_buffers.bindings[dynamic_buffer_idx + d] = binding;
         dynamic_buffer_idx += ordered_bindings[i].descriptorCount;
         assert(dynamic_buffer_idx <= MAX_DYNAMIC_BUFFERS);
      }

      unsigned num_descs =
         num_descs_for_type(desc_type, immutable_samplers);
      if (!num_descs) continue;

      assert(visibility < ARRAY_SIZE(ranges));

      bool has_range[NUM_POOL_TYPES] = {};
      has_range[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] =
         has_sampler && !immutable_samplers;
      has_range[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] =
         desc_type != VK_DESCRIPTOR_TYPE_SAMPLER;

      dzn_foreach_pool_type (type) {
         if (!has_range[type]) continue;

         uint32_t idx = range_idx[visibility][type]++;
         assert(idx < range_count[visibility][type]);

         binfos[binding].range_idx[type] = idx;
         auto range = (D3D12_DESCRIPTOR_RANGE1 *) &ranges[visibility][type][idx];
         VkDescriptorType range_type = desc_type;
         if (desc_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            range_type = type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ?
                         VK_DESCRIPTOR_TYPE_SAMPLER :
                         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
         }
         range->RangeType = desc_type_to_range_type(range_type);
         range->NumDescriptors = ordered_bindings[i].descriptorCount;
         range->BaseShaderRegister = binfos[binding].base_shader_register;
         range->Flags = type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ?
            D3D12_DESCRIPTOR_RANGE_FLAG_NONE :
            D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS;
         if (is_dynamic) {
            range->OffsetInDescriptorsFromTableStart =
               dynamic_buffers.range_offset + dynamic_buffers.count;
            dynamic_buffers.count += range->NumDescriptors;
         } else {
            range->OffsetInDescriptorsFromTableStart = range_desc_count[type];
            range_desc_count[type] += range->NumDescriptors;
         }
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
   uint32_t dynamic_ranges_offset = 0;
   uint32_t range_count[MAX_SHADER_VISIBILITIES][NUM_POOL_TYPES] = {};

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
         range_count[visibility][D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]++;
         total_ranges++;
      }

      if (desc_type != VK_DESCRIPTOR_TYPE_SAMPLER) {
         range_count[visibility][D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]++;
         total_ranges++;
         if (!is_dynamic_desc_type(desc_type))
            dynamic_ranges_offset += bindings[i].descriptorCount;
      }

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
   set_layout->dynamic_buffers.range_offset = dynamic_ranges_offset;

   for (uint32_t i = 0; i < MAX_SHADER_VISIBILITIES; i++) {
      dzn_foreach_pool_type (type) {
         if (range_count[i][type]) {
            set_layout->ranges[i][type] = ranges;
            set_layout->range_count[i][type] = range_count[i][type];
            ranges += range_count[i][type];
         }
      }
   }

   return set_layout;
}

uint32_t
dzn_descriptor_set_layout::get_heap_offset(uint32_t b, D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
   assert(b < binding_count);
   D3D12_SHADER_VISIBILITY visibility = bindings[b].visibility;
   assert(visibility < ARRAY_SIZE(ranges));
   assert(type < NUM_POOL_TYPES);

   uint32_t range_idx = bindings[b].range_idx[type];

   if (range_idx == ~0)
      return ~0;

   assert(range_idx < range_count[visibility][type]);
   return ranges[visibility][type][range_idx].OffsetInDescriptorsFromTableStart;
}

uint32_t
dzn_descriptor_set_layout::get_desc_count(uint32_t b) const
{
   D3D12_SHADER_VISIBILITY visibility = bindings[b].visibility;
   assert(visibility < ARRAY_SIZE(ranges));

   dzn_foreach_pool_type (type) {
      uint32_t range_idx = bindings[b].range_idx[type];
      assert(range_idx == ~0 || range_idx < range_count[visibility][type]);

      if (range_idx != ~0)
         return ranges[visibility][type][range_idx].NumDescriptors;
   }
   return 0;
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

   uint32_t range_count = 0, static_sampler_count = 0;

   root.param_count = 0;
   dzn_foreach_pool_type (type)
      desc_count[type] = 0;

   set_count = pCreateInfo->setLayoutCount;
   for (uint32_t j = 0; j < set_count; j++) {
      VK_FROM_HANDLE(dzn_descriptor_set_layout, set_layout, pCreateInfo->pSetLayouts[j]);

      static_sampler_count += set_layout->static_sampler_count;
      dzn_foreach_pool_type (type) {
         sets[j].heap_offsets[type] = desc_count[type];
         desc_count[type] += set_layout->range_desc_count[type];
         for (uint32_t i = 0; i < MAX_SHADER_VISIBILITIES; i++)
            range_count += set_layout->range_count[i][type];
      }

      desc_count[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] +=
         set_layout->dynamic_buffers.count;

      sets[j].layout = set_layout;
   }

   auto ranges =
      dzn_transient_zalloc<D3D12_DESCRIPTOR_RANGE1>(range_count,
                                                    &device->vk.alloc,
                                                    pAllocator);
   if (range_count && !ranges.get())
      throw vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   auto static_sampler_descs =
      dzn_transient_zalloc<D3D12_STATIC_SAMPLER_DESC>(static_sampler_count,
                                                      &device->vk.alloc,
                                                      pAllocator);
   if (static_sampler_count && !static_sampler_descs.get())
      throw vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   D3D12_ROOT_PARAMETER1 root_params[MAX_ROOT_PARAMS] = {};
   D3D12_DESCRIPTOR_RANGE1 *range_ptr = ranges.get();
   D3D12_ROOT_PARAMETER1 *root_param;
   uint32_t root_dwords = 0;

   for (uint32_t i = 0; i < MAX_SHADER_VISIBILITIES; i++) {
      dzn_foreach_pool_type (type) {
         root_param = &root_params[root.param_count];
         root_param->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
         root_param->DescriptorTable.pDescriptorRanges = range_ptr;
         root_param->DescriptorTable.NumDescriptorRanges = 0;
         root_param->ShaderVisibility = (D3D12_SHADER_VISIBILITY)i;

         for (uint32_t j = 0; j < pCreateInfo->setLayoutCount; j++) {
            VK_FROM_HANDLE(dzn_descriptor_set_layout, set_layout, pCreateInfo->pSetLayouts[j]);
            uint32_t range_count = set_layout->range_count[i][type];

            memcpy(range_ptr, set_layout->ranges[i][type],
                   range_count * sizeof(D3D12_DESCRIPTOR_RANGE1));
            for (uint32_t k = 0; k < range_count; k++) {
               range_ptr[k].RegisterSpace = j;
               range_ptr[k].OffsetInDescriptorsFromTableStart += sets[j].heap_offsets[type];
            }
            root_param->DescriptorTable.NumDescriptorRanges += range_count;
            range_ptr += range_count;
         }

         if (root_param->DescriptorTable.NumDescriptorRanges) {
            root.type[root.param_count++] = (D3D12_DESCRIPTOR_HEAP_TYPE)type;
            root_dwords++;
         }
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
   case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
     return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
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

dzn_descriptor_heap::dzn_descriptor_heap(dzn_device *device,
                                         uint32_t heap_type,
                                         uint32_t desc_count,
                                         bool shader_visible)
   : device(device)
   , desc_count(desc_count)
{
   D3D12_DESCRIPTOR_HEAP_DESC desc = {
      .Type = (D3D12_DESCRIPTOR_HEAP_TYPE)heap_type,
      .NumDescriptors = desc_count,
      .Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                              : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
   };

   HRESULT ret;
   ret = device->dev->CreateDescriptorHeap(&desc,
                                           IID_PPV_ARGS(&heap));
   assert(!FAILED(ret));

   type = desc.Type;
   desc_sz = device->dev->GetDescriptorHandleIncrementSize(desc.Type);
   cpu_base = heap->GetCPUDescriptorHandleForHeapStart().ptr;
}

const VkAllocationCallbacks *
dzn_descriptor_heap::get_vk_allocator()
{
   return &device->vk.alloc;
}

dzn_descriptor_heap::operator ID3D12DescriptorHeap *()
{
   return heap.Get();
}

SIZE_T
dzn_descriptor_heap::get_cpu_ptr(uint32_t desc_offset) const
{
   assert(desc_offset < desc_count);
   return cpu_base + (desc_offset * desc_sz);
}

void
dzn_descriptor_heap::write_desc(uint32_t desc_offset,
                                dzn_sampler *sampler)
{
   D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle {
      .ptr = get_cpu_ptr(desc_offset),
   };

   device->dev->CreateSampler(&sampler->desc, sampler_handle);
}

void
dzn_descriptor_heap::write_desc(uint32_t desc_offset,
                                dzn_image_view *iview)
{
   D3D12_CPU_DESCRIPTOR_HANDLE view_handle {
      .ptr = get_cpu_ptr(desc_offset),
   };

   device->dev->CreateShaderResourceView(iview->image->res.Get(), &iview->desc, view_handle);
}

dzn_buffer_desc::dzn_buffer_desc(VkDescriptorType t,
                                 const VkDescriptorBufferInfo *pBufferInfo)
{
   VK_FROM_HANDLE(dzn_buffer, buf, pBufferInfo->buffer);

   type = t;
   buffer = buf;
   range = pBufferInfo->range;
   offset = pBufferInfo->offset;
}

void
dzn_descriptor_heap::write_desc(uint32_t desc_offset,
                                const dzn_buffer_desc &info)
{
   D3D12_CPU_DESCRIPTOR_HANDLE view_handle {
      .ptr = get_cpu_ptr(desc_offset),
   };

   VkDeviceSize size =
      info.range == VK_WHOLE_SIZE ?
      info.buffer->size - info.offset :
      info.range;

   if (info.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
       info.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
      D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {
         .BufferLocation = info.buffer->res->GetGPUVirtualAddress() + info.offset,
         .SizeInBytes = ALIGN_POT(size, 256),
      };
      device->dev->CreateConstantBufferView(&cbv_desc, view_handle);
   } else {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
         .Format = DXGI_FORMAT_R32_TYPELESS,
         .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
         .Buffer = {
            .FirstElement = info.offset / sizeof(uint32_t),
            .NumElements = (UINT)size / sizeof(uint32_t),
            .Flags = D3D12_BUFFER_UAV_FLAG_RAW,
         },
      };
      device->dev->CreateUnorderedAccessView(info.buffer->res.Get(), NULL, &uav_desc, view_handle);
   }
}

void
dzn_descriptor_heap::copy(uint32_t dst_offset,
                          const dzn_descriptor_heap &src_heap,
                          uint32_t src_offset,
                          uint32_t desc_count)
{
   D3D12_CPU_DESCRIPTOR_HANDLE dst_handle = {
      .ptr = get_cpu_ptr(dst_offset),
   };
   D3D12_CPU_DESCRIPTOR_HANDLE src_handle = {
      .ptr = src_heap.get_cpu_ptr(src_offset),
   };

   device->dev->CopyDescriptorsSimple(desc_count,
                                      dst_handle,
                                      src_handle,
                                      type);
}

dzn_descriptor_set::dzn_descriptor_set(dzn_device *device,
                                       dzn_descriptor_pool *pool,
                                       VkDescriptorSetLayout l,
                                       const VkAllocationCallbacks *pAllocator)
{
   layout = dzn_descriptor_set_layout_from_handle(l);

   dzn_foreach_pool_type (type) {
      if (layout->range_desc_count[type] == 0)
         continue;

      heaps[type] =
         dzn_descriptor_heap(device, type, layout->range_desc_count[type], false);
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
   VK_MULTIALLOC_DECL(&ma, dzn_buffer_desc,
                      dynamic_buffers, layout->dynamic_buffers.count);

   if (!vk_multialloc_zalloc(&ma, &device->vk.alloc,
                             VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
      return NULL;

   set->dynamic_buffers = dynamic_buffers;
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

void
dzn_descriptor_set::write(const VkWriteDescriptorSet *pDescriptorWrite)
{
   uint32_t b = pDescriptorWrite->dstBinding;
   uint32_t offset = pDescriptorWrite->dstArrayElement;
   uint32_t d = 0;
   uint32_t desc_count = layout->get_desc_count(b);

   for (uint32_t d = 0; d < pDescriptorWrite->descriptorCount;) {
      if (b >= layout->binding_count)
         break;

      if (offset >= desc_count) {
         offset -= desc_count;
         b++;
         desc_count = layout->get_desc_count(b);
         continue;
      }

      switch (pDescriptorWrite->descriptorType) {
      case VK_DESCRIPTOR_TYPE_SAMPLER: {
         const VkDescriptorImageInfo *pImageInfo = pDescriptorWrite->pImageInfo + d;
         VK_FROM_HANDLE(dzn_sampler, sampler, pImageInfo->sampler);
         write_desc(b, offset, sampler);
         break;
      }
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
         const VkDescriptorImageInfo *pImageInfo = pDescriptorWrite->pImageInfo + d;
         VK_FROM_HANDLE(dzn_sampler, sampler, pImageInfo->sampler);
         write_desc(b, offset, sampler);
         VK_FROM_HANDLE(dzn_image_view, iview, pImageInfo->imageView);
         write_desc(b, offset, iview);
         break;
      }
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
         const VkDescriptorImageInfo *pImageInfo = pDescriptorWrite->pImageInfo + d;
         VK_FROM_HANDLE(dzn_image_view, iview, pImageInfo->imageView);
         write_desc(b, offset, iview);
         break;
      }
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
         const dzn_buffer_desc desc(pDescriptorWrite->descriptorType,
                                    pDescriptorWrite->pBufferInfo + d);
         write_desc(b, offset, desc);
         break;
      }
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
         const dzn_buffer_desc desc(pDescriptorWrite->descriptorType,
                                    pDescriptorWrite->pBufferInfo + d);
         write_dynamic_buffer_desc(b, offset, desc);
         break;
      }
      default:
         unreachable("invalid descriptor type");
         break;
      }

      offset++;
      d++;
   }
}

void
dzn_descriptor_set::write_desc(uint32_t b, uint32_t desc, dzn_sampler *sampler)
{
   D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
   uint32_t heap_offset = layout->get_heap_offset(b, type);
   if (heap_offset != ~0)
      heaps[type].write_desc(heap_offset + desc, sampler);
}

void
dzn_descriptor_set::write_dynamic_buffer_desc(uint32_t b, uint32_t desc,
                                              const dzn_buffer_desc &info)
{
   uint32_t dynamic_buffer_idx = layout->bindings[b].dynamic_buffer_idx;

   assert(dynamic_buffer_idx < layout->dynamic_buffers.count);
   dynamic_buffers[dynamic_buffer_idx + desc] = info;
}


template<typename ... Args> void
dzn_descriptor_set::write_desc(uint32_t b, uint32_t desc, Args... args)
{
   D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   uint32_t heap_offset = layout->get_heap_offset(b, type);
   if (heap_offset != ~0)
      heaps[type].write_desc(heap_offset + desc, args...);
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

   for (unsigned i = 0; i < descriptorWriteCount; i++) {
      VK_FROM_HANDLE(dzn_descriptor_set, set, pDescriptorWrites[i].dstSet);
      set->write(&pDescriptorWrites[i]);
   }

   for (unsigned i = 0; i < descriptorCopyCount; i++)
      dzn_copy_descriptor_set(dev, &pDescriptorCopies[i]);
}
