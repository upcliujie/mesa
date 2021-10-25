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

#include "util/debug.h"
#include "util/macros.h"

#include "glsl_types.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <directx/d3d12sdklayers.h>

#if defined(VK_USE_PLATFORM_WIN32_KHR) || \
    defined(VK_USE_PLATFORM_DISPLAY_KHR)
#define DZN_USE_WSI_PLATFORM
#endif

#define DZN_API_VERSION VK_MAKE_VERSION(1, 0, VK_HEADER_VERSION)

static const vk_instance_extension_table instance_extensions = {
#ifdef DZN_USE_WSI_PLATFORM
   .KHR_surface                              = true,
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
   .KHR_win32_surface                        = true,
#endif
#ifdef VK_USE_PLATFORM_DISPLAY_KHR
   .KHR_display                              = true,
   .KHR_get_display_properties2              = true,
   .EXT_direct_mode_display                  = true,
   .EXT_display_surface_counter              = true,
#endif
   .EXT_debug_report                         = true,
};

static void
get_device_extensions(const dzn_physical_device *device,
                      vk_device_extension_table *ext)
{
   *ext = vk_device_extension_table {
#ifdef DZN_USE_WSI_PLATFORM
      .KHR_swapchain                         = true,
#endif
   };
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_EnumerateInstanceExtensionProperties(const char *pLayerName,
                                         uint32_t *pPropertyCount,
                                         VkExtensionProperties *pProperties)
{
   /* We don't support any layers  */
   if (pLayerName)
      return vk_error(NULL, VK_ERROR_LAYER_NOT_PRESENT);

   return vk_enumerate_instance_extension_properties(
      &instance_extensions, pPropertyCount, pProperties);
}

static const struct debug_control dzn_debug_options[] = {
   { "sync", DZN_DEBUG_SYNC },
   { "nir", DZN_DEBUG_NIR },
   { "dxil", DZN_DEBUG_DXIL },
   { "warp", DZN_DEBUG_WARP },
   { NULL, 0 }
};

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                   const VkAllocationCallbacks *pAllocator,
                   VkInstance *pInstance)
{
   dzn_instance *instance;
   VkResult result;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);

   if (pAllocator == NULL)
      pAllocator = vk_default_allocator();

   instance = (dzn_instance *)
      vk_zalloc(pAllocator, sizeof(*instance), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!instance)
      return vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_instance_dispatch_table dispatch_table;
   vk_instance_dispatch_table_from_entrypoints(
      &dispatch_table, &dzn_instance_entrypoints, true);

   result = vk_instance_init(&instance->vk, &instance_extensions,
                             &dispatch_table, pCreateInfo, pAllocator);
   if (result != VK_SUCCESS) {
      vk_free(pAllocator, instance);
      return vk_error(NULL, result);
   }

   instance->physical_devices_enumerated = false;
   list_inithead(&instance->physical_devices);
   instance->debug_flags =
      parse_debug_string(getenv("DZN_DEBUG"), dzn_debug_options);

   *pInstance = dzn_instance_to_handle(instance);

   return VK_SUCCESS;
}

