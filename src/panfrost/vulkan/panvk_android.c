/*
 * Mesa 3-D graphics library
 *
 * Copyright © 2017, Google Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "panvk_private.h"

#include <hardware/gralloc.h>
#if ANDROID_API_LEVEL >= 26
#include <hardware/gralloc1.h>
#endif

#include <hardware/hardware.h>
#include <hardware/hwvulkan.h>
#include <vulkan/vk_android_native_buffer.h>
#include <vulkan/vk_icd.h>

#include "util/log.h"
#include "util/os_file.h"
#include "util/u_gralloc/u_gralloc.h"
#include "vulkan/util/vk_enum_defines.h"
#include "vulkan/util/vk_util.h"

static int panvk_hal_open(const struct hw_module_t *mod, const char *id,
                          struct hw_device_t **dev);
static int panvk_hal_close(struct hw_device_t *dev);

static_assert(HWVULKAN_DISPATCH_MAGIC == ICD_LOADER_MAGIC, "");

PUBLIC struct hwvulkan_module_t HAL_MODULE_INFO_SYM = {
   .common =
      {
         .tag = HARDWARE_MODULE_TAG,
         .module_api_version = HWVULKAN_MODULE_API_VERSION_0_1,
         .hal_api_version = HARDWARE_MAKE_API_VERSION(1, 0),
         .id = HWVULKAN_HARDWARE_MODULE_ID,
         .name = "ARM Vulkan HAL",
         .author = "Mesa3D",
         .methods =
            &(hw_module_methods_t){
               .open = panvk_hal_open,
            },
      },
};

static int
panvk_hal_open(const struct hw_module_t *mod, const char *id,
               struct hw_device_t **dev)
{
   assert(mod == &HAL_MODULE_INFO_SYM.common);
   assert(strcmp(id, HWVULKAN_DEVICE_0) == 0);

   hwvulkan_device_t *hal_dev = malloc(sizeof(*hal_dev));
   if (!hal_dev)
      return -1;

   *hal_dev = (hwvulkan_device_t){
      .common =
         {
            .tag = HARDWARE_DEVICE_TAG,
            .version = HWVULKAN_DEVICE_API_VERSION_0_1,
            .module = &HAL_MODULE_INFO_SYM.common,
            .close = panvk_hal_close,
         },
      .EnumerateInstanceExtensionProperties =
         panvk_EnumerateInstanceExtensionProperties,
      .CreateInstance = panvk_CreateInstance,
      .GetInstanceProcAddr = panvk_GetInstanceProcAddr,
   };

   mesa_logi("panvk: Warning: Android Vulkan implementation is experimental");

   *dev = &hal_dev->common;
   return 0;
}

static int
panvk_hal_close(struct hw_device_t *dev)
{
   /* hwvulkan.h claims that hw_device_t::close() is never called. */
   return -1;
}

/* If any bits in test_mask are set, then unset them and return true. */
static inline bool
unmask32(uint32_t *inout_mask, uint32_t test_mask)
{
   uint32_t orig_mask = *inout_mask;
   *inout_mask &= ~test_mask;
   return *inout_mask != orig_mask;
}

static VkResult
format_supported_with_usage(VkDevice device_h, VkFormat format,
                            VkImageUsageFlags imageUsage)
{
   VK_FROM_HANDLE(vk_device, device, device_h);
   struct vk_physical_device *phys_dev = device->physical;
   VkPhysicalDevice phys_dev_h = vk_physical_device_to_handle(phys_dev);
   VkResult result;

   const VkPhysicalDeviceImageFormatInfo2 image_format_info = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
      .format = format,
      .type = VK_IMAGE_TYPE_2D,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = imageUsage,
   };

   VkImageFormatProperties2 image_format_props = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
   };

   /* Check that requested format and usage are supported. */
   result = panvk_GetPhysicalDeviceImageFormatProperties2(
      phys_dev_h, &image_format_info, &image_format_props);
   if (result != VK_SUCCESS) {
      return vk_errorf(device, result,
                       "v3dv_GetPhysicalDeviceImageFormatProperties2 failed "
                       "inside %s",
                       __func__);
   }

   return VK_SUCCESS;
}

static struct u_gralloc *
panvk_get_u_gralloc(struct VkDevice_T *device_h)
{
   VK_FROM_HANDLE(vk_device, device, device_h);
   VkInstance instance_h = vk_instance_to_handle(device->physical->instance);
   VK_FROM_HANDLE(panvk_instance, pinstance, instance_h);
   return pinstance->u_gralloc;
}

