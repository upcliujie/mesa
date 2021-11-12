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

void
dzn_physical_device::get_device_extensions()
{
   vk.supported_extensions = vk_device_extension_table {
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
   { "internal", DZN_DEBUG_INTERNAL },
   { "signature", DZN_DEBUG_SIG },
   { NULL, 0 }
};

dzn_instance::dzn_instance(const VkInstanceCreateInfo *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator) :
                          physical_devices(physical_devices_allocator(pAllocator,
                                                                      VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE))
{
   vk_instance_dispatch_table dispatch_table;
   vk_instance_dispatch_table_from_entrypoints(&dispatch_table,
                                               &dzn_instance_entrypoints,
                                               true);

   VkResult result =
      vk_instance_init(&vk, &instance_extensions,
                       &dispatch_table, pCreateInfo,
                       pAllocator ? pAllocator : vk_default_allocator());
   if (result != VK_SUCCESS)
      throw result;

   physical_devices_enumerated = false;
   debug_flags =
      parse_debug_string(getenv("DZN_DEBUG"), dzn_debug_options);

   dxc.validator = dxil_get_validator();
   dxc.library = dxc_get_library();
   dxc.compiler = dxc_get_compiler();
   d3d12.serialize_root_sig = d3d12_get_serialize_root_sig();
}

dzn_instance::~dzn_instance()
{
   vk_instance_finish(&vk);
}

dzn_instance *
dzn_instance_factory::allocate(const VkInstanceCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator)
{
   return (dzn_instance *)
      vk_zalloc2(vk_default_allocator(), pAllocator,
                 sizeof(dzn_instance), 8,
                 VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                   const VkAllocationCallbacks *pAllocator,
                   VkInstance *pInstance)
{
   return dzn_instance_factory::create(pCreateInfo, pAllocator, pInstance);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyInstance(VkInstance instance,
                    const VkAllocationCallbacks *pAllocator)
{
   return dzn_instance_factory::destroy(instance, pAllocator);
}

dzn_physical_device::dzn_physical_device(dzn_instance *inst,
                                         ComPtr<IDXGIAdapter1> &adap,
                                         const VkAllocationCallbacks *alloc) :
                                        instance(inst), adapter(adap)
{
   vk_physical_device_dispatch_table dispatch_table;
   vk_physical_device_dispatch_table_from_entrypoints(&dispatch_table,
                                                      &dzn_physical_device_entrypoints,
                                                      true);
   vk_physical_device_dispatch_table_from_entrypoints(&dispatch_table,
                                                      &wsi_physical_device_entrypoints,
                                                      false);

   VkResult result =
      vk_physical_device_init(&vk, &instance->vk,
                              NULL, /* We set up extensions later */
                              &dispatch_table);
   if (result != VK_SUCCESS)
      throw vk_error(instance, result);

   vk_warn_non_conformant_implementation("dzn");

   /* TODO: correct UUIDs */
   memset(pipeline_cache_uuid, 0, VK_UUID_SIZE);
   memset(driver_uuid, 0, VK_UUID_SIZE);
   memset(device_uuid, 0, VK_UUID_SIZE);

   VkPhysicalDeviceMemoryProperties *mem = &memory;
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

   result = dzn_wsi_init(this);
   if (result != VK_SUCCESS) {
      vk_physical_device_finish(&vk);
      throw vk_error(instance, result);
   }

   get_device_extensions();
}

dzn_physical_device::~dzn_physical_device()
{
   dzn_wsi_finish(this);
   vk_physical_device_finish(&vk);
}

const VkAllocationCallbacks *
dzn_physical_device::get_vk_allocator()
{
   return &instance->vk.alloc;
}

const D3D12_FEATURE_DATA_ARCHITECTURE1 &
dzn_physical_device::get_arch_caps() const
{
   assert(dev);

   return architecture;
}

void
dzn_physical_device::cache_caps(std::lock_guard<std::mutex>&)
{
   D3D_FEATURE_LEVEL checklist[] = {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_12_0,
      D3D_FEATURE_LEVEL_12_1,
      D3D_FEATURE_LEVEL_12_2,
   };

   D3D12_FEATURE_DATA_FEATURE_LEVELS levels = {
      .NumFeatureLevels = ARRAY_SIZE(checklist),
      .pFeatureLevelsRequested = checklist,
   };

   dev->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof(levels));
   feature_level = levels.MaxSupportedFeatureLevel;

   dev->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE1, &architecture, sizeof(architecture));
}

ID3D12Device *
dzn_physical_device::get_d3d12_dev()
{
   std::lock_guard<std::mutex> lock(dev_lock);

   if (!dev.Get()) {
      dev = d3d12_create_device(adapter.Get(), false);
      cache_caps(lock);
   }

   return dev.Get();
}

dzn_physical_device *
dzn_physical_device_factory::allocate(dzn_instance *instance,
                                      ComPtr<IDXGIAdapter1> &adapter,
                                      const VkAllocationCallbacks *alloc)
{
   return (dzn_physical_device *)
      vk_zalloc(&instance->vk.alloc, sizeof(dzn_physical_device), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
}

void
dzn_physical_device_factory::deallocate(dzn_physical_device *device,
                                        const VkAllocationCallbacks *alloc)
{
   vk_free(&device->instance->vk.alloc, device);
}

VkResult
dzn_instance::enumerate_physical_devices(uint32_t *pPhysicalDeviceCount,
                                         VkPhysicalDevice *pPhysicalDevices)
{
   if (!physical_devices_enumerated) {
      physical_devices_enumerated = true;

      ComPtr<IDXGIFactory4> factory = dxgi_get_factory(false);
      ComPtr<IDXGIAdapter1> adapter(NULL);
      for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); ++i) {
         if (debug_flags & DZN_DEBUG_WARP) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
               continue;
         }

         dzn_physical_device *pdev;
         VkResult result =
            dzn_physical_device_factory::create(this, adapter, NULL, &pdev);
         if (result != VK_SUCCESS)
            return result;

         physical_devices.push_back(dzn_object_unique_ptr<dzn_physical_device>(pdev));
      }
   }

   VK_OUTARRAY_MAKE_TYPED(VkPhysicalDevice, out, pPhysicalDevices,
                          pPhysicalDeviceCount);

   for (auto &pdevice : physical_devices) {
      vk_outarray_append_typed(VkPhysicalDevice, &out, i)
      {
         *i = dzn_physical_device_to_handle(pdevice.get());
      }
   }

   return vk_outarray_status(&out);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_EnumeratePhysicalDevices(VkInstance _instance,
                             uint32_t *pPhysicalDeviceCount,
                             VkPhysicalDevice *pPhysicalDevices)
{
   VK_FROM_HANDLE(dzn_instance, instance, _instance);

   return instance->enumerate_physical_devices(pPhysicalDeviceCount,
                                               pPhysicalDevices);
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

   dev = physical_device->get_d3d12_dev();
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
      d3d12_descriptor_pool_new(dev,
                                D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                64);
   if (!pool) {
      vk_device_finish(&vk);
      throw vk_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   rtv_pool = std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter>(pool);

   pool = d3d12_descriptor_pool_new(dev,
                                    D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                                    64);
   if (!pool) {
      vk_device_finish(&vk);
      throw vk_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   dsv_pool = std::unique_ptr<struct d3d12_descriptor_pool, d3d12_descriptor_pool_deleter>(pool);

   for (uint32_t i = 0; i < ARRAY_SIZE(indirect_draws); i++) {
      enum dzn_indirect_draw_type type = (enum dzn_indirect_draw_type)i;

      indirect_draws[i] =
         dzn_private_object_create<dzn_meta_indirect_draw>(&vk.alloc, this, type);
   }

   for (uint32_t i = 0; i < ARRAY_SIZE(triangle_fan); i++) {
      auto type = (dzn_meta_triangle_fan_rewrite_index::index_type)i;

      triangle_fan[i] =
         dzn_private_object_create<dzn_meta_triangle_fan_rewrite_index>(&vk.alloc, this, type);
   }
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

ComPtr<ID3D12RootSignature>
dzn_device::create_root_sig(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc)
{
   ComPtr<ID3D12RootSignature> root_sig;
   ComPtr<ID3DBlob> sig, error;

   if (FAILED(instance->d3d12.serialize_root_sig(&desc,
                                                 &sig, &error))) {
      if (instance->debug_flags & DZN_DEBUG_SIG) {
         const char* error_msg = (const char*)error->GetBufferPointer();
         fprintf(stderr,
                 "== SERIALIZE ROOT SIG ERROR =============================================\n"
                 "%s\n"
                 "== END ==========================================================\n",
                 error_msg);
      }

      return root_sig;
   }

   dev->CreateRootSignature(0,
                            sig->GetBufferPointer(),
                            sig->GetBufferSize(),
                            IID_PPV_ARGS(&root_sig));

   return root_sig;
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

         for (auto &batch : cmd_buffer->batches) {
            ID3D12CommandList *cmdlists[] = { batch->cmdlist.Get() };

            for (auto &event : batch->wait)
               queue->cmdqueue->Wait(event->fence.Get(), 1);

            queue->cmdqueue->ExecuteCommandLists(1, cmdlists);

            for (auto &signal : batch->signal)
               queue->cmdqueue->Signal(signal.event->fence.Get(), signal.value ? 1 : 0);
         }
      }
   }

   if (fence)
      queue->cmdqueue->Signal(fence->fence.Get(), 1);

   queue->cmdqueue->Signal(queue->fence.Get(), ++queue->fence_point);

   if (queue->device->physical_device->instance->debug_flags & DZN_DEBUG_SYNC)
      dzn_QueueWaitIdle(_queue);

   return VK_SUCCESS;
}

dzn_device_memory::dzn_device_memory(dzn_device *device,
                                     const VkMemoryAllocateInfo *pAllocateInfo,
                                     const VkAllocationCallbacks *pAllocator)
{
   struct dzn_physical_device *pdevice = device->physical_device;
   auto& arch = pdevice->get_arch_caps();

   /* The Vulkan 1.0.33 spec says "allocationSize must be greater than 0". */
   assert(pAllocateInfo->allocationSize > 0);

   size = pAllocateInfo->allocationSize;

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
   heap_desc.Alignment =
      D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
   heap_desc.Flags = D3D12_HEAP_FLAG_NONE;

   /* TODO: Unsure about this logic??? */
   initial_state = D3D12_RESOURCE_STATE_COMMON;
   heap_desc.Properties.Type = D3D12_HEAP_TYPE_CUSTOM;
   heap_desc.Properties.MemoryPoolPreference =
      ((mem_type->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) && !arch.UMA) ?
      D3D12_MEMORY_POOL_L1 : D3D12_MEMORY_POOL_L0;
   if (mem_type->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
      heap_desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
   } else if (mem_type->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      heap_desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
   } else {
      heap_desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
   }

   if (FAILED(device->dev->CreateHeap(&heap_desc, IID_PPV_ARGS(&heap))))
      throw vk_error(device, VK_ERROR_UNKNOWN);

   if (mem_type->propertyFlags &
       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      D3D12_RESOURCE_DESC res_desc = {};
      res_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      res_desc.Format = DXGI_FORMAT_UNKNOWN;
      res_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
      res_desc.Width = heap_desc.SizeInBytes;
      res_desc.Height = 1;
      res_desc.DepthOrArraySize = 1;
      res_desc.MipLevels = 1;
      res_desc.SampleDesc.Count = 1;
      res_desc.SampleDesc.Quality = 0;
      res_desc.Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
      res_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      HRESULT hr = device->dev->CreatePlacedResource(heap.Get(), 0, &res_desc,
                                                     initial_state,
                                                     NULL, IID_PPV_ARGS(&map_res));
      if (FAILED(hr))
         throw vk_error(device, VK_ERROR_UNKNOWN);
   }

#if 0
   pthread_mutex_lock(&device->mutex);
   list_addtail(&mem->link, &device->memory_objects);
   pthread_mutex_unlock(&device->mutex);
#endif

   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_DEVICE_MEMORY);
}

