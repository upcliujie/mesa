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

#include "vk_object.h"

#include "vk_alloc.h"
#include "util/hash_table.h"
#include "util/ralloc.h"

void
vk_object_base_init(struct vk_device *device,
                    struct vk_object_base *base,
                    UNUSED VkObjectType obj_type)
{
   base->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
   base->type = obj_type;
   base->device = device;
   util_sparse_array_init(&base->private_data, sizeof(uint64_t), 8);
}

void
vk_object_base_finish(struct vk_object_base *base)
{
   util_sparse_array_finish(&base->private_data);
}

VkResult
vk_instance_init(struct vk_instance *instance,
                 const struct vk_instance_extension_table *supported_extensions,
                 const struct vk_instance_dispatch_table *dispatch_table,
                 const VkInstanceCreateInfo *pCreateInfo,
                 const VkAllocationCallbacks *alloc)
{
   memset(instance, 0, sizeof(*instance));
   vk_object_base_init(NULL, &instance->base, VK_OBJECT_TYPE_INSTANCE);
   instance->alloc = *alloc;

   instance->app_info = (struct vk_app_info) { .api_version = 0 };
   if (pCreateInfo->pApplicationInfo) {
      const VkApplicationInfo *app = pCreateInfo->pApplicationInfo;

      instance->app_info.app_name =
         vk_strdup(&instance->alloc, app->pApplicationName,
                   VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
      instance->app_info.app_version = app->applicationVersion;

      instance->app_info.engine_name =
         vk_strdup(&instance->alloc, app->pEngineName,
                   VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
      instance->app_info.engine_version = app->engineVersion;

      instance->app_info.api_version = app->apiVersion;
   }

   if (instance->app_info.api_version == 0)
      instance->app_info.api_version = VK_API_VERSION_1_0;

   if (supported_extensions != NULL) {
      for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; i++) {
         int idx;
         for (idx = 0; idx < VK_INSTANCE_EXTENSION_COUNT; idx++) {
            if (strcmp(pCreateInfo->ppEnabledExtensionNames[i],
                       vk_instance_extensions[idx].extensionName) == 0)
               break;
         }

         if (idx >= VK_INSTANCE_EXTENSION_COUNT)
            return VK_ERROR_EXTENSION_NOT_PRESENT;

         if (!supported_extensions->extensions[idx])
            return VK_ERROR_EXTENSION_NOT_PRESENT;

         instance->enabled_extensions.extensions[idx] = true;
      }
   }

   if (dispatch_table != NULL)
      instance->dispatch_table = *dispatch_table;

   return VK_SUCCESS;
}

void
vk_instance_finish(struct vk_instance *instance)
{
   vk_free(&instance->alloc, (char *)instance->app_info.app_name);
   vk_free(&instance->alloc, (char *)instance->app_info.engine_name);
   vk_object_base_finish(&instance->base);
}

VkResult
vk_physical_device_init(struct vk_physical_device *pdevice,
                        UNUSED struct vk_instance *instance,
                        const struct vk_device_extension_table *supported_extensions,
                        const struct vk_physical_device_dispatch_table *dispatch_table)
{
   memset(pdevice, 0, sizeof(*pdevice));
   vk_object_base_init(NULL, &pdevice->base, VK_OBJECT_TYPE_PHYSICAL_DEVICE);
   pdevice->instance = instance;

   if (supported_extensions != NULL)
      pdevice->supported_extensions = *supported_extensions;

   if (dispatch_table != NULL)
      pdevice->dispatch_table = *dispatch_table;

   return VK_SUCCESS;
}

void
vk_physical_device_finish(struct vk_physical_device *physical_device)
{
   vk_object_base_finish(&physical_device->base);
}

VkResult
vk_device_init(struct vk_device *device,
               struct vk_physical_device *physical_device,
               const struct vk_device_dispatch_table *dispatch_table,
               const VkDeviceCreateInfo *pCreateInfo,
               const VkAllocationCallbacks *instance_alloc,
               const VkAllocationCallbacks *device_alloc)
{
   memset(device, 0, sizeof(*device));
   vk_object_base_init(device, &device->base, VK_OBJECT_TYPE_DEVICE);
   if (device_alloc)
      device->alloc = *device_alloc;
   else
      device->alloc = *instance_alloc;

   device->physical = physical_device;

   if (dispatch_table != NULL)
      device->dispatch_table = *dispatch_table;

   if (physical_device != NULL) {
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

         device->enabled_extensions.extensions[idx] = true;
      }
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

void *
vk_object_alloc(struct vk_device *device,
                const VkAllocationCallbacks *alloc,
                size_t size,
                VkObjectType obj_type)
{
   void *ptr = vk_alloc2(&device->alloc, alloc, size, 8,
                         VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (ptr == NULL)
      return NULL;

   vk_object_base_init(device, (struct vk_object_base *)ptr, obj_type);

   return ptr;
}

void *
vk_object_zalloc(struct vk_device *device,
                const VkAllocationCallbacks *alloc,
                size_t size,
                VkObjectType obj_type)
{
   void *ptr = vk_zalloc2(&device->alloc, alloc, size, 8,
                         VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (ptr == NULL)
      return NULL;

   vk_object_base_init(device, (struct vk_object_base *)ptr, obj_type);

   return ptr;
}

void
vk_object_free(struct vk_device *device,
               const VkAllocationCallbacks *alloc,
               void *data)
{
   vk_object_base_finish((struct vk_object_base *)data);
   vk_free2(&device->alloc, alloc, data);
}

VkResult
vk_private_data_slot_create(struct vk_device *device,
                            const VkPrivateDataSlotCreateInfoEXT* pCreateInfo,
                            const VkAllocationCallbacks* pAllocator,
                            VkPrivateDataSlotEXT* pPrivateDataSlot)
{
   struct vk_private_data_slot *slot =
      vk_alloc2(&device->alloc, pAllocator, sizeof(*slot), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (slot == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   vk_object_base_init(device, &slot->base,
                       VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT);
   slot->index = p_atomic_inc_return(&device->private_data_next_index);

   *pPrivateDataSlot = vk_private_data_slot_to_handle(slot);

   return VK_SUCCESS;
}

void
vk_private_data_slot_destroy(struct vk_device *device,
                             VkPrivateDataSlotEXT privateDataSlot,
                             const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(vk_private_data_slot, slot, privateDataSlot);
   if (slot == NULL)
      return;

   vk_object_base_finish(&slot->base);
   vk_free2(&device->alloc, pAllocator, slot);
}

#ifdef ANDROID
static VkResult
get_swapchain_private_data_locked(struct vk_device *device,
                                  uint64_t objectHandle,
                                  struct vk_private_data_slot *slot,
                                  uint64_t **private_data)
{
   if (unlikely(device->swapchain_private == NULL)) {
      /* Even though VkSwapchain is a non-dispatchable object, we know a
       * priori that Android swapchains are actually pointers so we can use
       * the pointer hash table for them.
       */
      device->swapchain_private = _mesa_pointer_hash_table_create(NULL);
      if (device->swapchain_private == NULL)
         return VK_ERROR_OUT_OF_HOST_MEMORY;
   }

   struct hash_entry *entry =
      _mesa_hash_table_search(device->swapchain_private,
                              (void *)(uintptr_t)objectHandle);
   if (unlikely(entry == NULL)) {
      struct util_sparse_array *swapchain_private =
         ralloc(device->swapchain_private, struct util_sparse_array);
      util_sparse_array_init(swapchain_private, sizeof(uint64_t), 8);

      entry = _mesa_hash_table_insert(device->swapchain_private,
                                      (void *)(uintptr_t)objectHandle,
                                      swapchain_private);
      if (entry == NULL)
         return VK_ERROR_OUT_OF_HOST_MEMORY;
   }

   struct util_sparse_array *swapchain_private = entry->data;
   *private_data = util_sparse_array_get(swapchain_private, slot->index);

   return VK_SUCCESS;
}
#endif /* ANDROID */

static VkResult
vk_object_base_private_data(struct vk_device *device,
                            VkObjectType objectType,
                            uint64_t objectHandle,
                            VkPrivateDataSlotEXT privateDataSlot,
                            uint64_t **private_data)
{
   VK_FROM_HANDLE(vk_private_data_slot, slot, privateDataSlot);

#ifdef ANDROID
   /* There is an annoying spec corner here on Android.  Because WSI is
    * implemented in the Vulkan loader which doesn't know about the
    * VK_EXT_private_data extension, we have to handle VkSwapchainKHR in the
    * driver as a special case.  On future versions of Android where the
    * loader does understand VK_EXT_private_data, we'll never see a
    * vkGet/SetPrivateDataEXT call on a swapchain because the loader will
    * handle it.
    */
   if (objectType == VK_OBJECT_TYPE_SWAPCHAIN_KHR) {
      mtx_lock(&device->swapchain_private_mtx);
      VkResult result = get_swapchain_private_data_locked(device, objectHandle,
                                                          slot, private_data);
      mtx_unlock(&device->swapchain_private_mtx);
      return result;
   }
#endif /* ANDROID */

   struct vk_object_base *obj =
      vk_object_base_from_u64_handle(objectHandle, objectType);
   *private_data = util_sparse_array_get(&obj->private_data, slot->index);

   return VK_SUCCESS;
}

VkResult
vk_object_base_set_private_data(struct vk_device *device,
                                VkObjectType objectType,
                                uint64_t objectHandle,
                                VkPrivateDataSlotEXT privateDataSlot,
                                uint64_t data)
{
   uint64_t *private_data;
   VkResult result = vk_object_base_private_data(device,
                                                 objectType, objectHandle,
                                                 privateDataSlot,
                                                 &private_data);
   if (unlikely(result != VK_SUCCESS))
      return result;

   *private_data = data;
   return VK_SUCCESS;
}

void
vk_object_base_get_private_data(struct vk_device *device,
                                VkObjectType objectType,
                                uint64_t objectHandle,
                                VkPrivateDataSlotEXT privateDataSlot,
                                uint64_t *pData)
{
   uint64_t *private_data;
   VkResult result = vk_object_base_private_data(device,
                                                 objectType, objectHandle,
                                                 privateDataSlot,
                                                 &private_data);
   if (likely(result == VK_SUCCESS)) {
      *pData = *private_data;
   } else {
      *pData = 0;
   }
}