static void
dzn_physical_device_destroy(dzn_physical_device *device)
{
   dzn_instance *instance = device->instance;

   dzn_wsi_finish(device);
   device->adapter->Release();
   vk_free(&instance->vk.alloc, device);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyInstance(VkInstance _instance,
                    const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(dzn_instance, instance, _instance);

   if (!instance)
      return;

   list_for_each_entry_safe(dzn_physical_device, pdevice,
                            &instance->physical_devices, link) {
      list_del(&pdevice->link);
      dzn_physical_device_destroy(pdevice);
   }

   vk_instance_finish(&instance->vk);
   vk_free(&instance->vk.alloc, instance);
}

static VkResult
create_pysical_device(dzn_instance *instance,
                      IDXGIAdapter1 *adapter,
                      dzn_physical_device **device_out)
{
   VkResult result;
   VkPhysicalDeviceMemoryProperties *mem;

   dzn_physical_device *device = (dzn_physical_device *)vk_zalloc(
      &instance->vk.alloc, sizeof(*device), 8,
      VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (device == NULL)
      return vk_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_physical_device_dispatch_table dispatch_table;
   vk_physical_device_dispatch_table_from_entrypoints(
      &dispatch_table, &dzn_physical_device_entrypoints, true);
   vk_physical_device_dispatch_table_from_entrypoints(&dispatch_table,
                                                      &wsi_physical_device_entrypoints,
                                                      false);

   result = vk_physical_device_init(&device->vk, &instance->vk,
                                    NULL, /* We set up extensions later */
                                    &dispatch_table);
   if (result != VK_SUCCESS) {
      vk_error(instance, result);
      goto fail;
   }
   device->instance = instance;
   device->adapter = adapter;

   vk_warn_non_conformant_implementation("dzn");

   /* TODO: correct UUIDs */
   memset(device->pipeline_cache_uuid, 0, VK_UUID_SIZE);
   memset(device->driver_uuid, 0, VK_UUID_SIZE);
   memset(device->device_uuid, 0, VK_UUID_SIZE);

   mem = &device->memory;

   DXGI_ADAPTER_DESC1 desc;
   adapter->GetDesc1(&desc);

   mem->memoryHeapCount = 1;
   mem->memoryHeaps[0] = VkMemoryHeap {
      .size = desc.SharedSystemMemory,
      .flags = 0,
   };

   mem->memoryTypeCount = 2;
   mem->memoryTypes[0] = VkMemoryType {
      /* TODO: This should also have VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
       * in the CacheCoherentUMA-case; we should probably use
       * GetCustomHeapProperties to populate these flags instead.
       */
      .propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
      .heapIndex = 0,
   };
   mem->memoryTypes[1] = VkMemoryType {
      .propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
     .heapIndex = 0,
   };

   if (desc.DedicatedVideoMemory > 0) {
      mem->memoryHeaps[mem->memoryHeapCount++] = VkMemoryHeap {
         .size = desc.DedicatedVideoMemory,
         .flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT,
      };
      mem->memoryTypes[mem->memoryTypeCount++] = VkMemoryType {
         .propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
         .heapIndex = mem->memoryTypeCount,
      };
   } else {
      mem->memoryHeaps[0].flags |= VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
      mem->memoryTypes[0].propertyFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      mem->memoryTypes[1].propertyFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
   }

   /* TODO: something something queue families */

   result = dzn_wsi_init(device);
   if (result != VK_SUCCESS) {
      vk_error(instance, result);
      goto fail;
   }

   get_device_extensions(device, &device->vk.supported_extensions);

   *device_out = device;

   return VK_SUCCESS;

fail:
   vk_free(&instance->vk.alloc, device);
   return result;
}

static VkResult
dzn_enumerate_physical_devices(dzn_instance *instance)
{
   if (instance->physical_devices_enumerated)
      return VK_SUCCESS;

   instance->physical_devices_enumerated = true;

   ComPtr<IDXGIFactory4> factory = dxgi_get_factory(false);
   IDXGIAdapter1 *adapter;
   VkResult result = VK_SUCCESS;
   for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); ++i) {
      if (instance->debug_flags & DZN_DEBUG_WARP) {
         DXGI_ADAPTER_DESC1 desc;
         adapter->GetDesc1(&desc);
         if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
            adapter->Release();
            continue;
         }
      }

      dzn_physical_device *pdevice;
      result = create_pysical_device(instance, adapter, &pdevice);
      if (result != VK_SUCCESS)
         break;

      list_addtail(&pdevice->link, &instance->physical_devices);
   }

   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_EnumeratePhysicalDevices(VkInstance _instance,
                             uint32_t *pPhysicalDeviceCount,
                             VkPhysicalDevice *pPhysicalDevices)
{
   VK_FROM_HANDLE(dzn_instance, instance, _instance);
   VK_OUTARRAY_MAKE_TYPED(VkPhysicalDevice, out, pPhysicalDevices,
                          pPhysicalDeviceCount);

   VkResult result = dzn_enumerate_physical_devices(instance);
   if (result != VK_SUCCESS)
      return result;

   list_for_each_entry(dzn_physical_device, pdevice, &instance->physical_devices, link)
   {
      vk_outarray_append_typed(VkPhysicalDevice, &out, i)
      {
         *i = dzn_physical_device_to_handle(pdevice);
      }
   }

   return vk_outarray_status(&out);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_EnumerateInstanceVersion(uint32_t *pApiVersion)
{
    *pApiVersion = DZN_API_VERSION;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice,
                              VkPhysicalDeviceFeatures *pFeatures)
{
   *pFeatures = VkPhysicalDeviceFeatures {
      .robustBufferAccess = true, /* This feature is mandatory */
      .fullDrawIndexUint32 = false,
      .imageCubeArray = false,
      .independentBlend = false,
      .geometryShader = false,
      .tessellationShader = false,
      .sampleRateShading = false,
      .dualSrcBlend = false,
      .logicOp = false,
      .multiDrawIndirect = false,
      .drawIndirectFirstInstance = false,
      .depthClamp = false,
      .depthBiasClamp = false,
      .fillModeNonSolid = false,
      .depthBounds = false,
      .wideLines = false,
      .largePoints = false,
      .alphaToOne = false,
      .multiViewport = false,
      .samplerAnisotropy = false,
      .textureCompressionETC2 = false,
      .textureCompressionASTC_LDR = false,
      .textureCompressionBC = false,
      .occlusionQueryPrecise = false,
      .pipelineStatisticsQuery = false,
      .vertexPipelineStoresAndAtomics = false,
      .fragmentStoresAndAtomics = false,
      .shaderTessellationAndGeometryPointSize = false,
      .shaderImageGatherExtended = false,
      .shaderStorageImageExtendedFormats = false,
      .shaderStorageImageMultisample = false,
      .shaderStorageImageReadWithoutFormat = false,
      .shaderStorageImageWriteWithoutFormat = false,
      .shaderUniformBufferArrayDynamicIndexing = false,
      .shaderSampledImageArrayDynamicIndexing = false,
      .shaderStorageBufferArrayDynamicIndexing = false,
      .shaderStorageImageArrayDynamicIndexing = false,
      .shaderClipDistance = false,
      .shaderCullDistance = false,
      .shaderFloat64 = false,
      .shaderInt64 = false,
      .shaderInt16 = false,
      .shaderResourceResidency = false,
      .shaderResourceMinLod = false,
      .sparseBinding = false,
      .sparseResidencyBuffer = false,
      .sparseResidencyImage2D = false,
      .sparseResidencyImage3D = false,
      .sparseResidency2Samples = false,
      .sparseResidency4Samples = false,
      .sparseResidency8Samples = false,
      .sparseResidency16Samples = false,
      .sparseResidencyAliased = false,
      .variableMultisampleRate = false,
      .inheritedQueries = false,
   };
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice,
                               VkPhysicalDeviceFeatures2 *pFeatures)
{
   dzn_GetPhysicalDeviceFeatures(physicalDevice, &pFeatures->features);

   vk_foreach_struct(ext, pFeatures->pNext) {
      dzn_debug_ignored_stype(ext->sType);
   }
}


VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
dzn_GetInstanceProcAddr(VkInstance _instance,
                        const char *pName)
{
   VK_FROM_HANDLE(dzn_instance, instance, _instance);
   return vk_instance_get_proc_addr(&instance->vk,
                                    &dzn_instance_entrypoints,
                                    pName);
}

/* Windows will use a dll definition file to avoid build errors. */
#ifdef _WIN32
#undef PUBLIC
#define PUBLIC
#endif

/* With version 1+ of the loader interface the ICD should expose
 * vk_icdGetInstanceProcAddr to work around certain LD_PRELOAD issues seen in apps.
 */
PUBLIC VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vk_icdGetInstanceProcAddr(VkInstance instance,
                          const char *pName);

PUBLIC VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vk_icdGetInstanceProcAddr(VkInstance instance,
                          const char *pName)
{
   return dzn_GetInstanceProcAddr(instance, pName);
}

/* With version 4+ of the loader interface the ICD should expose
 * vk_icdGetPhysicalDeviceProcAddr()
 */
PUBLIC VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vk_icdGetPhysicalDeviceProcAddr(VkInstance  _instance,
                                const char* pName);

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vk_icdGetPhysicalDeviceProcAddr(VkInstance  _instance,
                                const char* pName)
{
   VK_FROM_HANDLE(dzn_instance, instance, _instance);
   return vk_instance_get_physical_device_proc_addr(&instance->vk, pName);
}

/* vk_icd.h does not declare this function, so we declare it here to
 * suppress Wmissing-prototypes.
 */
PUBLIC VKAPI_ATTR VkResult VKAPI_CALL
vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t* pSupportedVersion);

