/*
 * Copyright © 2020 Intel Corporation
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

#include "vk_device.h"

#include "vk_common_entrypoints.h"
#include "vk_instance.h"
#include "vk_physical_device.h"
#include "vk_util.h"
#include "util/hash_table.h"
#include "util/ralloc.h"

VkResult
vk_device_init(struct vk_device *device,
               struct vk_physical_device *physical_device,
               const struct vk_device_dispatch_table *dispatch_table,
               const VkDeviceCreateInfo *pCreateInfo,
               const VkAllocationCallbacks *alloc)
{
   memset(device, 0, sizeof(*device));
   vk_object_base_init(device, &device->base, VK_OBJECT_TYPE_DEVICE);
   if (alloc != NULL)
      device->alloc = *alloc;
   else
      device->alloc = physical_device->instance->alloc;

   device->physical = physical_device;

   device->dispatch_table = *dispatch_table;

   /* Add common entrypoints without overwriting driver-provided ones. */
   vk_device_dispatch_table_from_entrypoints(
      &device->dispatch_table, &vk_common_device_entrypoints, false);

   for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; i++) {
      int idx;
      for (idx = 0; idx < VK_DEVICE_EXTENSION_COUNT; idx++) {
         if (strcmp(pCreateInfo->ppEnabledExtensionNames[i],
                    vk_device_extensions[idx].extensionName) == 0)
            break;
      }

      if (idx >= VK_DEVICE_EXTENSION_COUNT)
         return VK_ERROR_EXTENSION_NOT_PRESENT;

      if (!physical_device->supported_extensions.extensions[idx])
         return VK_ERROR_EXTENSION_NOT_PRESENT;

#ifdef ANDROID
      if (!vk_android_allowed_device_extensions.extensions[idx])
         return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif

      device->enabled_extensions.extensions[idx] = true;
   }

   p_atomic_set(&device->private_data_next_index, 0);

#ifdef ANDROID
   mtx_init(&device->swapchain_private_mtx, mtx_plain);
   device->swapchain_private = NULL;
#endif /* ANDROID */

   return VK_SUCCESS;
}

void
vk_device_finish(UNUSED struct vk_device *device)
{
#ifdef ANDROID
   if (device->swapchain_private) {
      hash_table_foreach(device->swapchain_private, entry)
         util_sparse_array_finish(entry->data);
      ralloc_free(device->swapchain_private);
   }
#endif /* ANDROID */

   vk_object_base_finish(&device->base);
}

