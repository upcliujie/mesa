/*
 * Copyright © 2021 Intel Corporation
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

#include "vk_fence.h"

#include "util/os_time.h"

#ifndef _WIN32
#include <unistd.h>
#endif

#include "vk_common_entrypoints.h"
#include "vk_device.h"
#include "vk_log.h"
#include "vk_physical_device.h"
#include "vk_util.h"

static VkExternalFenceHandleTypeFlags
vk_sync_fence_import_types(const struct vk_sync_type *type)
{
   VkExternalFenceHandleTypeFlags handle_types = 0;

   if (type->import_opaque_fd)
      handle_types |= VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT;

   if (type->import_sync_file)
      handle_types |= VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;


#ifdef VK_USE_PLATFORM_WIN32_KHR
   if (type->import_win32_handle)
      handle_types |= VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#endif

   return handle_types;
}

static VkExternalFenceHandleTypeFlags
vk_sync_fence_export_types(const struct vk_sync_type *type)
{
   VkExternalFenceHandleTypeFlags handle_types = 0;

   if (type->export_opaque_fd)
      handle_types |= VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT;

   if (type->export_sync_file)
      handle_types |= VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;

#ifdef VK_USE_PLATFORM_WIN32_KHR
   if (type->export_win32_handle)
      handle_types |= VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#endif

   return handle_types;
}

static VkExternalFenceHandleTypeFlags
vk_sync_fence_handle_types(const struct vk_sync_type *type)
{
   return vk_sync_fence_export_types(type) &
          vk_sync_fence_import_types(type);
}

static const struct vk_sync_type *
get_fence_sync_type(struct vk_physical_device *pdevice,
                    VkExternalFenceHandleTypeFlags handle_types)
{
   static const enum vk_sync_features req_features =
      VK_SYNC_FEATURE_BINARY |
      VK_SYNC_FEATURE_CPU_WAIT |
      VK_SYNC_FEATURE_CPU_RESET;

   for (const struct vk_sync_type *const *t =
        pdevice->supported_sync_types; *t; t++) {
      if (req_features & ~(*t)->features)
         continue;

      if (handle_types & ~vk_sync_fence_handle_types(*t))
         continue;

      return *t;
   }

   return NULL;
}

VkResult
vk_fence_create(struct vk_device *device,
                const VkFenceCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                struct vk_fence **fence_out)
{
   struct vk_fence *fence;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);

   const VkExportFenceCreateInfo *export =
      vk_find_struct_const(pCreateInfo->pNext, EXPORT_FENCE_CREATE_INFO);
   VkExternalFenceHandleTypeFlags handle_types =
      export ? export->handleTypes : 0;

   struct vk_sync_init_info info = {
      .type = get_fence_sync_type(device->physical, handle_types),
      .flags = 0,
      .initial_value = (pCreateInfo->flags & VK_FENCE_CREATE_SIGNALED_BIT) ? 1 : 0,
   };

   if (info.type == NULL) {
      /* We should always be able to get a fence type for internal */
      assert(get_fence_sync_type(device->physical, 0) != NULL);
      return vk_errorf(device, VK_ERROR_INVALID_EXTERNAL_HANDLE,
                       "Combination of external handle types is unsupported "
                       "for VkFence creation.");
   }

   /* Allocate a vk_fence + vk_sync implementation. Because the permanent
    * field of vk_fence is the base field of the vk_sync implementation, we
    * can make the 2 structures overlap.
    */
   size_t size = offsetof(struct vk_fence, permanent) + info.type->size;
   fence = vk_object_zalloc(device, pAllocator, size, VK_OBJECT_TYPE_FENCE);
   if (fence == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   if (handle_types)
      info.flags |= VK_SYNC_IS_SHAREABLE;

#ifdef VK_USE_PLATFORM_WIN32_KHR
   if (handle_types & VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT) {
      const VkExportFenceWin32HandleInfoKHR *win32_export =
         vk_find_struct_const(export->pNext, EXPORT_FENCE_WIN32_HANDLE_INFO_KHR);
      DWORD req_access = EVENT_MODIFY_STATE | SYNCHRONIZE;

      info.win32.access = req_access;

      if (win32_export) {
         info.win32.sec_attrs = win32_export->pAttributes;
         info.win32.access = win32_export->dwAccess;
         info.win32.name = win32_export->name;
      }

      if ((info.win32.access & req_access) != req_access)
         return vk_error(device, VK_ERROR_INVALID_EXTERNAL_HANDLE);
   }
#endif

   VkResult result = vk_sync_init(device, &fence->permanent, &info);
   if (result != VK_SUCCESS) {
      vk_object_free(device, pAllocator, fence);
      return result;
   }

   *fence_out = fence;

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_CreateFence(VkDevice _device,
                      const VkFenceCreateInfo *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkFence *pFence)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   struct vk_fence *fence;

   VkResult result = vk_fence_create(device, pCreateInfo, pAllocator, &fence);
   if (result != VK_SUCCESS)
      return result;

   *pFence = vk_fence_to_handle(fence);

   return VK_SUCCESS;
}