PUBLIC VKAPI_ATTR VkResult VKAPI_CALL
vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t* pSupportedVersion)
{
   /* For the full details on loader interface versioning, see
    * <https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/blob/master/loader/LoaderAndLayerInterface.md>.
    * What follows is a condensed summary, to help you navigate the large and
    * confusing official doc.
    *
    *   - Loader interface v0 is incompatible with later versions. We don't
    *     support it.
    *
    *   - In loader interface v1:
    *       - The first ICD entrypoint called by the loader is
    *         vk_icdGetInstanceProcAddr(). The ICD must statically expose this
    *         entrypoint.
    *       - The ICD must statically expose no other Vulkan symbol unless it is
    *         linked with -Bsymbolic.
    *       - Each dispatchable Vulkan handle created by the ICD must be
    *         a pointer to a struct whose first member is VK_LOADER_DATA. The
    *         ICD must initialize VK_LOADER_DATA.loadMagic to ICD_LOADER_MAGIC.
    *       - The loader implements vkCreate{PLATFORM}SurfaceKHR() and
    *         vkDestroySurfaceKHR(). The ICD must be capable of working with
    *         such loader-managed surfaces.
    *
    *    - Loader interface v2 differs from v1 in:
    *       - The first ICD entrypoint called by the loader is
    *         vk_icdNegotiateLoaderICDInterfaceVersion(). The ICD must
    *         statically expose this entrypoint.
    *
    *    - Loader interface v3 differs from v2 in:
    *        - The ICD must implement vkCreate{PLATFORM}SurfaceKHR(),
    *          vkDestroySurfaceKHR(), and other API which uses VKSurfaceKHR,
    *          because the loader no longer does so.
    *
    *    - Loader interface v4 differs from v3 in:
    *        - The ICD must implement vk_icdGetPhysicalDeviceProcAddr().
    */
   *pSupportedVersion = MIN2(*pSupportedVersion, 4u);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                                VkPhysicalDeviceProperties *pProperties)
{
   VK_FROM_HANDLE(dzn_physical_device, pdevice, physicalDevice);

   /* minimum from the spec */
   const VkSampleCountFlags supported_sample_counts =
      VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;

   /* FIXME: this is mostly bunk for now */
   VkPhysicalDeviceLimits limits = {

      /* TODO: support older feature levels */
      .maxImageDimension1D                      = (1 << 14),
      .maxImageDimension2D                      = (1 << 14),
      .maxImageDimension3D                      = (1 << 11),
      .maxImageDimensionCube                    = (1 << 14),
      .maxImageArrayLayers                      = (1 << 11),

      /* from here on, we simply use the minimum values from the spec for now */
      .maxTexelBufferElements                   = 65536,
      .maxUniformBufferRange                    = 16384,
      .maxStorageBufferRange                    = (1ul << 27),
      .maxPushConstantsSize                     = 128,
      .maxMemoryAllocationCount                 = 4096,
      .maxSamplerAllocationCount                = 4000,
      .bufferImageGranularity                   = 131072,
      .sparseAddressSpaceSize                   = 0,
      .maxBoundDescriptorSets                   = MAX_SETS,
      .maxPerStageDescriptorSamplers            = 16,
      .maxPerStageDescriptorUniformBuffers      = 12,
      .maxPerStageDescriptorStorageBuffers      = 4,
      .maxPerStageDescriptorSampledImages       = 16,
      .maxPerStageDescriptorStorageImages       = 4,
      .maxPerStageDescriptorInputAttachments    = 4,
      .maxPerStageResources                     = 128,
      .maxDescriptorSetSamplers                 = 96,
      .maxDescriptorSetUniformBuffers           = 72,
      .maxDescriptorSetUniformBuffersDynamic    = 8,
      .maxDescriptorSetStorageBuffers           = 24,
      .maxDescriptorSetStorageBuffersDynamic    = 4,
      .maxDescriptorSetSampledImages            = 96,
      .maxDescriptorSetStorageImages            = 24,
      .maxDescriptorSetInputAttachments         = 4,
      .maxVertexInputAttributes                 = 16,
      .maxVertexInputBindings                   = 16,
      .maxVertexInputAttributeOffset            = 2047,
      .maxVertexInputBindingStride              = 2048,
      .maxVertexOutputComponents                = 64,
      .maxTessellationGenerationLevel           = 0,
      .maxTessellationPatchSize                 = 0,
      .maxTessellationControlPerVertexInputComponents = 0,
      .maxTessellationControlPerVertexOutputComponents = 0,
      .maxTessellationControlPerPatchOutputComponents = 0,
      .maxTessellationControlTotalOutputComponents = 0,
      .maxTessellationEvaluationInputComponents = 0,
      .maxTessellationEvaluationOutputComponents = 0,
      .maxGeometryShaderInvocations             = 0,
      .maxGeometryInputComponents               = 0,
      .maxGeometryOutputComponents              = 0,
      .maxGeometryOutputVertices                = 0,
      .maxGeometryTotalOutputComponents         = 0,
      .maxFragmentInputComponents               = 64,
      .maxFragmentOutputAttachments             = 4,
      .maxFragmentDualSrcAttachments            = 0,
      .maxFragmentCombinedOutputResources       = 4,
      .maxComputeSharedMemorySize               = 16384,
      .maxComputeWorkGroupCount                 = { 65535, 65535, 65535 },
      .maxComputeWorkGroupInvocations           = 128,
      .maxComputeWorkGroupSize                  = { 128, 128, 64 },
      .subPixelPrecisionBits                    = 4,
      .subTexelPrecisionBits                    = 4,
      .mipmapPrecisionBits                      = 4,
      .maxDrawIndexedIndexValue                 = 0x00ffffff,
      .maxDrawIndirectCount                     = 1,
      .maxSamplerLodBias                        = 2.0f,
      .maxSamplerAnisotropy                     = 1.0f,
      .maxViewports                             = 1,
      .maxViewportDimensions                    = { 4096, 4096 },
      .viewportBoundsRange                      = { -8192, 8191 },
      .viewportSubPixelBits                     = 0,
      .minMemoryMapAlignment                    = 64,
      .minTexelBufferOffsetAlignment            = 256,
      .minUniformBufferOffsetAlignment          = 256,
      .minStorageBufferOffsetAlignment          = 256,
      .minTexelOffset                           = -8,
      .maxTexelOffset                           = 7,
      .minTexelGatherOffset                     = 0,
      .maxTexelGatherOffset                     = 0,
      .minInterpolationOffset                   = 0.0f,
      .maxInterpolationOffset                   = 0.0f,
      .subPixelInterpolationOffsetBits          = 0,
      .maxFramebufferWidth                      = 4096,
      .maxFramebufferHeight                     = 4096,
      .maxFramebufferLayers                     = 256,
      .framebufferColorSampleCounts             = supported_sample_counts,
      .framebufferDepthSampleCounts             = supported_sample_counts,
      .framebufferStencilSampleCounts           = supported_sample_counts,
      .framebufferNoAttachmentsSampleCounts     = supported_sample_counts,
      .maxColorAttachments                      = 4,
      .sampledImageColorSampleCounts            = supported_sample_counts,
      .sampledImageIntegerSampleCounts          = VK_SAMPLE_COUNT_1_BIT,
      .sampledImageDepthSampleCounts            = supported_sample_counts,
      .sampledImageStencilSampleCounts          = supported_sample_counts,
      .storageImageSampleCounts                 = VK_SAMPLE_COUNT_1_BIT,
      .maxSampleMaskWords                       = 1,
      .timestampComputeAndGraphics              = false,
      .timestampPeriod                          = 0.0f,
      .maxClipDistances                         = 8,
      .maxCullDistances                         = 8,
      .maxCombinedClipAndCullDistances          = 8,
      .discreteQueuePriorities                  = 2,
      .pointSizeRange                           = { 1.0f, 1.0f },
      .lineWidthRange                           = { 1.0f, 1.0f },
      .pointSizeGranularity                     = 0.0f,
      .lineWidthGranularity                     = 0.0f,
      .strictLines                              = 0,
      .standardSampleLocations                  = false,
      .optimalBufferCopyOffsetAlignment         = 1,
      .optimalBufferCopyRowPitchAlignment       = 1,
      .nonCoherentAtomSize                      = 256,
   };

   DXGI_ADAPTER_DESC1 desc;
   HRESULT hr = pdevice->adapter->GetDesc1(&desc);

   VkPhysicalDeviceType devtype = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
   if (desc.Flags == DXGI_ADAPTER_FLAG_SOFTWARE)
      devtype = VK_PHYSICAL_DEVICE_TYPE_CPU;
   else if (false) { // TODO: detect discreete GPUs
      /* This is a tad tricky to get right, because we need to have the
       * actual ID3D12Device before we can query the
       * D3D12_FEATURE_DATA_ARCHITECTURE structure... So for now, let's
       * just pretend everything is integrated, because... well, that's
       * what I have at hand right now ;)
       */
      devtype = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
   }

   *pProperties = VkPhysicalDeviceProperties {
      .apiVersion = DZN_API_VERSION,
      .driverVersion = vk_get_driver_version(),

      .vendorID = desc.VendorId,
      .deviceID = desc.DeviceId,
      .deviceType = devtype,

      .limits = limits,
      .sparseProperties = { 0 },
   };

   snprintf(pProperties->deviceName, sizeof(pProperties->deviceName),
            "Microsoft Direct3D12 (%S)", desc.Description);

   memcpy(pProperties->pipelineCacheUUID,
          pdevice->pipeline_cache_uuid, VK_UUID_SIZE);
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                 VkPhysicalDeviceProperties2 *pProperties)
{
   VK_FROM_HANDLE(dzn_physical_device, pdevice, physicalDevice);

   dzn_GetPhysicalDeviceProperties(physicalDevice, &pProperties->properties);

   vk_foreach_struct(ext, pProperties->pNext) {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES: {
         VkPhysicalDeviceIDProperties *id_props =
            (VkPhysicalDeviceIDProperties *)ext;
         memcpy(id_props->deviceUUID, pdevice->device_uuid, VK_UUID_SIZE);
         memcpy(id_props->driverUUID, pdevice->driver_uuid, VK_UUID_SIZE);
         /* The LUID is for Windows. */
         id_props->deviceLUIDValid = false;
         break;
      }
      default:
         dzn_debug_ignored_stype(ext->sType);
         break;
      }
   }
}

