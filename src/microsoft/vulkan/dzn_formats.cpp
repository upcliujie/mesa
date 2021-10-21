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
#include "vk_util.h"
#include "vk_format.h"

#include "util/format/u_format.h"
#include "vulkan/wsi/wsi_common.h"

DXGI_FORMAT
dzn_get_format(VkFormat format)
{
   return dzn_pipe_to_dxgi_format(vk_format_to_pipe_format(format));
}

DXGI_FORMAT
dzn_get_rtv_format(VkFormat format)
{
   enum pipe_format f = vk_format_to_pipe_format(format);
   switch (f) {
   case PIPE_FORMAT_Z16_UNORM:
      return DXGI_FORMAT_D16_UNORM;
   case PIPE_FORMAT_Z32_FLOAT:
      return DXGI_FORMAT_D32_FLOAT;
   case PIPE_FORMAT_Z24X8_UNORM:
   case PIPE_FORMAT_X24S8_UINT:
      return DXGI_FORMAT_D24_UNORM_S8_UINT;
   case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
   case PIPE_FORMAT_X32_S8X24_UINT:
      return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
   case PIPE_FORMAT_Z24_UNORM_S8_UINT:
      return DXGI_FORMAT_D24_UNORM_S8_UINT;
   default:
      return dzn_pipe_to_dxgi_format(f);
   }
}

DXGI_FORMAT
dzn_get_srv_format(VkFormat format)
{
   enum pipe_format f = vk_format_to_pipe_format(format);
   switch (f) {
   case PIPE_FORMAT_Z16_UNORM:
      return DXGI_FORMAT_R16_UNORM;
   case PIPE_FORMAT_Z32_FLOAT:
      return DXGI_FORMAT_R32_FLOAT;
   case PIPE_FORMAT_Z24X8_UNORM:
   case PIPE_FORMAT_Z24_UNORM_S8_UINT:
      return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
   case PIPE_FORMAT_X24S8_UINT:
      return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
   case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
      return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
   case PIPE_FORMAT_X32_S8X24_UINT:
      return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
   default:
      return dzn_pipe_to_dxgi_format(f);
   }
}

DXGI_FORMAT
dzn_get_dsv_format(VkFormat format)
{
   enum pipe_format f = vk_format_to_pipe_format(format);
   switch (f) {
   case PIPE_FORMAT_Z24_UNORM_S8_UINT:
   case PIPE_FORMAT_Z24X8_UNORM:
   case PIPE_FORMAT_X24S8_UINT:
      return DXGI_FORMAT_D24_UNORM_S8_UINT;
   case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
   case PIPE_FORMAT_X32_S8X24_UINT:
      return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
   default:
      return dzn_pipe_to_dxgi_format(f);
   }
}

static VkFormatFeatureFlags
image_format_features(VkFormat vk_format,
                      VkImageTiling tiling)
{
   VkFormatFeatureFlags flags = 0;

   enum pipe_format f = vk_format_to_pipe_format(vk_format);
   if (!dzn_is_format_supported(f))
      return 0;

   flags |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
   if (tiling == VK_IMAGE_TILING_OPTIMAL) {
      flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
      if (vk_format_is_depth_or_stencil(vk_format))
         flags |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
      else
         flags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
   }

   return flags;
}

static VkFormatFeatureFlags
buffer_format_features(VkFormat vk_format)
{
   VkFormatFeatureFlags flags = 0;
   return flags;
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice,
                                      VkFormat format,
                                      VkFormatProperties* pFormatProperties)
{
   *pFormatProperties = VkFormatProperties {
      .linearTilingFeatures =
         image_format_features(format, VK_IMAGE_TILING_LINEAR),
      .optimalTilingFeatures =
         image_format_features(format, VK_IMAGE_TILING_OPTIMAL),
      .bufferFeatures =
         buffer_format_features(format),
   };
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice,
                                       VkFormat format,
                                       VkFormatProperties2 *pFormatProperties)
{
   dzn_GetPhysicalDeviceFormatProperties(physicalDevice, format,
                                         &pFormatProperties->formatProperties);

   vk_foreach_struct(ext, pFormatProperties->pNext) {
      dzn_debug_ignored_stype(ext->sType);
   }
}