dzn_device_memory::~dzn_device_memory()
{
#if 0
   pthread_mutex_lock(&device->mutex);
   list_del(&link);
   pthread_mutex_unlock(&device->mutex);
#endif

   if (map) {
      map_res->Unmap(0, NULL);
      map = NULL;
   }

#if 0
   p_atomic_add(&device->physical->memory.heaps[mem->type->heapIndex].used,
                -mem->bo->size);
#endif

   vk_object_base_finish(&base);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_AllocateMemory(VkDevice device,
                   const VkMemoryAllocateInfo *pAllocateInfo,
                   const VkAllocationCallbacks *pAllocator,
                   VkDeviceMemory *pMem)
{
   return dzn_device_memory_factory::create(device, pAllocateInfo,
                                            pAllocator, pMem);
}

VKAPI_ATTR void VKAPI_CALL
dzn_FreeMemory(VkDevice device,
               VkDeviceMemory mem,
               const VkAllocationCallbacks *pAllocator)
{
   dzn_device_memory_factory::destroy(device, mem, pAllocator);
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

dzn_buffer::dzn_buffer(dzn_device *device,
                       const VkBufferCreateInfo *pCreateInfo,
                       const VkAllocationCallbacks *pAllocator)
{
   create_flags = pCreateInfo->flags;
   size = pCreateInfo->size;
   usage = pCreateInfo->usage;

   if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
      size = ALIGN_POT(size, 256);

   desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
   desc.Format = DXGI_FORMAT_UNKNOWN;
   desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
   desc.Width = size;
   desc.Height = 1;
   desc.DepthOrArraySize = 1;
   desc.MipLevels = 1;
   desc.SampleDesc.Count = 1;
   desc.SampleDesc.Quality = 0;
   desc.Flags = D3D12_RESOURCE_FLAG_NONE;
   desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

   if (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
      desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_BUFFER);
}

dzn_buffer::~dzn_buffer()
{
   vk_object_base_finish(&base);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateBuffer(VkDevice device,
                 const VkBufferCreateInfo *pCreateInfo,
                 const VkAllocationCallbacks *pAllocator,
                 VkBuffer *pBuffer)
{
   return dzn_buffer_factory::create(device, pCreateInfo,
                                     pAllocator, pBuffer);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyBuffer(VkDevice device,
                  VkBuffer buffer,
                  const VkAllocationCallbacks *pAllocator)
{
   return dzn_buffer_factory::destroy(device, buffer, pAllocator);
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

      HRESULT hr = device->dev->CreatePlacedResource(mem->heap.Get(),
                                                     pBindInfos[i].memoryOffset,
                                                     &buffer->desc,
                                                     mem->initial_state,
                                                     NULL, IID_PPV_ARGS(&buffer->res));
      /* TODO: gracefully handle errors here */
      assert(hr == S_OK);
   }

   return VK_SUCCESS;
}

dzn_framebuffer::dzn_framebuffer(dzn_device *device,
                                 const VkFramebufferCreateInfo *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator)
{
   width = pCreateInfo->width;
   height = pCreateInfo->height;
   layers = pCreateInfo->layers;

   attachment_count = pCreateInfo->attachmentCount;
   for (uint32_t i = 0; i < attachment_count; i++) {
      VK_FROM_HANDLE(dzn_image_view, iview, pCreateInfo->pAttachments[i]);
      attachments[i] = iview;
   }

   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_FRAMEBUFFER);
}

dzn_framebuffer::~dzn_framebuffer()
{
   vk_object_base_finish(&base);
}

dzn_framebuffer *
dzn_framebuffer_factory::allocate(dzn_device *device,
                                  const VkFramebufferCreateInfo *pCreateInfo,
                                  const VkAllocationCallbacks *pAllocator)
{
   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);

   struct dzn_framebuffer *framebuffer;
   size_t size = sizeof(*framebuffer) +
                 (sizeof(struct dzn_image_view *) *
                  pCreateInfo->attachmentCount);

   framebuffer = (struct dzn_framebuffer *)
      vk_alloc2(&device->vk.alloc, pAllocator, size, alignof(dzn_framebuffer),
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   return framebuffer;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateFramebuffer(VkDevice device,
                      const VkFramebufferCreateInfo *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkFramebuffer *pFramebuffer)
{
   return dzn_framebuffer_factory::create(device, pCreateInfo,
                                          pAllocator, pFramebuffer);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyFramebuffer(VkDevice device,
                       VkFramebuffer fb,
                       const VkAllocationCallbacks *pAllocator)
{
   dzn_framebuffer_factory::destroy(device, fb, pAllocator);
}

dzn_event::dzn_event(dzn_device *device,
                     const VkEventCreateInfo *pCreateInfo,
                     const VkAllocationCallbacks *pAllocator)
{
   if (FAILED(device->dev->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                       IID_PPV_ARGS(&fence))))
      throw vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_object_base_init(&device->vk, &base, VK_OBJECT_TYPE_EVENT);
}

dzn_event::~dzn_event()
{
   vk_object_base_finish(&base);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_CreateEvent(VkDevice device,
                const VkEventCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkEvent *pEvent)
{
   return dzn_event_factory::create(device, pCreateInfo, pAllocator, pEvent);
}

VKAPI_ATTR void VKAPI_CALL
dzn_DestroyEvent(VkDevice device,
                 VkEvent event,
                 const VkAllocationCallbacks *pAllocator)
{
   return dzn_event_factory::destroy(device, event, pAllocator);
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
