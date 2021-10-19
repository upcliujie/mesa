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

#include "vk_semaphore.h"

#include "util/os_time.h"

#include "vk_common_entrypoints.h"
#include "vk_device.h"
#include "vk_log.h"
#include "vk_physical_device.h"
#include "vk_util.h"

static VkExternalSemaphoreHandleTypeFlags
vk_sync_semaphore_import_types(const struct vk_sync_type *type)
{
   VkExternalSemaphoreHandleTypeFlags handle_types = 0;

   if (type->import_opaque_fd)
      handle_types |= VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

   if (type->import_sync_file)
      handle_types |= VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

   return handle_types;
}

static VkExternalSemaphoreHandleTypeFlags
vk_sync_semaphore_export_types(const struct vk_sync_type *type)
{
   VkExternalSemaphoreHandleTypeFlags handle_types = 0;

   if (type->export_opaque_fd)
      handle_types |= VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

   if (type->export_sync_file)
      handle_types |= VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

   return handle_types;
}

static VkExternalSemaphoreHandleTypeFlags
vk_sync_semaphore_handle_types(const struct vk_sync_type *type)
{
   return vk_sync_semaphore_export_types(type) &
          vk_sync_semaphore_import_types(type);
}

static const struct vk_sync_type *
get_semaphore_sync_type(struct vk_physical_device *pdevice,
                        VkSemaphoreType semaphore_type,
                        VkExternalSemaphoreHandleTypeFlags handle_types)
{
   assert(semaphore_type == VK_SEMAPHORE_TYPE_BINARY ||
          semaphore_type == VK_SEMAPHORE_TYPE_TIMELINE);
   bool is_timeline = (semaphore_type == VK_SEMAPHORE_TYPE_TIMELINE);

   const struct vk_sync_type *const *t;
   for (t = pdevice->supported_sync_types; *t; t++) {
      /* TODO: This may be wrong for DXGI semaphores */
      if ((*t)->is_timeline != is_timeline)
         continue;

      if (!(*t)->reset || !vk_sync_type_has_cpu_wait(*t))
         continue;

      if (handle_types & ~vk_sync_semaphore_handle_types(*t))
         continue;

      return *t;
   }

   return NULL;
}

static VkSemaphoreType
get_semaphore_type(const void *pNext, uint64_t *initial_value)
{
   const VkSemaphoreTypeCreateInfo *type_info =
      vk_find_struct_const(pNext, SEMAPHORE_TYPE_CREATE_INFO);

   if (!type_info)
      return VK_SEMAPHORE_TYPE_BINARY;

   if (initial_value)
      *initial_value = type_info->initialValue;
   return type_info->semaphoreType;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_CreateSemaphore(VkDevice _device,
                          const VkSemaphoreCreateInfo *pCreateInfo,
                          const VkAllocationCallbacks *pAllocator,
                          VkSemaphore *pSemaphore)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   struct vk_semaphore *semaphore;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);

   uint64_t initial_value = 0;
   const VkSemaphoreType semaphore_type =
      get_semaphore_type(pCreateInfo->pNext, &initial_value);

   const VkExportSemaphoreCreateInfo *export =
      vk_find_struct_const(pCreateInfo->pNext, EXPORT_SEMAPHORE_CREATE_INFO);
   VkExternalSemaphoreHandleTypeFlags handle_types =
      export ? export->handleTypes : 0;

   const struct vk_sync_type *sync_type =
      get_semaphore_sync_type(device->physical, semaphore_type, handle_types);
   if (sync_type == NULL) {
      /* We should always be able to get a semaphore type for internal */
      assert(get_semaphore_sync_type(device->physical, semaphore_type, 0) != NULL);
      return vk_errorf(device, VK_ERROR_INVALID_EXTERNAL_HANDLE,
                       "Combination of external handle types is unsupported "
                       "for VkSemaphore creation.");
   }

   size_t size = offsetof(struct vk_semaphore, permanent) + sync_type->size;
   semaphore = vk_object_zalloc(device, pAllocator, size,
                                VK_OBJECT_TYPE_SEMAPHORE);
   if (semaphore == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   semaphore->type = semaphore_type;

   VkResult result = vk_sync_init(device, &semaphore->permanent,
                                  sync_type, initial_value);
   if (result != VK_SUCCESS) {
      vk_object_free(device, pAllocator, semaphore);
      return result;
   }

   *pSemaphore = vk_semaphore_to_handle(semaphore);

   return VK_SUCCESS;
}

void
vk_semaphore_reset_temporary(struct vk_device *device,
                             struct vk_semaphore *semaphore)
{
   if (semaphore->temporary == NULL)
      return;

   vk_sync_destroy(device, semaphore->temporary);
   semaphore->temporary = NULL;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_DestroySemaphore(VkDevice _device,
                           VkSemaphore _semaphore,
                           const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_semaphore, semaphore, _semaphore);

   if (semaphore == NULL)
      return;

   vk_semaphore_reset_temporary(device, semaphore);
   vk_sync_finish(device, &semaphore->permanent);

   vk_object_free(device, pAllocator, semaphore);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_GetPhysicalDeviceExternalSemaphoreProperties(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo,
   VkExternalSemaphoreProperties *pExternalSemaphoreProperties)
{
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_ImportSemaphoreFdKHR(VkDevice _device,
                               const VkImportSemaphoreFdInfoKHR *pImportSemaphoreFdInfo)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_semaphore, semaphore, pImportSemaphoreFdInfo->semaphore);