void
vk_fence_reset_temporary(struct vk_device *device,
                         struct vk_fence *fence)
{
   if (fence->temporary == NULL)
      return;

   vk_sync_destroy(device, fence->temporary);
   fence->temporary = NULL;
}

void
vk_fence_destroy(struct vk_device *device,
                 struct vk_fence *fence,
                 const VkAllocationCallbacks *pAllocator)
{
   vk_fence_reset_temporary(device, fence);
   vk_sync_finish(device, &fence->permanent);

   vk_object_free(device, pAllocator, fence);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_DestroyFence(VkDevice _device,
                       VkFence _fence,
                       const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_fence, fence, _fence);

   if (fence == NULL)
      return;

   vk_fence_destroy(device, fence, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_ResetFences(VkDevice _device,
                      uint32_t fenceCount,
                      const VkFence *pFences)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   for (uint32_t i = 0; i < fenceCount; i++) {
      VK_FROM_HANDLE(vk_fence, fence, pFences[i]);

      /* From the Vulkan 1.2.194 spec:
       *
       *    "If any member of pFences currently has its payload imported with
       *    temporary permanence, that fence’s prior permanent payload is
       *    first restored. The remaining operations described therefore
       *    operate on the restored payload."
       */
      vk_fence_reset_temporary(device, fence);

      VkResult result = vk_sync_reset(device, &fence->permanent);
      if (result != VK_SUCCESS)
         return result;
   }

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_GetFenceStatus(VkDevice _device,
                         VkFence _fence)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_fence, fence, _fence);

   if (vk_device_is_lost(device))
      return VK_ERROR_DEVICE_LOST;

   VkResult result = vk_sync_wait(device, vk_fence_get_active_sync(fence),
                                  0 /* wait_value */,
                                  VK_SYNC_WAIT_COMPLETE,
                                  0 /* abs_timeout_ns */);
   if (result == VK_TIMEOUT)
      return VK_NOT_READY;
   else
      return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_WaitForFences(VkDevice _device,
                        uint32_t fenceCount,
                        const VkFence *pFences,
                        VkBool32 waitAll,
                        uint64_t timeout)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   if (vk_device_is_lost(device))
      return VK_ERROR_DEVICE_LOST;

   if (fenceCount == 0)
      return VK_SUCCESS;

   uint64_t abs_timeout_ns = os_time_get_absolute_timeout(timeout);

   STACK_ARRAY(struct vk_sync_wait, waits, fenceCount);

   for (uint32_t i = 0; i < fenceCount; i++) {
      VK_FROM_HANDLE(vk_fence, fence, pFences[i]);
      waits[i] = (struct vk_sync_wait) {
         .sync = vk_fence_get_active_sync(fence),
         .stage_mask = ~(VkPipelineStageFlags2KHR)0,
      };
   }

   enum vk_sync_wait_flags wait_flags = VK_SYNC_WAIT_COMPLETE;
   if (!waitAll)
      wait_flags |= VK_SYNC_WAIT_ANY;

   VkResult result = vk_sync_wait_many(device, fenceCount, waits,
                                       wait_flags, abs_timeout_ns);

   STACK_ARRAY_FINISH(waits);

   VkResult device_status = vk_device_check_status(device);
   if (device_status != VK_SUCCESS)
      return device_status;

   return result;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_GetPhysicalDeviceExternalFenceProperties(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo,
   VkExternalFenceProperties *pExternalFenceProperties)
{
   VK_FROM_HANDLE(vk_physical_device, pdevice, physicalDevice);

   assert(pExternalFenceInfo->sType ==
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO);
   const VkExternalFenceHandleTypeFlagBits handle_type =
      pExternalFenceInfo->handleType;

   const struct vk_sync_type *sync_type =
      get_fence_sync_type(pdevice, handle_type);
   if (sync_type == NULL) {
      pExternalFenceProperties->exportFromImportedHandleTypes = 0;
      pExternalFenceProperties->compatibleHandleTypes = 0;
      pExternalFenceProperties->externalFenceFeatures = 0;
      return;
   }

   VkExternalFenceHandleTypeFlagBits import =
      vk_sync_fence_import_types(sync_type);
   VkExternalFenceHandleTypeFlagBits export =
      vk_sync_fence_export_types(sync_type);

   if (handle_type != VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT) {
      const struct vk_sync_type *opaque_sync_type =
         get_fence_sync_type(pdevice, VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT);

      /* If we're a different vk_sync_type than the one selected when only
       * OPAQUE_FD is set, then we can't import/export OPAQUE_FD.  Put
       * differently, there can only be one OPAQUE_FD sync type.
       */
      if (sync_type != opaque_sync_type) {
         import &= ~VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT;
         export &= ~VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT;
      }
   }

   VkExternalFenceHandleTypeFlags compatible = import & export;
   VkExternalFenceFeatureFlags features = 0;
   if (handle_type & export)
      features |= VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT;
   if (handle_type & import)
      features |= VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT;

   pExternalFenceProperties->exportFromImportedHandleTypes = export;
   pExternalFenceProperties->compatibleHandleTypes = compatible;
   pExternalFenceProperties->externalFenceFeatures = features;
}

#ifndef _WIN32

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_ImportFenceFdKHR(VkDevice _device,
                           const VkImportFenceFdInfoKHR *pImportFenceFdInfo)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_fence, fence, pImportFenceFdInfo->fence);

   assert(pImportFenceFdInfo->sType ==
          VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR);

   const int fd = pImportFenceFdInfo->fd;
   const VkExternalFenceHandleTypeFlagBits handle_type =
      pImportFenceFdInfo->handleType;

   struct vk_sync *temporary = NULL, *sync;
   if (pImportFenceFdInfo->flags & VK_FENCE_IMPORT_TEMPORARY_BIT) {
      struct vk_sync_init_info info = {
         .type = get_fence_sync_type(device->physical, handle_type),
         .flags = 0,
         .initial_value = 0,
      };

      VkResult result = vk_sync_create(device, &info, &temporary);
      if (result != VK_SUCCESS)
         return result;

      sync = temporary;
   } else {
      sync = &fence->permanent;
   }
   assert(handle_type & vk_sync_fence_handle_types(sync->type));

   VkResult result;
   switch (pImportFenceFdInfo->handleType) {
   case VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT:
      result = vk_sync_import_opaque_fd(device, sync, fd);
      break;

   case VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT:
      result = vk_sync_import_sync_file(device, sync, fd);
      break;

   default:
      result = vk_error(fence, VK_ERROR_INVALID_EXTERNAL_HANDLE);
   }

   if (result != VK_SUCCESS) {
      if (temporary != NULL)
         vk_sync_destroy(device, temporary);
      return result;
   }

   /* From the Vulkan 1.2.194 spec:
    *
    *    "Importing a fence payload from a file descriptor transfers
    *    ownership of the file descriptor from the application to the
    *    Vulkan implementation. The application must not perform any
    *    operations on the file descriptor after a successful import."
    *
    * If the import fails, we leave the file descriptor open.
    */
   if (fd != -1)
      close(fd);

   if (temporary) {
      vk_fence_reset_temporary(device, fence);
      fence->temporary = temporary;
   }

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_GetFenceFdKHR(VkDevice _device,
                        const VkFenceGetFdInfoKHR *pGetFdInfo,
                        int *pFd)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_fence, fence, pGetFdInfo->fence);

   assert(pGetFdInfo->sType == VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR);

   struct vk_sync *sync = vk_fence_get_active_sync(fence);

   VkResult result;
   switch (pGetFdInfo->handleType) {
   case VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT:
      result = vk_sync_export_opaque_fd(device, sync, pFd);
      if (unlikely(result != VK_SUCCESS))
         return result;
      break;

   case VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT:
      /* There's no direct spec quote for this but the same rules as for
       * semaphore export apply.  We can't export a sync file from a fence
       * if the fence event hasn't been submitted to the kernel yet.
       */
      if (device->timeline_mode == VK_DEVICE_TIMELINE_MODE_ASSISTED) {
         result = vk_sync_wait(device, sync, 0,
                               VK_SYNC_WAIT_PENDING,
                               UINT64_MAX);
         if (unlikely(result != VK_SUCCESS))
            return result;
      }

      result = vk_sync_export_sync_file(device, sync, pFd);
      if (unlikely(result != VK_SUCCESS))
         return result;

      /* From the Vulkan 1.2.194 spec:
       *
       *    "Export operations have the same transference as the specified
       *    handle type’s import operations. Additionally, exporting a fence
       *    payload to a handle with copy transference has the same side
       *    effects on the source fence’s payload as executing a fence reset
       *    operation."
       *
       * In other words, exporting a sync file also resets the fence.  We
       * only care about this for the permanent payload because the temporary
       * payload will be destroyed below.
       */
      if (sync == &fence->permanent) {
         result = vk_sync_reset(device, sync);
         if (unlikely(result != VK_SUCCESS))
            return result;
      }
      break;

   default:
      unreachable("Invalid fence export handle type");
   }

   /* From the Vulkan 1.2.194 spec:
    *
    *    "Export operations have the same transference as the specified
    *    handle type’s import operations. [...]  If the fence was using a
    *    temporarily imported payload, the fence’s prior permanent payload
    *    will be restored.
    */
   vk_fence_reset_temporary(device, fence);

   return VK_SUCCESS;
}