/* We support exactly one queue family. */
static const VkQueueFamilyProperties
dzn_queue_family_properties = {
   .queueFlags = VK_QUEUE_GRAPHICS_BIT |
                 VK_QUEUE_COMPUTE_BIT |
                 VK_QUEUE_TRANSFER_BIT,
   .queueCount = 1,
   .timestampValidBits = 0,
   .minImageTransferGranularity = { 0, 0, 0 },
};

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice,
                                           uint32_t *pCount,
                                           VkQueueFamilyProperties *pQueueFamilyProperties)
{
   VK_FROM_HANDLE(dzn_physical_device, pdevice, physicalDevice);
   VK_OUTARRAY_MAKE_TYPED(VkQueueFamilyProperties, out, pQueueFamilyProperties, pCount);
   vk_outarray_append_typed(VkQueueFamilyProperties, &out, p) {
      *p = dzn_queue_family_properties;
   }
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice,
                                            uint32_t *pQueueFamilyPropertyCount,
                                            VkQueueFamilyProperties2 *pQueueFamilyProperties)
{
   VK_FROM_HANDLE(dzn_physical_device, pdevice, physicalDevice);
   VK_OUTARRAY_MAKE_TYPED(VkQueueFamilyProperties2, out,
                          pQueueFamilyProperties, pQueueFamilyPropertyCount);

#if 0
   /* TODO: enumerate queue families */
   for (uint32_t i = 0; i < pdevice->queue.family_count; i++) {
      dzn_queue_family *queue_family = &pdevice->queue.families[i];
      vk_outarray_append(VkQueueFamilyProperties, &out, p) {
         p->queueFamilyProperties = dzn_queue_family_properties_template;
         p->queueFamilyProperties.queueFlags = queue_family->queueFlags;
         p->queueFamilyProperties.queueCount = queue_family->queueCount;

         vk_foreach_struct(ext, pMemoryProperties->pNext) {
            dzn_debug_ignored_stype(ext->sType);
         }
      }
   }
#endif
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                      VkPhysicalDeviceMemoryProperties *pMemoryProperties)
{
   VK_FROM_HANDLE(dzn_physical_device, device, physicalDevice);
   *pMemoryProperties = device->memory;
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                       VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
   dzn_GetPhysicalDeviceMemoryProperties(physicalDevice,
                                         &pMemoryProperties->memoryProperties);

   vk_foreach_struct(ext, pMemoryProperties->pNext) {
      dzn_debug_ignored_stype(ext->sType);
   }
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_EnumerateInstanceLayerProperties(uint32_t *pPropertyCount,
                                     VkLayerProperties *pProperties)
{
   if (pProperties == NULL) {
      *pPropertyCount = 0;
      return VK_SUCCESS;
   }

   return vk_error(NULL, VK_ERROR_LAYER_NOT_PRESENT);
}

dzn_queue::dzn_queue(dzn_device *dev,
                     const VkDeviceQueueCreateInfo *pCreateInfo,
                     const VkAllocationCallbacks *alloc)
{
   VkResult result = vk_queue_init(&vk, &dev->vk, pCreateInfo, 0);
   if (result != VK_SUCCESS)
      throw result;

   device = dev;

   D3D12_COMMAND_QUEUE_DESC queue_desc = {
      .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
      .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
      .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
      .NodeMask = 0,
   };

   if (FAILED(device->dev->CreateCommandQueue(&queue_desc,
                                              IID_PPV_ARGS(&cmdqueue))))
      throw vk_error(device, VK_ERROR_INITIALIZATION_FAILED);

   if (FAILED(device->dev->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                       IID_PPV_ARGS(&fence))))
      throw vk_error(device, VK_ERROR_INITIALIZATION_FAILED);
}