PFN_vkVoidFunction
vk_device_get_proc_addr(const struct vk_device *device,
                        const char *name)
{
   if (device == NULL || name == NULL)
      return NULL;

   struct vk_instance *instance = device->physical->instance;
   return vk_device_dispatch_table_get_if_supported(&device->dispatch_table,
                                                    name,
                                                    instance->app_info.api_version,
                                                    &instance->enabled_extensions,
                                                    &device->enabled_extensions);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vk_common_GetDeviceProcAddr(VkDevice _device,
                            const char *pName)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   return vk_device_get_proc_addr(device, pName);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_GetDeviceQueue(VkDevice _device,
                         uint32_t queueFamilyIndex,
                         uint32_t queueIndex,
                         VkQueue *pQueue)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   const VkDeviceQueueInfo2 info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
      .pNext = NULL,
      /* flags = 0 because (Vulkan spec 1.2.170 - vkGetDeviceQueue):
       *
       *    "vkGetDeviceQueue must only be used to get queues that were
       *     created with the flags parameter of VkDeviceQueueCreateInfo set
       *     to zero. To get queues that were created with a non-zero flags
       *     parameter use vkGetDeviceQueue2."
       */
      .flags = 0,
      .queueFamilyIndex = queueFamilyIndex,
      .queueIndex = queueIndex,
   };

   device->dispatch_table.GetDeviceQueue2(_device, &info, pQueue);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_GetBufferMemoryRequirements(VkDevice _device,
                                      VkBuffer buffer,
                                      VkMemoryRequirements *pMemoryRequirements)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   VkBufferMemoryRequirementsInfo2 info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
      .buffer = buffer,
   };
   VkMemoryRequirements2 reqs = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
   };
   device->dispatch_table.GetBufferMemoryRequirements2(_device, &info, &reqs);

   *pMemoryRequirements = reqs.memoryRequirements;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_BindBufferMemory(VkDevice _device,
                           VkBuffer buffer,
                           VkDeviceMemory memory,
                           VkDeviceSize memoryOffset)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   VkBindBufferMemoryInfo bind = {
      .sType         = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
      .buffer        = buffer,
      .memory        = memory,
      .memoryOffset  = memoryOffset,
   };

   return device->dispatch_table.BindBufferMemory2(_device, 1, &bind);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_GetImageMemoryRequirements(VkDevice _device,
                                     VkImage image,
                                     VkMemoryRequirements *pMemoryRequirements)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   VkImageMemoryRequirementsInfo2 info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
      .image = image,
   };
   VkMemoryRequirements2 reqs = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
   };
   device->dispatch_table.GetImageMemoryRequirements2(_device, &info, &reqs);

   *pMemoryRequirements = reqs.memoryRequirements;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_BindImageMemory(VkDevice _device,
                          VkImage image,
                          VkDeviceMemory memory,
                          VkDeviceSize memoryOffset)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   VkBindImageMemoryInfo bind = {
      .sType         = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .image         = image,
      .memory        = memory,
      .memoryOffset  = memoryOffset,
   };

   return device->dispatch_table.BindImageMemory2(_device, 1, &bind);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_GetImageSparseMemoryRequirements(VkDevice _device,
                                           VkImage image,
                                           uint32_t *pSparseMemoryRequirementCount,
                                           VkSparseImageMemoryRequirements *pSparseMemoryRequirements)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   VkImageSparseMemoryRequirementsInfo2 info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2,
      .image = image,
   };

   if (!pSparseMemoryRequirements) {
      device->dispatch_table.GetImageSparseMemoryRequirements2(_device,
                                                               &info,
                                                               pSparseMemoryRequirementCount,
                                                               NULL);
      return;
   }

   STACK_ARRAY(VkSparseImageMemoryRequirements2, mem_reqs2, *pSparseMemoryRequirementCount);

   for (unsigned i = 0; i < *pSparseMemoryRequirementCount; ++i) {
      mem_reqs2[i].sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2;
      mem_reqs2[i].pNext = NULL;
   }

   device->dispatch_table.GetImageSparseMemoryRequirements2(_device,
                                                            &info,
                                                            pSparseMemoryRequirementCount,
                                                            mem_reqs2);

   for (unsigned i = 0; i < *pSparseMemoryRequirementCount; ++i)
      pSparseMemoryRequirements[i] = mem_reqs2[i].memoryRequirements;

   STACK_ARRAY_FINISH(mem_reqs2);
}

static void
copy_vk_struct_guts(VkBaseOutStructure *dst, VkBaseInStructure *src, size_t struct_size)
{
   STATIC_ASSERT(sizeof(*dst) == sizeof(*src));
   memcpy(dst + 1, src + 1, struct_size - sizeof(VkBaseOutStructure));
}