#elif defined(VK_USE_PLATFORM_WIN32_KHR)

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_ImportFenceWin32HandleKHR(VkDevice _device,
                                    const VkImportFenceWin32HandleInfoKHR *info)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_fence, fence, info->fence);

   assert(info->sType == VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR);

   HANDLE handle = info->handle;
   LPCWSTR name = info->name;
   const VkExternalFenceHandleTypeFlagBits handle_type = info->handleType;

   struct vk_sync *temporary = NULL, *sync;
   if (info->flags & VK_FENCE_IMPORT_TEMPORARY_BIT) {
      const struct vk_sync_init_info info = {
         .type = get_fence_sync_type(device->physical, handle_type),
         .flags = 0,
         .initial_value = 0,
      };

      VkResult result = vk_sync_create(device, &info, &temporary);
      if (result != VK_SUCCESS)
         return result;

      sync = temporary;
   } else {
      sync = &fence->permanent;
   }
   assert(handle_type & vk_sync_fence_handle_types(sync->type));

   VkResult result;
   switch (info->handleType) {
   case VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
      result = vk_sync_import_win32_handle(device, sync, handle, name);
      break;

   case VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
      /* Global share handles not supported. */
   default:
      result = vk_error(fence, VK_ERROR_INVALID_EXTERNAL_HANDLE);
   }

   if (result != VK_SUCCESS) {
      if (temporary != NULL)
         vk_sync_destroy(device, temporary);
      return result;
   }

   if (temporary) {
      vk_fence_reset_temporary(device, fence);
      fence->temporary = temporary;
   }

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_GetFenceWin32HandleKHR(VkDevice _device,
                                 const VkFenceGetWin32HandleInfoKHR *info,
                                 HANDLE *pHandle)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_fence, fence, info->fence);

   assert(info->sType == VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR);

   struct vk_sync *sync = vk_fence_get_active_sync(fence);

   VkResult result;
   switch (info->handleType) {
   case VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
      result = vk_sync_export_win32_handle(device, sync, pHandle);
      break;

   case VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
      /* We don't support Global Share handles. */
   default:
      unreachable("Invalid fence export handle type");
   }

   if (unlikely(result != VK_SUCCESS))
      return result;

   /* From the Vulkan 1.2.194 spec:
    *
    *    "Export operations have the same transference as the specified
    *    handle type’s import operations. [...]  If the fence was using a
    *    temporarily imported payload, the fence’s prior permanent payload
    *    will be restored.
    */
   vk_fence_reset_temporary(device, fence);

   return VK_SUCCESS;
}

#endif /* !defined(_WIN32) */