   assert(pImportSemaphoreFdInfo->sType ==
          VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR);

   const int fd = pImportSemaphoreFdInfo->fd;
   const VkExternalSemaphoreHandleTypeFlagBits handle_type =
      pImportSemaphoreFdInfo->handleType;

   struct vk_sync *temporary = NULL, *sync;
   if (pImportSemaphoreFdInfo->flags & VK_SEMAPHORE_IMPORT_TEMPORARY_BIT) {
      const struct vk_sync_type *sync_type =
         get_semaphore_sync_type(device->physical, semaphore->type, handle_type);

      VkResult result = vk_sync_create(device, sync_type, 0, &temporary);
      if (result != VK_SUCCESS)
         return result;

      sync = temporary;
   } else {
      sync = &semaphore->permanent;
   }
   assert(handle_type & vk_sync_semaphore_handle_types(sync->type));

   VkResult result;
   switch (pImportSemaphoreFdInfo->handleType) {
   case VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT:
      result = vk_sync_import_opaque_fd(device, sync, fd);
      break;

   case VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT:
      result = vk_sync_import_sync_file(device, sync, fd);
      break;

   default:
      result = vk_error(semaphore, VK_ERROR_INVALID_EXTERNAL_HANDLE);
   }

   if (result != VK_SUCCESS) {
      if (temporary != NULL)
         vk_sync_destroy(device, temporary);
      return result;
   }

   /* From the Vulkan 1.0.53 spec:
    *
    *    "Importing a semaphore payload from a file descriptor transfers
    *    ownership of the file descriptor from the application to the
    *    Vulkan implementation. The application must not perform any
    *    operations on the file descriptor after a successful import."
    *
    * If the import fails, we leave the file descriptor open.
    */
   if (fd != -1)
      close(fd);

   if (temporary) {
      vk_semaphore_reset_temporary(device, semaphore);
      semaphore->temporary = temporary;
   }

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_GetSemaphoreFdKHR(VkDevice _device,
                            const VkSemaphoreGetFdInfoKHR *pGetFdInfo,
                            int *pFd)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_semaphore, semaphore, pGetFdInfo->semaphore);

   assert(pGetFdInfo->sType == VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR);

   struct vk_sync *sync = vk_semaphore_get_active_sync(semaphore);

   VkResult result;
   switch (pGetFdInfo->handleType) {
   case VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT:
      result = vk_sync_export_opaque_fd(device, sync, pFd);
      break;

   case VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT:
      result = vk_sync_export_sync_file(device, sync, pFd);
      break;

   default:
      unreachable("Invalid semaphore export handle type");
   }

   if (result != VK_SUCCESS)
      return result;

   /* From the Vulkan 1.0.53 spec:
    *
    *    "Export operations have the same transference as the specified handle
    *    type’s import operations. [...] If the semaphore was using a
    *    temporarily imported payload, the semaphore’s prior permanent payload
    *    will be restored.
    */
   vk_semaphore_reset_temporary(device, semaphore);

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_GetSemaphoreCounterValue(VkDevice _device,
                                   VkSemaphore _semaphore,
                                   uint64_t *pValue)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_semaphore, semaphore, _semaphore);

   struct vk_sync *sync = vk_semaphore_get_active_sync(semaphore);
   return vk_sync_get_value(device, sync, pValue);
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_WaitSemaphores(VkDevice _device,
                         const VkSemaphoreWaitInfo *pWaitInfo,
                         uint64_t timeout)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   if (pWaitInfo->semaphoreCount == 0)
      return VK_SUCCESS;

   uint64_t abs_timeout_ns = os_time_get_absolute_timeout(timeout);

   const uint32_t wait_count = pWaitInfo->semaphoreCount;
   STACK_ARRAY(struct vk_sync_wait, waits, wait_count);

   for (uint32_t i = 0; i < wait_count; i++) {
      VK_FROM_HANDLE(vk_semaphore, semaphore, pWaitInfo->pSemaphores[i]);
      waits[i] = (struct vk_sync_wait) {
         .sync = vk_semaphore_get_active_sync(semaphore),
         .stage_mask = ~(VkPipelineStageFlags2KHR)0,
         .wait_value = pWaitInfo->pValues[i],
      };
   }

   VkResult result;
   if (pWaitInfo->flags & VK_SEMAPHORE_WAIT_ANY_BIT) {
      result = vk_sync_wait_any(device, wait_count, waits,
                                VK_SYNC_WAIT_COMPLETE, abs_timeout_ns);
   } else {
      result = vk_sync_wait_all(device, wait_count, waits,
                                VK_SYNC_WAIT_COMPLETE, abs_timeout_ns);
   }

   STACK_ARRAY_FINISH(waits);

   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_SignalSemaphore(VkDevice _device,
                          const VkSemaphoreSignalInfo *pSignalInfo)
{
   VK_FROM_HANDLE(vk_device, device, _device);
   VK_FROM_HANDLE(vk_semaphore, semaphore, pSignalInfo->semaphore);

   struct vk_sync *sync = vk_semaphore_get_active_sync(semaphore);
   return vk_sync_signal(device, sync, pSignalInfo->value);
}