bool
vk_get_physical_device_core_feature_ext(struct VkBaseOutStructure *ext,
                                        const VkPhysicalDeviceVulkan11Features *core_1_1,
                                        const VkPhysicalDeviceVulkan12Features *core_1_2)
{
#define CORE_FEATURE(major, minor, feature) \
   features->feature = core_##major##_##minor->feature

   switch (ext->sType) {
   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR: {
      VkPhysicalDevice8BitStorageFeaturesKHR *features = (void *)ext;
      CORE_FEATURE(1, 2, storageBuffer8BitAccess);
      CORE_FEATURE(1, 2, uniformAndStorageBuffer8BitAccess);
      CORE_FEATURE(1, 2, storagePushConstant8);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES: {
      VkPhysicalDevice16BitStorageFeatures *features = (void *)ext;
      CORE_FEATURE(1, 1, storageBuffer16BitAccess);
      CORE_FEATURE(1, 1, uniformAndStorageBuffer16BitAccess);
      CORE_FEATURE(1, 1, storagePushConstant16);
      CORE_FEATURE(1, 1, storageInputOutput16);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR: {
      VkPhysicalDeviceBufferDeviceAddressFeaturesKHR *features = (void *)ext;
      CORE_FEATURE(1, 2, bufferDeviceAddress);
      CORE_FEATURE(1, 2, bufferDeviceAddressCaptureReplay);
      CORE_FEATURE(1, 2, bufferDeviceAddressMultiDevice);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT: {
      VkPhysicalDeviceDescriptorIndexingFeaturesEXT *features = (void *)ext;
      CORE_FEATURE(1, 2, shaderInputAttachmentArrayDynamicIndexing);
      CORE_FEATURE(1, 2, shaderUniformTexelBufferArrayDynamicIndexing);
      CORE_FEATURE(1, 2, shaderStorageTexelBufferArrayDynamicIndexing);
      CORE_FEATURE(1, 2, shaderUniformBufferArrayNonUniformIndexing);
      CORE_FEATURE(1, 2, shaderSampledImageArrayNonUniformIndexing);
      CORE_FEATURE(1, 2, shaderStorageBufferArrayNonUniformIndexing);
      CORE_FEATURE(1, 2, shaderStorageImageArrayNonUniformIndexing);
      CORE_FEATURE(1, 2, shaderInputAttachmentArrayNonUniformIndexing);
      CORE_FEATURE(1, 2, shaderUniformTexelBufferArrayNonUniformIndexing);
      CORE_FEATURE(1, 2, shaderStorageTexelBufferArrayNonUniformIndexing);
      CORE_FEATURE(1, 2, descriptorBindingUniformBufferUpdateAfterBind);
      CORE_FEATURE(1, 2, descriptorBindingSampledImageUpdateAfterBind);
      CORE_FEATURE(1, 2, descriptorBindingStorageImageUpdateAfterBind);
      CORE_FEATURE(1, 2, descriptorBindingStorageBufferUpdateAfterBind);
      CORE_FEATURE(1, 2, descriptorBindingUniformTexelBufferUpdateAfterBind);
      CORE_FEATURE(1, 2, descriptorBindingStorageTexelBufferUpdateAfterBind);
      CORE_FEATURE(1, 2, descriptorBindingUpdateUnusedWhilePending);
      CORE_FEATURE(1, 2, descriptorBindingPartiallyBound);
      CORE_FEATURE(1, 2, descriptorBindingVariableDescriptorCount);
      CORE_FEATURE(1, 2, runtimeDescriptorArray);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR: {
      VkPhysicalDeviceFloat16Int8FeaturesKHR *features = (void *)ext;
      CORE_FEATURE(1, 2, shaderFloat16);
      CORE_FEATURE(1, 2, shaderInt8);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT: {
      VkPhysicalDeviceHostQueryResetFeaturesEXT *features = (void *)ext;
      CORE_FEATURE(1, 2, hostQueryReset);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES_KHR: {
      VkPhysicalDeviceImagelessFramebufferFeaturesKHR *features = (void *)ext;
      CORE_FEATURE(1, 2, imagelessFramebuffer);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES: {
      VkPhysicalDeviceMultiviewFeatures *features = (void *)ext;
      CORE_FEATURE(1, 1, multiview);
      CORE_FEATURE(1, 1, multiviewGeometryShader);
      CORE_FEATURE(1, 1, multiviewTessellationShader);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES: {
      VkPhysicalDeviceProtectedMemoryFeatures *features = (void *)ext;
      CORE_FEATURE(1, 1, protectedMemory);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES: {
      VkPhysicalDeviceSamplerYcbcrConversionFeatures *features = (void *) ext;
      CORE_FEATURE(1, 1, samplerYcbcrConversion);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT: {
      VkPhysicalDeviceScalarBlockLayoutFeaturesEXT *features =(void *)ext;
      CORE_FEATURE(1, 2, scalarBlockLayout);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES_KHR: {
      VkPhysicalDeviceSeparateDepthStencilLayoutsFeaturesKHR *features = (void *)ext;
      CORE_FEATURE(1, 2, separateDepthStencilLayouts);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR: {
      VkPhysicalDeviceShaderAtomicInt64FeaturesKHR *features = (void *)ext;
      CORE_FEATURE(1, 2, shaderBufferInt64Atomics);
      CORE_FEATURE(1, 2, shaderSharedInt64Atomics);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES: {
      VkPhysicalDeviceShaderDrawParametersFeatures *features = (void *)ext;
      CORE_FEATURE(1, 1, shaderDrawParameters);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES_KHR: {
      VkPhysicalDeviceShaderSubgroupExtendedTypesFeaturesKHR *features = (void *)ext;
      CORE_FEATURE(1, 2, shaderSubgroupExtendedTypes);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR: {
      VkPhysicalDeviceTimelineSemaphoreFeaturesKHR *features = (void *) ext;
      CORE_FEATURE(1, 2, timelineSemaphore);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES_KHR: {
      VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR *features = (void *)ext;
      CORE_FEATURE(1, 2, uniformBufferStandardLayout);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES: {
      VkPhysicalDeviceVariablePointersFeatures *features = (void *)ext;
      CORE_FEATURE(1, 1, variablePointersStorageBuffer);
      CORE_FEATURE(1, 1, variablePointers);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR: {
      VkPhysicalDeviceVulkanMemoryModelFeaturesKHR *features = (void *)ext;
      CORE_FEATURE(1, 2, vulkanMemoryModel);
      CORE_FEATURE(1, 2, vulkanMemoryModelDeviceScope);
      CORE_FEATURE(1, 2, vulkanMemoryModelAvailabilityVisibilityChains);
      return true;

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
      copy_vk_struct_guts(ext, (void *)core_1_1, sizeof(*core_1_1));
      return true;

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
      copy_vk_struct_guts(ext, (void *)core_1_2, sizeof(*core_1_2));
      return true;
   }

   default:
      return false;
   }
#undef CORE_FEATURE
}

bool vk_get_physical_device_core_property_ext(struct VkBaseOutStructure *ext,
                                              const VkPhysicalDeviceVulkan11Properties *core_1_1,
                                              const VkPhysicalDeviceVulkan12Properties *core_1_2)
{
#define CORE_RENAMED_PROPERTY(major, minor, ext_property, core_property) \
   memcpy(&properties->ext_property, &core_##major##_##minor->core_property, \
          sizeof(core_##major##_##minor->core_property))

#define CORE_PROPERTY(major, minor, property) \
   CORE_RENAMED_PROPERTY(major, minor, property, property)

   switch (ext->sType) {
   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES_KHR: {
      VkPhysicalDeviceDepthStencilResolvePropertiesKHR *properties = (void *)ext;
      CORE_PROPERTY(1, 2, supportedDepthResolveModes);
      CORE_PROPERTY(1, 2, supportedStencilResolveModes);
      CORE_PROPERTY(1, 2, independentResolveNone);
      CORE_PROPERTY(1, 2, independentResolve);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT: {
      VkPhysicalDeviceDescriptorIndexingPropertiesEXT *properties = (void *)ext;
      CORE_PROPERTY(1, 2, maxUpdateAfterBindDescriptorsInAllPools);
      CORE_PROPERTY(1, 2, shaderUniformBufferArrayNonUniformIndexingNative);
      CORE_PROPERTY(1, 2, shaderSampledImageArrayNonUniformIndexingNative);
      CORE_PROPERTY(1, 2, shaderStorageBufferArrayNonUniformIndexingNative);
      CORE_PROPERTY(1, 2, shaderStorageImageArrayNonUniformIndexingNative);
      CORE_PROPERTY(1, 2, shaderInputAttachmentArrayNonUniformIndexingNative);
      CORE_PROPERTY(1, 2, robustBufferAccessUpdateAfterBind);
      CORE_PROPERTY(1, 2, quadDivergentImplicitLod);
      CORE_PROPERTY(1, 2, maxPerStageDescriptorUpdateAfterBindSamplers);
      CORE_PROPERTY(1, 2, maxPerStageDescriptorUpdateAfterBindUniformBuffers);
      CORE_PROPERTY(1, 2, maxPerStageDescriptorUpdateAfterBindStorageBuffers);
      CORE_PROPERTY(1, 2, maxPerStageDescriptorUpdateAfterBindSampledImages);
      CORE_PROPERTY(1, 2, maxPerStageDescriptorUpdateAfterBindStorageImages);
      CORE_PROPERTY(1, 2, maxPerStageDescriptorUpdateAfterBindInputAttachments);
      CORE_PROPERTY(1, 2, maxPerStageUpdateAfterBindResources);
      CORE_PROPERTY(1, 2, maxDescriptorSetUpdateAfterBindSamplers);
      CORE_PROPERTY(1, 2, maxDescriptorSetUpdateAfterBindUniformBuffers);
      CORE_PROPERTY(1, 2, maxDescriptorSetUpdateAfterBindUniformBuffersDynamic);
      CORE_PROPERTY(1, 2, maxDescriptorSetUpdateAfterBindStorageBuffers);
      CORE_PROPERTY(1, 2, maxDescriptorSetUpdateAfterBindStorageBuffersDynamic);
      CORE_PROPERTY(1, 2, maxDescriptorSetUpdateAfterBindSampledImages);
      CORE_PROPERTY(1, 2, maxDescriptorSetUpdateAfterBindStorageImages);
      CORE_PROPERTY(1, 2, maxDescriptorSetUpdateAfterBindInputAttachments);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR: {
      VkPhysicalDeviceDriverPropertiesKHR *properties = (void *) ext;
      CORE_PROPERTY(1, 2, driverID);
      CORE_PROPERTY(1, 2, driverName);
      CORE_PROPERTY(1, 2, driverInfo);
      CORE_PROPERTY(1, 2, conformanceVersion);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES: {
      VkPhysicalDeviceIDProperties *properties = (void *)ext;
      CORE_PROPERTY(1, 1, deviceUUID);
      CORE_PROPERTY(1, 1, driverUUID);
      CORE_PROPERTY(1, 1, deviceLUID);
      CORE_PROPERTY(1, 1, deviceLUIDValid);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES: {
      VkPhysicalDeviceMaintenance3Properties *properties = (void *)ext;
      /* This value doesn't matter for us today as our per-stage
       * descriptors are the real limit.
       */
      CORE_PROPERTY(1, 1, maxPerSetDescriptors);
      CORE_PROPERTY(1, 1, maxMemoryAllocationSize);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES: {
      VkPhysicalDeviceMultiviewProperties *properties = (void *)ext;
      CORE_PROPERTY(1, 1, maxMultiviewViewCount);
      CORE_PROPERTY(1, 1, maxMultiviewInstanceIndex);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES: {
      VkPhysicalDevicePointClippingProperties *properties = (void *) ext;
      CORE_PROPERTY(1, 1, pointClippingBehavior);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES: {
      VkPhysicalDeviceProtectedMemoryProperties *properties = (void *)ext;
      CORE_PROPERTY(1, 1, protectedNoFault);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT: {
      VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT *properties = (void *)ext;
      CORE_PROPERTY(1, 2, filterMinmaxImageComponentMapping);
      CORE_PROPERTY(1, 2, filterMinmaxSingleComponentFormats);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES: {
      VkPhysicalDeviceSubgroupProperties *properties = (void *)ext;
      CORE_PROPERTY(1, 1, subgroupSize);
      CORE_RENAMED_PROPERTY(1, 1, supportedStages,
                                    subgroupSupportedStages);
      CORE_RENAMED_PROPERTY(1, 1, supportedOperations,
                                    subgroupSupportedOperations);
      CORE_RENAMED_PROPERTY(1, 1, quadOperationsInAllStages,
                                    subgroupQuadOperationsInAllStages);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR : {
      VkPhysicalDeviceFloatControlsPropertiesKHR *properties = (void *)ext;
      CORE_PROPERTY(1, 2, denormBehaviorIndependence);
      CORE_PROPERTY(1, 2, roundingModeIndependence);
      CORE_PROPERTY(1, 2, shaderDenormFlushToZeroFloat16);
      CORE_PROPERTY(1, 2, shaderDenormPreserveFloat16);
      CORE_PROPERTY(1, 2, shaderRoundingModeRTEFloat16);
      CORE_PROPERTY(1, 2, shaderRoundingModeRTZFloat16);
      CORE_PROPERTY(1, 2, shaderSignedZeroInfNanPreserveFloat16);
      CORE_PROPERTY(1, 2, shaderDenormFlushToZeroFloat32);
      CORE_PROPERTY(1, 2, shaderDenormPreserveFloat32);
      CORE_PROPERTY(1, 2, shaderRoundingModeRTEFloat32);
      CORE_PROPERTY(1, 2, shaderRoundingModeRTZFloat32);
      CORE_PROPERTY(1, 2, shaderSignedZeroInfNanPreserveFloat32);
      CORE_PROPERTY(1, 2, shaderDenormFlushToZeroFloat64);
      CORE_PROPERTY(1, 2, shaderDenormPreserveFloat64);
      CORE_PROPERTY(1, 2, shaderRoundingModeRTEFloat64);
      CORE_PROPERTY(1, 2, shaderRoundingModeRTZFloat64);
      CORE_PROPERTY(1, 2, shaderSignedZeroInfNanPreserveFloat64);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES_KHR: {
      VkPhysicalDeviceTimelineSemaphorePropertiesKHR *properties = (void *) ext;
      CORE_PROPERTY(1, 2, maxTimelineSemaphoreValueDifference);
      return true;
   }

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES:
      copy_vk_struct_guts(ext, (void *)core_1_1, sizeof(*core_1_1));
      return true;

   case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES:
      copy_vk_struct_guts(ext, (void *)core_1_2, sizeof(*core_1_2));
      return true;

   default:
      return false;
   }

#undef CORE_RENAMED_PROPERTY
#undef CORE_PROPERTY
}
