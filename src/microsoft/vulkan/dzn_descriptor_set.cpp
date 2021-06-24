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
desc_type_to_range_type(VkDescriptorType in, bool writeable)
{
   switch (in) {
   case VK_DESCRIPTOR_TYPE_SAMPLER:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

   case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
   case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
   case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

   case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
   case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      return writeable ? D3D12_DESCRIPTOR_RANGE_TYPE_UAV : D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
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

   /* Some type map to an SRV or UAV depending on how the shaders is using the
    * resource (NONWRITEABLE flag set or not), in that case we need to reserve
    * slots for both the UAV and SRV descs.
    */
   if (dzn_descriptor_heap::type_depends_on_shader_usage(type))
      num_descs++;

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
      binfos[binding].type = desc_type;
      binfos[binding].visibility = visibility;
      binfos[binding].base_shader_register = base_register;
      assert(base_register + ordered_bindings[i].descriptorCount >= base_register);
      base_register += ordered_bindings[i].descriptorCount;

      if (immutable_samplers) {
         for (uint32_t s = 0; s < ordered_bindings[i].descriptorCount; s++) {
            binfos[binding].static_sampler_idx = static_sampler_idx;
            D3D12_STATIC_SAMPLER_DESC *desc = (D3D12_STATIC_SAMPLER_DESC *)
               &static_samplers[static_sampler_idx];
            VK_FROM_HANDLE(dzn_sampler, sampler, ordered_bindings[i].pImmutableSamplers[s]);
            desc->Filter = sampler->desc.Filter;
            desc->AddressU = sampler->desc.AddressU;
            desc->AddressV = sampler->desc.AddressV;
            desc->AddressW = sampler->desc.AddressW;
            desc->MipLODBias = sampler->desc.MipLODBias;
            desc->MaxAnisotropy = sampler->desc.MaxAnisotropy;
            desc->ComparisonFunc = sampler->desc.ComparisonFunc;
            desc->BorderColor = sampler->static_border_color;
            desc->MinLOD = sampler->desc.MinLOD;
            desc->MaxLOD = sampler->desc.MaxLOD;
            desc->ShaderRegister = binding;
            desc->ShaderVisibility = translate_desc_visibility(ordered_bindings[i].stageFlags);
            static_sampler_idx++;
         }
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
         range->RangeType = desc_type_to_range_type(range_type, false);
         range->NumDescriptors = ordered_bindings[i].descriptorCount;
         range->BaseShaderRegister = binfos[binding].base_shader_register;
         range->Flags = type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ?
            D3D12_DESCRIPTOR_RANGE_FLAG_NONE :
            D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS;
         if (is_dynamic) {
            range->OffsetInDescriptorsFromTableStart =
               dynamic_buffers.range_offset + dynamic_buffers.desc_count;
            dynamic_buffers.count += range->NumDescriptors;
            dynamic_buffers.desc_count += range->NumDescriptors;
         } else {
            range->OffsetInDescriptorsFromTableStart = range_desc_count[type];
            range_desc_count[type] += range->NumDescriptors;
         }

         if (!dzn_descriptor_heap::type_depends_on_shader_usage(desc_type))
            continue;

         assert(idx + 1 < range_count[visibility][type]);
         range_idx[visibility][type]++;
         range[1] = range[0];
         range++;
         range->RangeType = desc_type_to_range_type(range_type, true);
         if (is_dynamic) {
            range->OffsetInDescriptorsFromTableStart =
               dynamic_buffers.range_offset + dynamic_buffers.desc_count;
            dynamic_buffers.desc_count += range->NumDescriptors;
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

         if (dzn_descriptor_heap::type_depends_on_shader_usage(desc_type)) {
            range_count[visibility][D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]++;
            total_ranges++;
         }

         if (!is_dynamic_desc_type(desc_type)) {
            uint32_t factor =
               dzn_descriptor_heap::type_depends_on_shader_usage(desc_type) ? 2 : 1;
            dynamic_ranges_offset += bindings[i].descriptorCount * factor;
         }
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

   if (!vk_multialloc_zalloc2(&ma, &device->vk.alloc, pAllocator,
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
dzn_descriptor_set_layout::get_heap_offset(uint32_t b, D3D12_DESCRIPTOR_HEAP_TYPE type, bool writeable) const
{
   assert(b < binding_count);
   D3D12_SHADER_VISIBILITY visibility = bindings[b].visibility;
   assert(visibility < ARRAY_SIZE(ranges));
   assert(type < NUM_POOL_TYPES);

   uint32_t range_idx = bindings[b].range_idx[type];

   if (range_idx == ~0)
      return ~0;

   if (writeable &&
       !dzn_descriptor_heap::type_depends_on_shader_usage(bindings[b].type))
      return ~0;

   if (writeable)
      range_idx++;

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
      dxil_spirv_vulkan_binding *bindings = binding_translation[j].bindings;

      sets[j].dynamic_buffer_count = set_layout->dynamic_buffers.count;
      memcpy(sets[j].range_desc_count, set_layout->range_desc_count,
             sizeof(sets[j].range_desc_count));
      binding_translation[j].binding_count = set_layout->binding_count;
      for (uint32_t b = 0; b < set_layout->binding_count; b++)
         bindings[b].base_register = set_layout->bindings[b].base_shader_register;

      static_sampler_count += set_layout->static_sampler_count;
      dzn_foreach_pool_type (type) {
         sets[j].heap_offsets[type] = desc_count[type];
         desc_count[type] += set_layout->range_desc_count[type];
         for (uint32_t i = 0; i < MAX_SHADER_VISIBILITIES; i++)
            range_count += set_layout->range_count[i][type];
      }

      desc_count[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] +=
         set_layout->dynamic_buffers.desc_count;
      for (uint32_t o = 0, elem = 0; o < set_layout->dynamic_buffers.count; o++, elem++) {
         uint32_t b = set_layout->dynamic_buffers.bindings[o];

         if (o > 0 && set_layout->dynamic_buffers.bindings[o - 1] != b)
            elem = 0;

         sets[j].dynamic_buffer_heap_offsets[o].srv =
            set_layout->get_heap_offset(b, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false) + elem;
         sets[j].dynamic_buffer_heap_offsets[o].uav =
            set_layout->get_heap_offset(b, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true) + elem;
      }
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

dzn_pipeline_layout::dzn_pipeline_layout(dzn_device *device, const dzn_pipeline_layout &src)
{
   set_count = src.set_count;
   memcpy(sets, src.sets, set_count * sizeof(sets[0]));
   for (uint32_t s = 0; s < set_count; s++) {
      binding_translation[s].binding_count = src.binding_translation[s].binding_count;
      memcpy(binding_translation[s].bindings, src.binding_translation[s].bindings,
             binding_translation[s].binding_count * sizeof(binding_translation[0].bindings));
   }
   memcpy(desc_count, src.desc_count, sizeof(desc_count));
   root.param_count = src.root.param_count;
   root.sets_param_count = src.root.sets_param_count;
   root.sysval_cbv_param_idx = src.root.sysval_cbv_param_idx;
   memcpy(root.type, src.root.type, sizeof(root.type));
   root.sig = src.root.sig;

   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_PIPELINE_LAYOUT);
}

dzn_pipeline_layout::~dzn_pipeline_layout()
{
   vk_object_base_finish(&base);
}

dzn_pipeline_layout *
dzn_pipeline_layout_factory::allocate(dzn_device *device,
                                      const VkPipelineLayoutCreateInfo *info,
                                      const VkAllocationCallbacks *alloc)
{
   uint32_t binding_count = 0;

   for (uint32_t s = 0; s < info->setLayoutCount; s++) {
      VK_FROM_HANDLE(dzn_descriptor_set_layout, set_layout, info->pSetLayouts[s]);

      if (!set_layout)
         continue;

      binding_count += set_layout->binding_count;
   }

   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, dzn_pipeline_layout, layout, 1);
   VK_MULTIALLOC_DECL(&ma, dxil_spirv_vulkan_binding,
                      bindings, binding_count);

   if (!vk_multialloc_alloc2(&ma, &device->vk.alloc, alloc,
                             VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
      return NULL;

   for (uint32_t s = 0; s < info->setLayoutCount; s++) {
      VK_FROM_HANDLE(dzn_descriptor_set_layout, set_layout, info->pSetLayouts[s]);

      if (!set_layout || !set_layout->binding_count)
         continue;

      layout->binding_translation[s].bindings = bindings;
      bindings += set_layout->binding_count;
   }

   return layout;
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
   for (uint32_t p = 0; p < pCreateInfo->poolSizeCount; p++) {
      VkDescriptorType type = pCreateInfo->pPoolSizes[p].type;
      uint32_t num_desc = pCreateInfo->pPoolSizes[p].descriptorCount;

      switch (type) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
         desc_count[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] += num_desc;
         break;
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         desc_count[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] += num_desc;
         desc_count[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] += num_desc;
         break;
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
         desc_count[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] += num_desc;
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
         /* Reserve one UAV and one SRV slot for those. */
         desc_count[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] += num_desc * 2;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         break;
      default:
         unreachable("Unsupported desc type");
      }
   }

   alloc = pAllocator ? *pAllocator : device->vk.alloc;
   set_count = pCreateInfo->maxSets;
   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_DESCRIPTOR_POOL);

   for (uint32_t s = 0; s < set_count; s++)
      std::construct_at(&sets[s]);

   dzn_foreach_pool_type(type) {
      if (desc_count[type])
         heaps[type] = dzn_descriptor_heap(device, type, desc_count[type], false);
   }
}

dzn_descriptor_pool::~dzn_descriptor_pool()
{
   for (uint32_t s = 0; s < set_count; s++)
      std::destroy_at(&sets[s]);

   vk_object_base_finish(&base);
}

VkResult
dzn_descriptor_pool::defragment_heap(D3D12_DESCRIPTOR_HEAP_TYPE type)
{
   dzn_device *device = container_of(base.device, dzn_device, vk);

   try {
      dzn_descriptor_heap new_heap(device, type, heaps[type].get_desc_count(), false);

      std::lock_guard excl_lock(defragment_lock);
      uint32_t heap_offset = 0;
      for (uint32_t s = 0; s < set_count; s++) {
         if (!sets[s].layout)
            continue;

         new_heap.copy(heap_offset, heaps[type], sets[s].heap_offsets[type], sets[s].heap_sizes[type]);
         sets[s].heap_offsets[type] = heap_offset;
         heap_offset += sets[s].heap_sizes[type];
      }

      heaps[type] = new_heap;
      return VK_SUCCESS;
   } catch (VkResult error) {
      return error;
   } catch (...) {
      throw;
   }
}

VkResult
dzn_descriptor_pool::allocate_sets(dzn_device *device,
                                   const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                   VkDescriptorSet *pDescriptorSets)
{
   VkResult result;
   unsigned i;

   if (pAllocateInfo->descriptorSetCount > (set_count - used_set_count))
      return VK_ERROR_OUT_OF_POOL_MEMORY;

   uint32_t set_idx = 0;
   for (i = 0; i < pAllocateInfo->descriptorSetCount; i++) {
      VK_FROM_HANDLE(dzn_descriptor_set_layout, layout, pAllocateInfo->pSetLayouts[i]);

      dzn_foreach_pool_type(type) {
         if (used_desc_count[type] + layout->range_desc_count[type] > desc_count[type]) {
            free_sets(device, i, pDescriptorSets);
            result = VK_ERROR_OUT_OF_POOL_MEMORY;
            goto err_free_sets;
         }

         if (free_offset[type] + layout->range_desc_count[type] > desc_count[type]) {
            result = defragment_heap(type);
            if (result != VK_SUCCESS) {
               free_sets(device, i, pDescriptorSets);
               result = VK_ERROR_FRAGMENTED_POOL;
               goto err_free_sets;
            }
         }
      }

      dzn_descriptor_set *set = NULL;
      for (; set_idx < set_count; set_idx++) {
         if (!sets[set_idx].layout) {
            set = &sets[set_idx];
            break;
         }
      }

      set->layout = dzn_descriptor_set_layout_from_handle(pAllocateInfo->pSetLayouts[i]);
      set->pool = this;
      dzn_foreach_pool_type(type) {
         set->heap_offsets[type] = free_offset[type];
         set->heap_sizes[type] = layout->range_desc_count[type];
         free_offset[type] += layout->range_desc_count[type];
      }

      vk_object_base_init(&device->vk, &set->base, VK_OBJECT_TYPE_DESCRIPTOR_SET);
      pDescriptorSets[i] = dzn_descriptor_set_to_handle(set);
   }

   return VK_SUCCESS;

err_free_sets:
   free_sets(device, i, pDescriptorSets);
   for (i = 0; i < pAllocateInfo->descriptorSetCount; i++)
      pDescriptorSets[i] = VK_NULL_HANDLE;

   return result;
}

VkResult
dzn_descriptor_pool::free_sets(dzn_device *device,
                               uint32_t count,
                               const VkDescriptorSet *pDescriptorSets)
{
   for (uint32_t s = 0; s < count; s++) {
      VK_FROM_HANDLE(dzn_descriptor_set, set, pDescriptorSets[s]);

      assert(set->pool == this);

      set->layout = NULL;
      set->pool = NULL;
      vk_object_base_finish(&set->base);
   }

   dzn_foreach_pool_type(type)
      free_offset[type] = 0;

   for (uint32_t s = 0; s < set_count; s++) {
      const dzn_descriptor_set *set = &sets[s];

      if (set->layout) {
         dzn_foreach_pool_type(type) {
            free_offset[type] =
               MAX2(free_offset[type],
                    set->heap_offsets[type] +
                    set->layout->range_desc_count[type]);
         }
      }
   }

   return VK_SUCCESS;
}

VkResult
dzn_descriptor_pool::reset(dzn_device *device)
{
   for (uint32_t s = 0; s < set_count; s++) {
      sets[s].layout = NULL;
      sets[s].pool = NULL;
      vk_object_base_finish(&sets[s].base);
   }

   dzn_foreach_pool_type(type)
      free_offset[type] = 0;

   return VK_SUCCESS;
}

dzn_descriptor_pool *
dzn_descriptor_pool_factory::allocate(dzn_device *device,
                                      const VkDescriptorPoolCreateInfo *info,
                                      const VkAllocationCallbacks *alloc)
{
   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, dzn_descriptor_pool, pool, 1);
   VK_MULTIALLOC_DECL(&ma, dzn_descriptor_set, sets, info->maxSets);

   if (!vk_multialloc_zalloc2(&ma, &device->vk.alloc, alloc,
                              VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
      return NULL;

   pool->sets = sets;
   return pool;
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

VKAPI_ATTR VkResult VKAPI_CALL
dzn_ResetDescriptorPool(VkDevice device,
                        VkDescriptorPool descriptorPool,
                        VkDescriptorPoolResetFlags flags)
{
   VK_FROM_HANDLE(dzn_descriptor_pool, pool, descriptorPool);
   VK_FROM_HANDLE(dzn_device, dev, device);

   return pool->reset(dev);
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
   if (shader_visible)
      gpu_base = heap->GetGPUDescriptorHandleForHeapStart().ptr;
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

D3D12_CPU_DESCRIPTOR_HANDLE
dzn_descriptor_heap::get_cpu_handle(uint32_t desc_offset) const
{
   return D3D12_CPU_DESCRIPTOR_HANDLE {
      .ptr = cpu_base + (desc_offset * desc_sz),
   };
}

D3D12_GPU_DESCRIPTOR_HANDLE
dzn_descriptor_heap::get_gpu_handle(uint32_t desc_offset) const
{
   return D3D12_GPU_DESCRIPTOR_HANDLE {
      .ptr = gpu_base ? gpu_base + (desc_offset * desc_sz) : 0,
   };
}

void
dzn_descriptor_heap::write_desc(uint32_t desc_offset,
                                dzn_sampler *sampler)
{
   device->dev->CreateSampler(&sampler->desc, get_cpu_handle(desc_offset));
}

void
dzn_descriptor_heap::write_desc(uint32_t desc_offset,
                                bool writeable,
                                dzn_image_view *iview)
{
   D3D12_CPU_DESCRIPTOR_HANDLE view_handle = get_cpu_handle(desc_offset);

   if (writeable)
      device->dev->CreateUnorderedAccessView(iview->get_image()->res.Get(), NULL, &iview->uav_desc, view_handle);
   else
      device->dev->CreateShaderResourceView(iview->get_image()->res.Get(), &iview->desc, view_handle);
}

void
dzn_descriptor_heap::write_desc(uint32_t desc_offset,
                                bool writeable,
                                dzn_buffer_view *bview)
{
   D3D12_CPU_DESCRIPTOR_HANDLE view_handle = get_cpu_handle(desc_offset);

   if (writeable)
      device->dev->CreateUnorderedAccessView(bview->buffer->res.Get(), NULL, &bview->uav_desc, view_handle);
   else
      device->dev->CreateShaderResourceView(bview->buffer->res.Get(), &bview->srv_desc, view_handle);
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
                                bool writeable,
                                const dzn_buffer_desc &info)
{
   D3D12_CPU_DESCRIPTOR_HANDLE view_handle = get_cpu_handle(desc_offset);

   VkDeviceSize size =
      info.range == VK_WHOLE_SIZE ?
      info.buffer->size - info.offset :
      info.range;

   if (info.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
       info.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
      assert(!writeable);
      D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {
         .BufferLocation = info.buffer->res->GetGPUVirtualAddress() + info.offset,
         .SizeInBytes = ALIGN_POT(size, 256),
      };
      device->dev->CreateConstantBufferView(&cbv_desc, view_handle);
   } else if (writeable) {
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
   } else {
      D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
         .Format = DXGI_FORMAT_R32_TYPELESS,
         .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
         .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
         .Buffer = {
            .FirstElement = info.offset / sizeof(uint32_t),
            .NumElements = (UINT)size / sizeof(uint32_t),
            .Flags = D3D12_BUFFER_SRV_FLAG_RAW,
         },
      };
      device->dev->CreateShaderResourceView(info.buffer->res.Get(), &srv_desc, view_handle);
   }
}

void
dzn_descriptor_heap::copy(uint32_t dst_offset,
                          const dzn_descriptor_heap &src_heap,
                          uint32_t src_offset,
                          uint32_t desc_count)
{
   D3D12_CPU_DESCRIPTOR_HANDLE dst_handle = get_cpu_handle(dst_offset);
   D3D12_CPU_DESCRIPTOR_HANDLE src_handle = src_heap.get_cpu_handle(src_offset);

   device->dev->CopyDescriptorsSimple(desc_count,
                                      dst_handle,
                                      src_handle,
                                      type);
}

bool
dzn_descriptor_heap::type_depends_on_shader_usage(VkDescriptorType type)
{
   return type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
          type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
          type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
          type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
}

dzn_descriptor_heap_pool::dzn_descriptor_heap_pool(dzn_device *device,
		                                   D3D12_DESCRIPTOR_HEAP_TYPE t,
                                                   bool shader_vis,
                                                   const VkAllocationCallbacks *allocator) :
   type(t), shader_visible(shader_vis), heaps(heaps_allocator(allocator)),
   desc_sz(device->dev->GetDescriptorHandleIncrementSize(t))
{
   assert(!shader_visible ||
          type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
          type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

std::pair<dzn_descriptor_heap *, uint32_t>
dzn_descriptor_heap_pool::allocate(dzn_device *device, uint32_t desc_count)
{
   if (heaps.size() == 0 ||
       (offset + desc_count > heaps[heaps.size() - 1].get_desc_count())) {
      uint32_t granularity =
         (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
          type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) ?
         64 * 1024 : 4 * 1024;
      uint32_t alloc_step = ALIGN_POT(desc_count * desc_sz, granularity);
      uint32_t heap_desc_count = MAX2(alloc_step / desc_sz, 16);
      heaps.emplace_back(device, type, heap_desc_count, shader_visible);
      offset = 0;
   }

   uint32_t desc = offset;
   offset += desc_count;

   return std::pair<dzn_descriptor_heap *, uint32_t>(&heaps[heaps.size() - 1], desc);
}

void
dzn_descriptor_heap_pool::reset()
{
   offset = 0;
   heaps.clear();
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_AllocateDescriptorSets(VkDevice device,
                           const VkDescriptorSetAllocateInfo *pAllocateInfo,
                           VkDescriptorSet *pDescriptorSets)
{
   VK_FROM_HANDLE(dzn_descriptor_pool, pool, pAllocateInfo->descriptorPool);
   VK_FROM_HANDLE(dzn_device, dev, device);

   return pool->allocate_sets(dev, pAllocateInfo, pDescriptorSets);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_FreeDescriptorSets(VkDevice device,
                       VkDescriptorPool descriptorPool,
                       uint32_t count,
                       const VkDescriptorSet *pDescriptorSets)
{
   VK_FROM_HANDLE(dzn_descriptor_pool, pool, descriptorPool);
   VK_FROM_HANDLE(dzn_device, dev, device);

   return pool->free_sets(dev, count, pDescriptorSets);
}

dzn_descriptor_set::range::range(const dzn_descriptor_set_layout &l,
                                 uint32_t binding,
                                 uint32_t offset,
                                 uint32_t count) :
   layout(l)
{
   first.binding = last.binding = ~0;
   first.elem = last.elem = ~0;

   if (binding >= layout.binding_count)
      return;

   for (; binding < layout.binding_count; ++binding) {
      uint32_t desc_count = layout.get_desc_count(binding);

      if (offset < desc_count)
         break;

      offset -= desc_count;
   }

   if (binding >= layout.binding_count)
      return;

   uint32_t first_binding = binding, first_elem = offset;

   for (; binding < layout.binding_count; ++binding) {
      uint32_t desc_count = layout.get_desc_count(binding);

      if (count <= desc_count)
         break;

      count -= desc_count;
   }

   if (binding >= layout.binding_count)
      return;

   first.binding = first_binding;
   first.elem = first_elem;
   last.binding = binding;
   last.elem = count - 1;
}

dzn_descriptor_set::range::iterator
dzn_descriptor_set::range::begin()
{
   return iterator { first.binding, first.elem, *this };
}

dzn_descriptor_set::range::iterator
dzn_descriptor_set::range::end()
{
   return iterator { ~0UL, ~0UL, *this };
}

dzn_descriptor_set::range::iterator &
dzn_descriptor_set::range::iterator::operator+=(uint32_t count)
{
   if (binding == ~0)
      return *this;

   while (count) {
      uint32_t desc_count = range.layout.get_desc_count(binding);

      if (count >= desc_count - elem) {
         count -= desc_count - elem;
         binding++;
         elem = 0;
      } else {
         elem += count;
         count = 0;
      }
   }

   if (binding > range.last.binding ||
       (binding == range.last.binding && elem > range.last.elem)) {
      binding = ~0;
      elem = ~0;
   }

   return *this;
}

bool
dzn_descriptor_set::range::iterator::operator!=(const iterator &iter) const
{
   return memcmp(this, &iter, sizeof(iter)) != 0;
}

uint32_t
dzn_descriptor_set::range::iterator::get_heap_offset(D3D12_DESCRIPTOR_HEAP_TYPE type, bool writeable) const
{
   if (binding == ~0)
      return ~0;

   uint32_t base = range.layout.get_heap_offset(binding, type, writeable);
   if (base == ~0)
      return ~0;

   return base + elem;
}

uint32_t
dzn_descriptor_set::range::iterator::get_dynamic_buffer_idx() const
{
   if (binding == ~0)
      return ~0;

   uint32_t base = range.layout.bindings[binding].dynamic_buffer_idx;

   if (base == ~0)
      return ~0;

   return base + elem;
}

VkDescriptorType
dzn_descriptor_set::range::iterator::get_vk_type() const
{
   if (binding >= range.layout.binding_count)
      return (VkDescriptorType)~0;

   return range.layout.bindings[binding].type;
}

uint32_t
dzn_descriptor_set::range::iterator::remaining_descs_in_binding() const
{
   if (binding == ~0)
      return 0;

   return range.layout.get_desc_count(binding) - elem;
}

void
dzn_descriptor_set::write(const VkWriteDescriptorSet *pDescriptorWrite)
{
   range range(*this->layout, pDescriptorWrite->dstBinding,
               pDescriptorWrite->dstArrayElement,
               pDescriptorWrite->descriptorCount);
   uint32_t d = 0;

   switch (pDescriptorWrite->descriptorType) {
   case VK_DESCRIPTOR_TYPE_SAMPLER:
      for (auto iter : range) {
         assert(iter.get_vk_type() == pDescriptorWrite->descriptorType);
         const VkDescriptorImageInfo *pImageInfo = pDescriptorWrite->pImageInfo + d;
         VK_FROM_HANDLE(dzn_sampler, sampler, pImageInfo->sampler);
         write_desc(iter, sampler);
         d++;
      }
      break;
   case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      for (auto iter : range) {
         assert(iter.get_vk_type() == pDescriptorWrite->descriptorType);
         const VkDescriptorImageInfo *pImageInfo = pDescriptorWrite->pImageInfo + d;
         VK_FROM_HANDLE(dzn_sampler, sampler, pImageInfo->sampler);
         write_desc(iter, sampler);
         VK_FROM_HANDLE(dzn_image_view, iview, pImageInfo->imageView);
         write_desc(iter, iview);
         d++;
      }
      break;

   case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
   case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
   case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      for (auto iter : range) {
         assert(iter.get_vk_type() == pDescriptorWrite->descriptorType);
         const VkDescriptorImageInfo *pImageInfo = pDescriptorWrite->pImageInfo + d;
         VK_FROM_HANDLE(dzn_image_view, iview, pImageInfo->imageView);
         write_desc(iter, iview);
         d++;
      }
      break;
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      for (auto iter : range) {
         assert(iter.get_vk_type() == pDescriptorWrite->descriptorType);
         const dzn_buffer_desc desc(pDescriptorWrite->descriptorType,
                                    pDescriptorWrite->pBufferInfo + d);
         write_desc(iter, desc);
         d++;
      }
      break;

   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      for (auto iter : range) {
         assert(iter.get_vk_type() == pDescriptorWrite->descriptorType);
         const dzn_buffer_desc desc(pDescriptorWrite->descriptorType,
                                    pDescriptorWrite->pBufferInfo + d);
         write_dynamic_buffer_desc(iter, desc);
         d++;
      }
      break;

   case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
   case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      for (auto iter : range) {
         assert(iter.get_vk_type() == pDescriptorWrite->descriptorType);
         VK_FROM_HANDLE(dzn_buffer_view, bview, pDescriptorWrite->pTexelBufferView[d]);
         write_desc(iter, bview);
         d++;
      }
      break;

   default:
      unreachable("invalid descriptor type");
      break;
   }

   assert(d == pDescriptorWrite->descriptorCount);
}

void
dzn_descriptor_set::copy(const dzn_descriptor_set *src,
                         const VkCopyDescriptorSet *pDescriptorCopy)
{
   range src_range(*src->layout,
                   pDescriptorCopy->srcBinding,
                   pDescriptorCopy->srcArrayElement,
                   pDescriptorCopy->descriptorCount);
   range dst_range(*this->layout,
                   pDescriptorCopy->dstBinding,
                   pDescriptorCopy->dstArrayElement,
                   pDescriptorCopy->descriptorCount);
   uint32_t copied_count = 0;

   auto src_iter = src_range.begin();
   auto src_end = src_range.end();
   auto dst_iter = dst_range.begin();
   auto dst_end = dst_range.end();

   while (src_iter != src_end && dst_iter != dst_end) {
      VkDescriptorType src_type = src_iter.get_vk_type();
      VkDescriptorType dst_type = dst_iter.get_vk_type();
      assert(copied_count < pDescriptorCopy->descriptorCount);
      assert(src_type == dst_type);
      uint32_t count =
         MIN2(src_iter.remaining_descs_in_binding(),
              dst_iter.remaining_descs_in_binding());

      if (src_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
          src_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
         uint32_t src_idx = src_iter.get_dynamic_buffer_idx();
         uint32_t dst_idx = dst_iter.get_dynamic_buffer_idx();

         memcpy(&dynamic_buffers[dst_idx],
                &src->dynamic_buffers[src_idx],
                sizeof(*dynamic_buffers) * count);
      } else {
         dzn_foreach_pool_type(type) {
            uint32_t src_heap_offset = src_iter.get_heap_offset(type, false);
            uint32_t dst_heap_offset = dst_iter.get_heap_offset(type, false);

            if (src_heap_offset == ~0) {
               assert(dst_heap_offset == ~0);
               continue;
            }

            std::shared_lock src_lock(src->pool->defragment_lock);
            std::shared_lock dst_lock(pool->defragment_lock);
            pool->heaps[type].copy(heap_offsets[type] + dst_heap_offset,
                                   src->pool->heaps[type],
                                   src->heap_offsets[type] + src_heap_offset,
                                   count);

            if (!dzn_descriptor_heap::type_depends_on_shader_usage(src_type))
               continue;

            src_heap_offset = src_iter.get_heap_offset(type, true);
            dst_heap_offset = dst_iter.get_heap_offset(type, true);
            assert(src_heap_offset != ~0);
            assert(dst_heap_offset != ~0);
            pool->heaps[type].copy(heap_offsets[type] + dst_heap_offset,
                                   src->pool->heaps[type],
                                   src->heap_offsets[type] + src_heap_offset,
                                   count);
         }
      }

      src_iter += count;
      dst_iter += count;
      copied_count += count;
   }

   assert(copied_count == pDescriptorCopy->descriptorCount);
}

void
dzn_descriptor_set::write_desc(const range::iterator &iter,
                               dzn_sampler *sampler)
{
   D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
   uint32_t heap_offset = iter.get_heap_offset(type);
   if (heap_offset != ~0) {
      std::shared_lock lock(pool->defragment_lock);
      pool->heaps[type].write_desc(heap_offsets[type] + heap_offset, sampler);
   }
}

void
dzn_descriptor_set::write_dynamic_buffer_desc(const range::iterator &iter,
                                              const dzn_buffer_desc &info)
{
   uint32_t dynamic_buffer_idx = iter.get_dynamic_buffer_idx();
   if (dynamic_buffer_idx == ~0)
      return;

   assert(dynamic_buffer_idx < layout->dynamic_buffers.count);
   dynamic_buffers[dynamic_buffer_idx] = info;
}


template<typename ... Args> void
dzn_descriptor_set::write_desc(const range::iterator &iter,
                               Args... args)
{
   D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   uint32_t heap_offset = iter.get_heap_offset(type, false);
   if (heap_offset == ~0)
      return;

   std::shared_lock lock(pool->defragment_lock);
   pool->heaps[type].write_desc(heap_offsets[type] + heap_offset, false, args...);

   if (dzn_descriptor_heap::type_depends_on_shader_usage(iter.get_vk_type())) {
      heap_offset = iter.get_heap_offset(type, true);
      assert(heap_offset != ~0);
      pool->heaps[type].write_desc(heap_offsets[type] + heap_offset, true, args...);
   }
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

   for (unsigned i = 0; i < descriptorCopyCount; i++) {
      VK_FROM_HANDLE(dzn_descriptor_set, src_set, pDescriptorCopies[i].srcSet);
      VK_FROM_HANDLE(dzn_descriptor_set, dst_set, pDescriptorCopies[i].dstSet);
      dst_set->copy(src_set, &pDescriptorCopies[i]);
   }
}