dzn_queue::~dzn_queue()
{
   vk_queue_finish(&vk);
}

const VkAllocationCallbacks *
dzn_queue::get_vk_allocator()
{
   return &device->vk.alloc;
}

static VkResult
check_physical_device_features(VkPhysicalDevice physicalDevice,
                               const VkPhysicalDeviceFeatures *features)
{
   VkPhysicalDeviceFeatures supported_features;
   dzn_GetPhysicalDeviceFeatures(physicalDevice, &supported_features);
   VkBool32 *supported_feature = (VkBool32 *)&supported_features;
   VkBool32 *enabled_feature = (VkBool32 *)features;
   unsigned num_features = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
   for (uint32_t i = 0; i < num_features; i++) {
      if (enabled_feature[i] && !supported_feature[i])
         return VK_ERROR_FEATURE_NOT_PRESENT;
   }

   return VK_SUCCESS;
}

dzn_device::dzn_device(VkPhysicalDevice pdev,
                       const VkDeviceCreateInfo *pCreateInfo,
                       const VkAllocationCallbacks *pAllocator)
{
   physical_device = dzn_physical_device_from_handle(pdev);
   instance = physical_device->instance;

   vk_device_dispatch_table dispatch_table;
   vk_device_dispatch_table_from_entrypoints(&dispatch_table,
      &dzn_device_entrypoints, true);
   vk_device_dispatch_table_from_entrypoints(&dispatch_table,
      &wsi_device_entrypoints, false);

   VkResult result =
      vk_device_init(&vk, &physical_device->vk,
                     &dispatch_table, pCreateInfo, pAllocator);
   if (result != VK_SUCCESS)
      throw result;

   d3d12_enable_debug_layer();

   dev = d3d12_create_device(physical_device->adapter, false);
   if (!dev) {
      vk_device_finish(&vk);
      throw vk_error(instance, VK_ERROR_UNKNOWN);
   }

   ID3D12InfoQueue *info_queue;
   if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&info_queue)))) {
      D3D12_MESSAGE_SEVERITY severities[] = {
         D3D12_MESSAGE_SEVERITY_INFO,
         D3D12_MESSAGE_SEVERITY_WARNING,
      };

      D3D12_MESSAGE_ID msg_ids[] = {
         D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
      };

      D3D12_INFO_QUEUE_FILTER NewFilter = {};
      NewFilter.DenyList.NumSeverities = ARRAY_SIZE(severities);
      NewFilter.DenyList.pSeverityList = severities;
      NewFilter.DenyList.NumIDs = ARRAY_SIZE(msg_ids);
      NewFilter.DenyList.pIDList = msg_ids;

      info_queue->PushStorageFilter(&NewFilter);
   }

   assert(pCreateInfo->queueCreateInfoCount == 1);
   dzn_queue *q;
   result = dzn_queue_factory::create(this,
                                      &pCreateInfo->pQueueCreateInfos[0],
                                      NULL, &q);
   if (result != VK_SUCCESS) {
      vk_device_finish(&vk);
      throw result;
   }

   queue = dzn_object_unique_ptr<dzn_queue>(q);

   struct d3d12_descriptor_pool *pool =
      d3d12_descriptor_pool_new(dev.Get(),
                                D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                64);
   if (!pool) {
      vk_device_finish(&vk);
      throw vk_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   rtv_pool = std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter>(pool);

   pool = d3d12_descriptor_pool_new(dev.Get(),
                                    D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                                    64);
   if (!pool) {
      vk_device_finish(&vk);
      throw vk_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   dsv_pool = std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter>(pool);

   dev->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE1,
                            &arch, sizeof(arch));
}