static VkResult
get_image_format_properties(const VkPhysicalDeviceImageFormatInfo2 *info,
                            VkImageTiling tiling,
                            VkImageFormatProperties *pImageFormatProperties)
{
   VkFormatFeatureFlags format_feature_flags =
      image_format_features(info->format, tiling);
   if (!format_feature_flags)
      goto unsupported;

   if (info->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_STORAGE_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      if (!(format_feature_flags &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
         goto unsupported;
      }
   }

   switch (info->type) {
   case VK_IMAGE_TYPE_1D:
   case VK_IMAGE_TYPE_2D:
      /* TODO: support older feature levels */
      pImageFormatProperties->maxMipLevels = 14;
      pImageFormatProperties->maxArrayLayers = 1 << 14;
      break;
   case VK_IMAGE_TYPE_3D:
      /* TODO: support older feature levels */
      pImageFormatProperties->maxMipLevels = 11;
      pImageFormatProperties->maxArrayLayers = 1;
      break;
   default:
      unreachable("bad VkImageType");
   }

   switch (info->type) {
   case VK_IMAGE_TYPE_1D:
      pImageFormatProperties->maxExtent.width = 1 << pImageFormatProperties->maxMipLevels;
      pImageFormatProperties->maxExtent.height = 1;
      pImageFormatProperties->maxExtent.depth = 1;
      break;
   case VK_IMAGE_TYPE_2D:
      pImageFormatProperties->maxExtent.width = 1 << pImageFormatProperties->maxMipLevels;
      pImageFormatProperties->maxExtent.height = 1 << pImageFormatProperties->maxMipLevels;
      pImageFormatProperties->maxExtent.depth = 1;
      break;
   case VK_IMAGE_TYPE_3D:
      pImageFormatProperties->maxExtent.width = 1 << pImageFormatProperties->maxMipLevels;
      pImageFormatProperties->maxExtent.height = 1 << pImageFormatProperties->maxMipLevels;
      pImageFormatProperties->maxExtent.depth = 1 << pImageFormatProperties->maxMipLevels;
      break;
   default:
      unreachable("bad VkImageType");
   }

   /* From the Vulkan 1.0 spec, section 34.1.1. Supported Sample Counts:
    *
    * sampleCounts will be set to VK_SAMPLE_COUNT_1_BIT if at least one of the
    * following conditions is true:
    *
    *   - tiling is VK_IMAGE_TILING_LINEAR
    *   - type is not VK_IMAGE_TYPE_2D
    *   - flags contains VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
    *   - neither the VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT flag nor the
    *     VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT flag in
    *     VkFormatProperties::optimalTilingFeatures returned by
    *     vkGetPhysicalDeviceFormatProperties is set.
    */
   pImageFormatProperties->sampleCounts = VK_SAMPLE_COUNT_1_BIT;
   if (tiling != VK_IMAGE_TILING_LINEAR &&
       info->type == VK_IMAGE_TYPE_2D &&
       !(info->flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) &&
       (format_feature_flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT ||
        format_feature_flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
      pImageFormatProperties->sampleCounts |= VK_SAMPLE_COUNT_4_BIT;
   }

   if (tiling == VK_IMAGE_TILING_LINEAR)
      pImageFormatProperties->maxMipLevels = 1;

   /* TODO: set correct value here */
   pImageFormatProperties->maxResourceSize = UINT32_MAX;

   return VK_SUCCESS;

unsupported:
   *pImageFormatProperties = VkImageFormatProperties {
      .maxExtent = { 0, 0, 0 },
      .maxMipLevels = 0,
      .maxArrayLayers = 0,
      .sampleCounts = 0,
      .maxResourceSize = 0,
   };

   return VK_ERROR_FORMAT_NOT_SUPPORTED;
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetPhysicalDeviceImageFormatProperties(
   VkPhysicalDevice physicalDevice,
   VkFormat format,
   VkImageType type,
   VkImageTiling tiling,
   VkImageUsageFlags usage,
   VkImageCreateFlags createFlags,
   VkImageFormatProperties *pImageFormatProperties)
{
   const VkPhysicalDeviceImageFormatInfo2 info = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
      .pNext = NULL,
      .format = format,
      .type = type,
      .tiling = tiling,
      .usage = usage,
      .flags = createFlags,
   };

   return get_image_format_properties(&info, tiling, pImageFormatProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL
dzn_GetPhysicalDeviceImageFormatProperties2(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceImageFormatInfo2 *base_info,
   VkImageFormatProperties2 *base_props)
{
   const VkPhysicalDeviceExternalImageFormatInfo *external_info = NULL;
   VkExternalImageFormatProperties *external_props = NULL;
   VkImageTiling tiling = base_info->tiling;

   /* Extract input structs */
   vk_foreach_struct_const(s, base_info->pNext) {
      switch (s->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO:
         external_info = (const VkPhysicalDeviceExternalImageFormatInfo *)s;
         break;
      default:
         dzn_debug_ignored_stype(s->sType);
         break;
      }
   }

   assert(tiling == VK_IMAGE_TILING_OPTIMAL ||
          tiling == VK_IMAGE_TILING_LINEAR);

   /* Extract output structs */
   vk_foreach_struct(s, base_props->pNext) {
      switch (s->sType) {
      case VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES:
         external_props = (VkExternalImageFormatProperties *)s;
         break;
      default:
         dzn_debug_ignored_stype(s->sType);
         break;
      }
   }

   VkResult result =
      get_image_format_properties(base_info, tiling,
                                  &base_props->imageFormatProperties);
   if (result != VK_SUCCESS)
      goto done;

   if (external_info && external_info->handleType != 0) {
      result = VK_ERROR_FORMAT_NOT_SUPPORTED;
   }

done:
   return result;
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceSparseImageFormatProperties(
   VkPhysicalDevice physicalDevice,
   VkFormat format,
   VkImageType type,
   VkSampleCountFlagBits samples,
   VkImageUsageFlags usage,
   VkImageTiling tiling,
   uint32_t *pPropertyCount,
   VkSparseImageFormatProperties *pProperties)
{
   *pPropertyCount = 0;
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceSparseImageFormatProperties2(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceSparseImageFormatInfo2 *pFormatInfo,
   uint32_t *pPropertyCount,
   VkSparseImageFormatProperties2 *pProperties)
{
   *pPropertyCount = 0;
}

VKAPI_ATTR void VKAPI_CALL
dzn_GetPhysicalDeviceExternalBufferProperties(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo,
   VkExternalBufferProperties *pExternalBufferProperties)
{
   pExternalBufferProperties->externalMemoryProperties =
      VkExternalMemoryProperties {
         .compatibleHandleTypes = (VkExternalMemoryHandleTypeFlags)pExternalBufferInfo->handleType,
      };
}