static VkResult
setup_gralloc0_usage(struct vk_device *device, VkFormat format,
                     VkImageUsageFlags imageUsage, int *grallocUsage)
{
   if (unmask32(&imageUsage, VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
      *grallocUsage |= GRALLOC_USAGE_HW_RENDER;

   if (unmask32(&imageUsage, VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                VK_IMAGE_USAGE_SAMPLED_BIT |
                                VK_IMAGE_USAGE_STORAGE_BIT |
                                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
      *grallocUsage |= GRALLOC_USAGE_HW_TEXTURE;

   /* All VkImageUsageFlags not explicitly checked here are unsupported for
    * gralloc swapchains.
    */
   if (imageUsage != 0) {
      return vk_errorf(device, VK_ERROR_FORMAT_NOT_SUPPORTED,
                       "unsupported VkImageUsageFlags(0x%x) for gralloc "
                       "swapchain",
                       imageUsage);
   }

   /* Swapchain assumes direct displaying, therefore enable COMPOSER flag,
    * In case format is not supported by display controller, gralloc will
    * drop this flag and still allocate the buffer in VRAM
    */
   *grallocUsage |= GRALLOC_USAGE_HW_COMPOSER;

   if (*grallocUsage == 0)
      return VK_ERROR_FORMAT_NOT_SUPPORTED;

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_GetSwapchainGrallocUsageANDROID(VkDevice device_h, VkFormat format,
                                      VkImageUsageFlags imageUsage,
                                      int *grallocUsage)
{
   VK_FROM_HANDLE(vk_device, device, device_h);
   VkResult result;

   result = format_supported_with_usage(device_h, format, imageUsage);
   if (result != VK_SUCCESS)
      return result;

   *grallocUsage = 0;
   return setup_gralloc0_usage(device, format, imageUsage, grallocUsage);
}

#if ANDROID_API_LEVEL >= 26
VKAPI_ATTR VkResult VKAPI_CALL
panvk_GetSwapchainGrallocUsage2ANDROID(
   VkDevice device_h, VkFormat format, VkImageUsageFlags imageUsage,
   VkSwapchainImageUsageFlagsANDROID swapchainImageUsage,
   uint64_t *grallocConsumerUsage, uint64_t *grallocProducerUsage)
{
   VK_FROM_HANDLE(vk_device, device, device_h);
   VkResult result;

   *grallocConsumerUsage = 0;
   *grallocProducerUsage = 0;

   result = format_supported_with_usage(device_h, format, imageUsage);
   if (result != VK_SUCCESS)
      return result;

   int32_t grallocUsage = 0;
   result = setup_gralloc0_usage(device, format, imageUsage, &grallocUsage);
   if (result != VK_SUCCESS)
      return result;

   /* Setup gralloc1 usage flags from gralloc0 flags. */

   if (grallocUsage & GRALLOC_USAGE_HW_RENDER)
      *grallocProducerUsage |= GRALLOC1_PRODUCER_USAGE_GPU_RENDER_TARGET;

   if (grallocUsage & GRALLOC_USAGE_HW_TEXTURE)
      *grallocConsumerUsage |= GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE;

   if (grallocUsage & GRALLOC_USAGE_HW_COMPOSER) {
      /* GPU composing case */
      *grallocConsumerUsage |= GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE;
      /* Hardware composing case */
      *grallocConsumerUsage |= GRALLOC1_CONSUMER_USAGE_HWCOMPOSER;
   }

   if (swapchainImageUsage & VK_SWAPCHAIN_IMAGE_USAGE_SHARED_BIT_ANDROID) {
      uint64_t front_rendering_usage = 0;
      u_gralloc_get_front_rendering_usage(panvk_get_u_gralloc(device_h),
                                          &front_rendering_usage);
      *grallocProducerUsage |= front_rendering_usage;
   }

   return VK_SUCCESS;
}
#endif

static VkResult
vk_gralloc_to_drm_explicit_layout(
   struct u_gralloc *gralloc, struct u_gralloc_buffer_handle *in_hnd,
   VkImageDrmFormatModifierExplicitCreateInfoEXT *out,
   VkSubresourceLayout *out_layouts, int max_planes)
{
   struct u_gralloc_buffer_basic_info info;

   if (u_gralloc_get_buffer_basic_info(gralloc, in_hnd, &info) != 0)
      return VK_ERROR_INVALID_EXTERNAL_HANDLE;

   if (info.num_planes > max_planes)
      return VK_ERROR_INVALID_EXTERNAL_HANDLE;

   bool is_disjoint = false;
   for (int i = 1; i < info.num_planes; i++) {
      if (info.offsets[i] == 0) {
         is_disjoint = true;
         break;
      }
   }

   if (is_disjoint) {
      /* We don't support disjoint planes yet */
      return VK_ERROR_INVALID_EXTERNAL_HANDLE;
   }

   memset(out_layouts, 0, sizeof(*out_layouts) * info.num_planes);
   memset(out, 0, sizeof(*out));

   out->sType =
      VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
   out->pPlaneLayouts = out_layouts;

   out->drmFormatModifier = info.modifier;
   out->drmFormatModifierPlaneCount = info.num_planes;
   for (int i = 0; i < info.num_planes; i++) {
      out_layouts[i].offset = info.offsets[i];
      out_layouts[i].rowPitch = info.strides[i];
   }

   if (info.drm_fourcc == DRM_FORMAT_YVU420) {
      /* Swap the U and V planes to match the
       * VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM */
      VkSubresourceLayout tmp = out_layouts[1];
      out_layouts[1] = out_layouts[2];
      out_layouts[2] = tmp;
   }

   return VK_SUCCESS;
}

#define ANDROID_MAX_PLANE_COUNT 4

enum android_buffer_type {
   ANDROID_BUFFER_NATIVE,
   ANDROID_BUFFER_HARDWARE,
};

struct panvk_android_image {
   enum android_buffer_type type;
   VkImageDrmFormatModifierExplicitCreateInfoEXT android_create_info;
   VkSubresourceLayout android_plane_layout[ANDROID_MAX_PLANE_COUNT];
   VkDeviceMemory anb_memory;
};

VkResult
panvk_import_anb(VkDevice device_h, const VkImageCreateInfo *pCreateInfo,
                 const VkAllocationCallbacks *alloc, VkImage image_h)
{
   VkResult result;
   VK_FROM_HANDLE(panvk_image, image, image_h);
   assert(panvk_is_image_anb(image));
   struct panvk_android_image *aimage = image->android_image;

   const VkNativeBufferANDROID *native_buffer =
      vk_find_struct_const(pCreateInfo->pNext, NATIVE_BUFFER_ANDROID);

   assert(native_buffer);
   assert(native_buffer->handle);
   assert(native_buffer->handle->numFds > 0);

   const VkMemoryDedicatedAllocateInfo ded_alloc = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
      .pNext = NULL,
      .buffer = VK_NULL_HANDLE,
      .image = image_h};

   const VkImportMemoryFdInfoKHR import_info = {
      .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
      .pNext = &ded_alloc,
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
      .fd = os_dupfd_cloexec(native_buffer->handle->data[0]),
   };

   result = panvk_AllocateMemory(
      device_h,
      &(VkMemoryAllocateInfo){
         .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
         .pNext = &import_info,
         .allocationSize = lseek(import_info.fd, 0, SEEK_END),
         .memoryTypeIndex = 0,
      },
      alloc, &aimage->anb_memory);

   if (result != VK_SUCCESS) {
      close(import_info.fd);
      return result;
   }

   VkBindImageMemoryInfo bind_info = {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .image = image_h,
      .memory = aimage->anb_memory,
      .memoryOffset = 0,
   };

   return panvk_BindImageMemory2(device_h, 1, &bind_info);
}

VkResult
panvk_android_image_create(VkDevice device_h,
                           const VkImageCreateInfo *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator,
                           struct panvk_android_image **out_aimage)
{
   const VkNativeBufferANDROID *native_buffer =
      vk_find_struct_const(pCreateInfo->pNext, NATIVE_BUFFER_ANDROID);

   if (!native_buffer)
      return VK_SUCCESS;

   VK_FROM_HANDLE(vk_device, device, device_h);

   struct panvk_android_image *aimage =
      vk_zalloc2(&device->alloc, pAllocator, sizeof(*aimage), 8,
                 VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!aimage)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   aimage->type = ANDROID_BUFFER_NATIVE;

   struct u_gralloc_buffer_handle gr_handle = {
      .handle = native_buffer->handle,
      .hal_format = native_buffer->format,
      .pixel_stride = native_buffer->stride,
   };

   VkResult result = vk_gralloc_to_drm_explicit_layout(
      panvk_get_u_gralloc(device_h), &gr_handle, &aimage->android_create_info,
      aimage->android_plane_layout, ANDROID_MAX_PLANE_COUNT);
   if (result != VK_SUCCESS)
      goto fail;

   *out_aimage = aimage;

   return VK_SUCCESS;

fail:
   panvk_android_image_destroy(device_h, pAllocator, &aimage);
   return result;
}

void
panvk_android_image_destroy(VkDevice device_h,
                            const VkAllocationCallbacks *pAllocator,
                            struct panvk_android_image **aimage)
{
   if (!*aimage)
      return;

   if ((*aimage)->anb_memory != VK_NULL_HANDLE)
      panvk_FreeMemory(device_h, (*aimage)->anb_memory, pAllocator);

   VK_FROM_HANDLE(vk_device, device, device_h);

   vk_free2(&device->alloc, pAllocator, *aimage);
   *aimage = NULL;
}

VkResult
panvk_process_anb(struct panvk_android_image *aimage, uint64_t *out_modifier,
                  const VkSubresourceLayout **out_layouts)
{
   *out_modifier = aimage->android_create_info.drmFormatModifier;
   *out_layouts = aimage->android_plane_layout;

   return VK_SUCCESS;
}

bool
panvk_is_image_anb(struct panvk_image *image)
{
   return image->android_image &&
          image->android_image->type == ANDROID_BUFFER_NATIVE;
}