dzn_device::~dzn_device()
{
   /* We need to explicitly reset the queue before calling vk_device_finish(),
    * otherwise the queue list maintained by the vk_device object is not empty
    * which makes vk_device_finish() unhappy.
    */
   queue.reset(NULL);
   vk_device_finish(&vk);
}

void
dzn_device::alloc_rtv_handle(struct d3d12_descriptor_handle *handle)
{
   std::lock_guard<std::mutex> lock(pools_lock);
   d3d12_descriptor_pool_alloc_handle(rtv_pool.get(), handle);
}

void
dzn_device::alloc_dsv_handle(struct d3d12_descriptor_handle *handle)
{
   std::lock_guard<std::mutex> lock(pools_lock);
   d3d12_descriptor_pool_alloc_handle(dsv_pool.get(), handle);
}

void
dzn_device::free_handle(struct d3d12_descriptor_handle *handle)
{
   std::lock_guard<std::mutex> lock(pools_lock);
   d3d12_descriptor_handle_free(handle);
}

dzn_device *
dzn_device_factory::allocate(VkPhysicalDevice physical_device,
                             const VkDeviceCreateInfo *pCreateInfo,
                             const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(dzn_physical_device, pdev, physical_device);

   return (dzn_device *)
      vk_zalloc2(&pdev->instance->vk.alloc, pAllocator,
                 sizeof(dzn_device), 8, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
}

void
dzn_device_factory::deallocate(dzn_device *device,
                               const VkAllocationCallbacks *pAllocator)
{
   vk_free2(&device->instance->vk.alloc, pAllocator, device);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateDevice(VkPhysicalDevice physicalDevice,
                 const VkDeviceCreateInfo *pCreateInfo,
                 const VkAllocationCallbacks *pAllocator,
                 VkDevice *pDevice)
{
   VK_FROM_HANDLE(dzn_physical_device, physical_device, physicalDevice);
   dzn_instance *instance = physical_device->instance;
   VkResult result;

   d3d12_enable_gpu_validation();
   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);

   /* Check enabled features */
   if (pCreateInfo->pEnabledFeatures) {
      result = check_physical_device_features(physicalDevice,
                                              pCreateInfo->pEnabledFeatures);
      if (result != VK_SUCCESS)
         return vk_error(instance, result);
   }

   /* Check requested queues and fail if we are requested to create any
    * queues with flags we don't support.
    */
   assert(pCreateInfo->queueCreateInfoCount > 0);
   for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; i++) {
      if (pCreateInfo->pQueueCreateInfos[i].flags != 0)
         return vk_error(instance, VK_ERROR_INITIALIZATION_FAILED);
   }

   return dzn_device_factory::create(physicalDevice,
                                     pCreateInfo, pAllocator, pDevice);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyDevice(VkDevice dev,
                  const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(dzn_device, device, dev);

   dzn_DeviceWaitIdle(dev);

   dzn_device_factory::destroy(dev, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetDeviceQueue(VkDevice _device,
                   uint32_t queueFamilyIndex,
                   uint32_t queueIndex,
                   VkQueue *pQueue)
{
   VK_FROM_HANDLE(dzn_device, device, _device);

   assert(queueIndex == 0);
   assert(queueFamilyIndex == 0);

   *pQueue = dzn_queue_to_handle(device->queue.get());
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_DeviceWaitIdle(VkDevice _device)
{
#if 0
   VK_FROM_HANDLE(dzn_device, device, _device);
   return dzn_QueueWaitIdle(dzn_queue_to_handle(&device->queue));
#else
   return VK_SUCCESS;
#endif
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_QueueWaitIdle(VkQueue _queue)
{
   VK_FROM_HANDLE(dzn_queue, queue, _queue);

   if (FAILED(queue->fence->SetEventOnCompletion(queue->fence_point, NULL)))
      return vk_error(queue, VK_ERROR_OUT_OF_HOST_MEMORY);

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_QueueSubmit(VkQueue _queue,
                uint32_t submitCount,
                const VkSubmitInfo *pSubmits,
                VkFence _fence)
{
   VK_FROM_HANDLE(dzn_queue, queue, _queue);
   VK_FROM_HANDLE(dzn_fence, fence, _fence);
   struct dzn_device *device = queue->device;

   /* TODO: execute an array of these instead of one at the time */
   for (uint32_t i = 0; i < submitCount; i++) {
      for (uint32_t j = 0; j < pSubmits[i].commandBufferCount; j++) {
         VK_FROM_HANDLE(dzn_cmd_buffer, cmd_buffer,
                         pSubmits[i].pCommandBuffers[j]);
         assert(cmd_buffer->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY);

         util_dynarray_foreach(&cmd_buffer->batches, dzn_batch *, batch) {
            ID3D12CommandList *cmdlists[] = { (*batch)->cmdlist };

            util_dynarray_foreach(&(*batch)->events.wait, dzn_event *, event)
               queue->cmdqueue->Wait((*event)->fence, 1);

            queue->cmdqueue->ExecuteCommandLists(1, cmdlists);

            util_dynarray_foreach(&(*batch)->events.wait, dzn_cmd_event_signal, signal)
               queue->cmdqueue->Signal(signal->event->fence, signal->value ? 1 : 0);
         }
      }
   }

   if (fence)
      queue->cmdqueue->Signal(fence->fence, 1);

   queue->cmdqueue->Signal(queue->fence.Get(), ++queue->fence_point);

   if (queue->device->physical_device->instance->debug_flags & DZN_DEBUG_SYNC)
      dzn_QueueWaitIdle(_queue);

   return VK_SUCCESS;
}


VKAPI_ATTR VkResult VKAPI_CALL
dzn_AllocateMemory(VkDevice _device,
                   const VkMemoryAllocateInfo *pAllocateInfo,
                   const VkAllocationCallbacks *pAllocator,
                   VkDeviceMemory *pMem)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   struct dzn_physical_device *pdevice = device->physical_device;
   struct dzn_device_memory *mem;
   VkResult result = VK_SUCCESS;

   assert(pAllocateInfo->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);

   /* The Vulkan 1.0.33 spec says "allocationSize must be greater than 0". */
   assert(pAllocateInfo->allocationSize > 0);

   mem = (struct dzn_device_memory *)
      vk_object_alloc(&device->vk, pAllocator, sizeof(*mem),
                      VK_OBJECT_TYPE_DEVICE_MEMORY);
   if (mem == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   mem->size = pAllocateInfo->allocationSize;
   mem->map = NULL;
   mem->map_size = 0;

#if 0
   const VkExportMemoryAllocateInfo *export_info = NULL;
   VkMemoryAllocateFlags vk_flags = 0;
#endif

   vk_foreach_struct_const(ext, pAllocateInfo->pNext) {
      dzn_debug_ignored_stype(ext->sType);
   }

   const VkMemoryType *mem_type =
      &device->physical_device->memory.memoryTypes[pAllocateInfo->memoryTypeIndex];

   D3D12_HEAP_DESC heap_desc = {};
   // TODO: fix all of these:
   heap_desc.SizeInBytes = pAllocateInfo->allocationSize;
   heap_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
   heap_desc.Flags = D3D12_HEAP_FLAG_NONE;

   /* TODO: Unsure about this logic??? */
   mem->initial_state = D3D12_RESOURCE_STATE_COMMON;
   heap_desc.Properties.Type = D3D12_HEAP_TYPE_CUSTOM;
   heap_desc.Properties.MemoryPoolPreference =
      ((mem_type->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) && !device->arch.UMA) ?
      D3D12_MEMORY_POOL_L1 : D3D12_MEMORY_POOL_L0;
   if (mem_type->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
      heap_desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
   } else if (mem_type->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      heap_desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
   } else {
      heap_desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
   }

   if (FAILED(device->dev->CreateHeap(&heap_desc, IID_PPV_ARGS(&mem->heap)))) {
      result = vk_error(device, VK_ERROR_UNKNOWN);
      goto fail;
   }

   if (mem_type->propertyFlags &
       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      D3D12_RESOURCE_DESC res_desc = {};
      res_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      res_desc.Format = DXGI_FORMAT_UNKNOWN;
      res_desc.Alignment = heap_desc.Alignment;
      res_desc.Width = heap_desc.SizeInBytes;
      res_desc.Height = 1;
      res_desc.DepthOrArraySize = 1;
      res_desc.MipLevels = 1;
      res_desc.SampleDesc.Count = 1;
      res_desc.SampleDesc.Quality = 0;
      res_desc.Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
      res_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      HRESULT hr = device->dev->CreatePlacedResource(mem->heap, 0, &res_desc,
                                                     mem->initial_state,
                                                     NULL, IID_PPV_ARGS(&mem->map_res));
      if (FAILED(hr)) {
         result = vk_error(device, VK_ERROR_UNKNOWN);
         goto fail;
      }
   } else
      mem->map_res = NULL;

#if 0
   pthread_mutex_lock(&device->mutex);
   list_addtail(&mem->link, &device->memory_objects);
   pthread_mutex_unlock(&device->mutex);
#endif

   *pMem = dzn_device_memory_to_handle(mem);

   return VK_SUCCESS;

 fail:
   vk_object_free(&device->vk, pAllocator, mem);

   return result;
}

VKAPI_ATTR void VKAPI_CALL
dzn_FreeMemory(VkDevice _device,
               VkDeviceMemory _mem,
               const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_device_memory, mem, _mem);

   if (mem == NULL)
      return;

#if 0
   pthread_mutex_lock(&device->mutex);
   list_del(&mem->link);
   pthread_mutex_unlock(&device->mutex);
#endif

   if (mem->map)
      dzn_UnmapMemory(_device, _mem);

#if 0
   p_atomic_add(&device->physical->memory.heaps[mem->type->heapIndex].used,
                -mem->bo->size);
#endif

   mem->map_res->Release();
   mem->heap->Release();

   vk_object_free(&device->vk, pAllocator, mem);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_MapMemory(VkDevice _device,
              VkDeviceMemory _memory,
              VkDeviceSize offset,
              VkDeviceSize size,
              VkMemoryMapFlags flags,
              void **ppData)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_device_memory, mem, _memory);

   if (mem == NULL) {
      *ppData = NULL;
      return VK_SUCCESS;
   }

   if (size == VK_WHOLE_SIZE)
      size = mem->size - offset;

   /* From the Vulkan spec version 1.0.32 docs for MapMemory:
    *
    *  * If size is not equal to VK_WHOLE_SIZE, size must be greater than 0
    *    assert(size != 0);
    *  * If size is not equal to VK_WHOLE_SIZE, size must be less than or
    *    equal to the size of the memory minus offset
    */
   assert(size > 0);
   assert(offset + size <= mem->size);

   assert(mem->map_res);
   D3D12_RANGE range = {};
   range.Begin = offset;
   range.End = offset + size;
   void *map = NULL;
   if (FAILED(mem->map_res->Map(0, &range, &map)))
      return vk_error(device, VK_ERROR_MEMORY_MAP_FAILED);

   mem->map = map;
   mem->map_size = size;

   *ppData = ((uint8_t*) map) + offset;

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
dzn_UnmapMemory(VkDevice _device,
                VkDeviceMemory _memory)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_device_memory, mem, _memory);

   if (mem == NULL)
      return;

   assert(mem->map_res);
   mem->map_res->Unmap(0, NULL);

   mem->map = NULL;
   mem->map_size = 0;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_FlushMappedMemoryRanges(VkDevice _device,
                            uint32_t memoryRangeCount,
                            const VkMappedMemoryRange *pMemoryRanges)
{
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_InvalidateMappedMemoryRanges(VkDevice _device,
                                 uint32_t memoryRangeCount,
                                 const VkMappedMemoryRange *pMemoryRanges)
{
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateBuffer(VkDevice _device,
                 const VkBufferCreateInfo *pCreateInfo,
                 const VkAllocationCallbacks *pAllocator,
                 VkBuffer *pBuffer)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   dzn_buffer *buffer;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);

   buffer = (dzn_buffer *)
      vk_object_alloc(&device->vk, pAllocator, sizeof(*buffer),
                      VK_OBJECT_TYPE_BUFFER);
   if (buffer == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   buffer->create_flags = pCreateInfo->flags;
   buffer->size = pCreateInfo->size;
   buffer->usage = pCreateInfo->usage;

   if (buffer->usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
      buffer->size = ALIGN_POT(buffer->size, 256);

   buffer->desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
   buffer->desc.Format = DXGI_FORMAT_UNKNOWN;
   buffer->desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
   buffer->desc.Width = buffer->size;
   buffer->desc.Height = 1;
   buffer->desc.DepthOrArraySize = 1;
   buffer->desc.MipLevels = 1;
   buffer->desc.SampleDesc.Count = 1;
   buffer->desc.SampleDesc.Quality = 0;
   buffer->desc.Flags = D3D12_RESOURCE_FLAG_NONE;
   buffer->desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

   *pBuffer = dzn_buffer_to_handle(buffer);

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyBuffer(VkDevice _device,
                  VkBuffer _buffer,
                  const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_buffer, buffer, _buffer);

   if (!buffer)
      return;

   vk_object_free(&device->vk, pAllocator, buffer);
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetBufferMemoryRequirements2(VkDevice _device,
                                 const VkBufferMemoryRequirementsInfo2 *pInfo,
                                 VkMemoryRequirements2 *pMemoryRequirements)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_buffer, buffer, pInfo->buffer);

   /* uh, this is grossly over-estimating things */
   uint32_t alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
   VkDeviceSize size = buffer->size;

   if (buffer->usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
      alignment = MAX2(alignment, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
      size = ALIGN_POT(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
   }

   pMemoryRequirements->memoryRequirements.size = size;
   pMemoryRequirements->memoryRequirements.alignment = 0;
   pMemoryRequirements->memoryRequirements.memoryTypeBits =
      (1ull << device->physical_device->memory.memoryTypeCount) - 1;

   vk_foreach_struct(ext, pMemoryRequirements->pNext) {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS: {
         VkMemoryDedicatedRequirements *requirements =
            (VkMemoryDedicatedRequirements *)ext;
         /* TODO: figure out dedicated allocations */
         requirements->prefersDedicatedAllocation = false;
         requirements->requiresDedicatedAllocation = false;
         break;
      }

      default:
         dzn_debug_ignored_stype(ext->sType);
         break;
      }
   }

#if 0
   D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo(
      UINT                      visibleMask,
      UINT                      numResourceDescs,
      const D3D12_RESOURCE_DESC *pResourceDescs);
#endif
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_BindBufferMemory2(VkDevice _device,
                      uint32_t bindInfoCount,
                      const VkBindBufferMemoryInfo *pBindInfos)
{
   VK_FROM_HANDLE(dzn_device, device, _device);

   for (uint32_t i = 0; i < bindInfoCount; i++) {
      assert(pBindInfos[i].sType == VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO);

      VK_FROM_HANDLE(dzn_device_memory, mem, pBindInfos[i].memory);
      VK_FROM_HANDLE(dzn_buffer, buffer, pBindInfos[i].buffer);

      HRESULT hr = device->dev->CreatePlacedResource(mem->heap,
                                                     pBindInfos[i].memoryOffset,
                                                     &buffer->desc,
                                                     mem->initial_state,
                                                     NULL, IID_PPV_ARGS(&buffer->res));
      /* TODO: gracefully handle errors here */
      assert(hr == S_OK);
   }

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateFramebuffer(VkDevice _device,
                      const VkFramebufferCreateInfo *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkFramebuffer *pFramebuffer)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   struct dzn_framebuffer *framebuffer;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);

   size_t size = sizeof(*framebuffer) + sizeof(struct dzn_image_view *) *
                 pCreateInfo->attachmentCount;

   framebuffer = (struct dzn_framebuffer *)
      vk_object_alloc(&device->vk, pAllocator, size,
                      VK_OBJECT_TYPE_FRAMEBUFFER);
   if (framebuffer == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   framebuffer->width = pCreateInfo->width;
   framebuffer->height = pCreateInfo->height;
   framebuffer->layers = pCreateInfo->layers;

   for (uint32_t i = 0; i < pCreateInfo->attachmentCount; i++) {
      VK_FROM_HANDLE(dzn_image_view, iview, pCreateInfo->pAttachments[i]);
      framebuffer->attachments[i] = iview;
   }
   framebuffer->attachment_count = pCreateInfo->attachmentCount;

   *pFramebuffer = dzn_framebuffer_to_handle(framebuffer);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyFramebuffer(VkDevice _device,
                       VkFramebuffer _fb,
                       const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_framebuffer, fb, _fb);

   if (!fb)
      return;

   vk_object_free(&device->vk, pAllocator, fb);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateEvent(VkDevice _device,
                const VkEventCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkEvent *pEvent)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   dzn_event *event;

   event = (struct dzn_event *)
      vk_object_alloc(&device->vk, pAllocator, sizeof(*event),
                      VK_OBJECT_TYPE_EVENT);

   if (FAILED(device->dev->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                       IID_PPV_ARGS(&event->fence))))
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   *pEvent = dzn_event_to_handle(event);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyEvent(VkDevice _device,
                 VkEvent _event,
                 const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_event, event, _event);

   if (!event)
      return;

   event->fence->Release();
   vk_object_free(&device->vk, pAllocator, event);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_ResetEvent(VkDevice _device,
               VkEvent _event)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_event, event, _event);

   event->fence->Signal(0);
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetEventStatus(VkDevice _device,
                   VkEvent _event)
{
   VK_FROM_HANDLE(dzn_device, device, _device);
   VK_FROM_HANDLE(dzn_event, event, _event);

   return event->fence->GetCompletedValue() ?
          VK_EVENT_SET : VK_EVENT_RESET;
   return VK_SUCCESS;
}
