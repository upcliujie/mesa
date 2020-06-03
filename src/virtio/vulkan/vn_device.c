/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 */

#include "vn_device.h"

#include "vn_icd.h"

static uint32_t
get_instance_api_version(const VkInstanceCreateInfo *create_info)
{
   return (create_info->pApplicationInfo &&
           create_info->pApplicationInfo->apiVersion)
             ? create_info->pApplicationInfo->apiVersion
             : VK_API_VERSION_1_0;
}

static void
vn_instance_init_dispatch(struct vn_instance *instance)
{
   uint32_t count = ARRAY_SIZE(vn_instance_dispatch_table.entrypoints);
   void *const *from = vn_instance_dispatch_table.entrypoints;
   void **to = instance->dispatch.entrypoints;
   for (uint32_t i = 0; i < count; i++) {
      to[i] = vn_instance_entrypoint_is_enabled(i, instance->api_version,
                                                &instance->enabled_extensions)
                 ? from[i]
                 : NULL;
   }

   count = ARRAY_SIZE(vn_physical_device_dispatch_table.entrypoints);
   from = vn_physical_device_dispatch_table.entrypoints;
   to = instance->physical_device_dispatch.entrypoints;
   for (uint32_t i = 0; i < count; i++) {
      to[i] = vn_physical_device_entrypoint_is_enabled(
                 i, instance->api_version, &instance->enabled_extensions)
                 ? from[i]
                 : NULL;
   }

   count = ARRAY_SIZE(vn_device_dispatch_table.entrypoints);
   from = vn_device_dispatch_table.entrypoints;
   to = instance->device_dispatch.entrypoints;
   for (uint32_t i = 0; i < count; i++) {
      to[i] =
         vn_device_entrypoint_is_enabled(i, instance->api_version,
                                         &instance->enabled_extensions, NULL)
            ? from[i]
            : NULL;
   }
}

static PFN_vkVoidFunction
vn_instance_get_dispatch(struct vn_instance *instance, const char *name)
{
   int idx = vn_get_instance_entrypoint_index(name);
   if (idx >= 0)
      return instance->dispatch.entrypoints[idx];

   idx = vn_get_physical_device_entrypoint_index(name);
   if (idx >= 0)
      return instance->physical_device_dispatch.entrypoints[idx];

   idx = vn_get_device_entrypoint_index(name);
   if (idx >= 0)
      return instance->device_dispatch.entrypoints[idx];

   return NULL;
}

/* instance commands */

/* vn_EnumerateInstanceVersion is generated */

VkResult
vn_EnumerateInstanceExtensionProperties(const char *pLayerName,
                                        uint32_t *pPropertyCount,
                                        VkExtensionProperties *pProperties)
{
   if (pLayerName)
      return vn_error(NULL, VK_ERROR_LAYER_NOT_PRESENT);

   /*
    * Instance extensions add instance-level or physical-device-level
    * functionalities.  Currently, there are
    *
    *  - VK_KHR_surface and related extensions
    *  - VK_KHR_display and related extensions
    *  - VK_EXT_debug_{report,utils}
    *  - VK_EXT_validation_{flags,features}
    *  - promoted to core
    *    - VK_KHR_get_physical_device_properties2
    *    - VK_KHR_device_group_creation
    *    - VK_KHR_external_{memory,semaphore,fence}_capabilities
    *
    * It seems renderer support is either unnecessary or optional.  We should
    * be able to advertise them or lie about them locally.
    */
   VK_OUTARRAY_MAKE(out, pProperties, pPropertyCount);
   for (uint32_t i = 0; i < VN_INSTANCE_EXTENSION_COUNT; i++) {
      if (vn_instance_extensions_supported.extensions[i]) {
         vk_outarray_append(&out, prop) { *prop = vn_instance_extensions[i]; }
      }
   }

   return vk_outarray_status(&out);
}

VkResult
vn_EnumerateInstanceLayerProperties(uint32_t *pPropertyCount,
                                    VkLayerProperties *pProperties)
{
   *pPropertyCount = 0;
   return VK_SUCCESS;
}

VkResult
vn_CreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                  const VkAllocationCallbacks *pAllocator,
                  VkInstance *pInstance)
{
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &vn_default_allocator;
   struct vn_instance *instance;
   VkResult result;

   vn_debug_init();

   instance = vk_zalloc(alloc, sizeof(*instance), VN_DEFAULT_ALIGN,
                        VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!instance)
      return vn_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_object_base_init(NULL, &instance->base, VK_OBJECT_TYPE_INSTANCE);
   instance->allocator = *alloc;
   instance->api_version = get_instance_api_version(pCreateInfo);

   if (!vn_icd_supports_api_version(instance->api_version)) {
      result = VK_ERROR_INCOMPATIBLE_DRIVER;
      goto fail;
   }

   if (pCreateInfo->enabledLayerCount) {
      result = VK_ERROR_LAYER_NOT_PRESENT;
      goto fail;
   }

   if (pCreateInfo->enabledExtensionCount) {
      result = VK_ERROR_EXTENSION_NOT_PRESENT;
      goto fail;
   }

   vn_instance_init_dispatch(instance);

   *pInstance = vn_instance_to_handle(instance);
   return VK_SUCCESS;

fail:
   vk_free(alloc, instance);
   return vn_error(NULL, result);
}

void
vn_DestroyInstance(VkInstance _instance,
                   const VkAllocationCallbacks *pAllocator)
{
   struct vn_instance *instance = vn_instance_from_handle(_instance);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &instance->allocator;

   if (!instance)
      return;

   vk_object_base_finish(&instance->base);
   vk_free(alloc, instance);
}

PFN_vkVoidFunction
vn_GetInstanceProcAddr(VkInstance _instance, const char *pName)
{
   static const struct {
      const char *name;
      PFN_vkVoidFunction command;
   } instance_commands[] = {
      { "vkGetInstanceProcAddr", (PFN_vkVoidFunction)vn_GetInstanceProcAddr },
      { "vkEnumerateInstanceVersion",
        (PFN_vkVoidFunction)vn_EnumerateInstanceVersion },
      { "vkEnumerateInstanceExtensionProperties",
        (PFN_vkVoidFunction)vn_EnumerateInstanceExtensionProperties },
      { "vkEnumerateInstanceLayerProperties",
        (PFN_vkVoidFunction)vn_EnumerateInstanceLayerProperties },
      { "vkCreateInstance", (PFN_vkVoidFunction)vn_CreateInstance },
   };
   struct vn_instance *instance = vn_instance_from_handle(_instance);

   assert(pName);
   for (uint32_t i = 0; i < ARRAY_SIZE(instance_commands); i++) {
      if (!strcmp(instance_commands[i].name, pName))
         return instance_commands[i].command;
   }

   if (!instance)
      return NULL;

   return vn_instance_get_dispatch(instance, pName);
}

/* physical device commands */

VkResult
vn_EnumeratePhysicalDevices(VkInstance _instance,
                            uint32_t *pPhysicalDeviceCount,
                            VkPhysicalDevice *pPhysicalDevices)
{
   struct vn_instance *instance = vn_instance_from_handle(_instance);

   return vn_error(instance, VK_ERROR_INCOMPATIBLE_DRIVER);
}

void
vn_GetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice,
                             VkPhysicalDeviceFeatures *pFeatures)
{
}

void
vn_GetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                               VkPhysicalDeviceProperties *pProperties)
{
}

void
vn_GetPhysicalDeviceQueueFamilyProperties(
   VkPhysicalDevice physicalDevice,
   uint32_t *pQueueFamilyPropertyCount,
   VkQueueFamilyProperties *pQueueFamilyProperties)
{
}

void
vn_GetPhysicalDeviceMemoryProperties(
   VkPhysicalDevice physicalDevice,
   VkPhysicalDeviceMemoryProperties *pMemoryProperties)
{
}

void
vn_GetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice,
                                     VkFormat format,
                                     VkFormatProperties *pFormatProperties)
{
}

VkResult
vn_GetPhysicalDeviceImageFormatProperties(
   VkPhysicalDevice physicalDevice,
   VkFormat format,
   VkImageType type,
   VkImageTiling tiling,
   VkImageUsageFlags usage,
   VkImageCreateFlags flags,
   VkImageFormatProperties *pImageFormatProperties)
{
   return vn_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);
}

void
vn_GetPhysicalDeviceSparseImageFormatProperties(
   VkPhysicalDevice physicalDevice,
   VkFormat format,
   VkImageType type,
   uint32_t samples,
   VkImageUsageFlags usage,
   VkImageTiling tiling,
   uint32_t *pPropertyCount,
   VkSparseImageFormatProperties *pProperties)
{
}

/* device commands */

VkResult
vn_EnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                      const char *pLayerName,
                                      uint32_t *pPropertyCount,
                                      VkExtensionProperties *pProperties)
{
   return vn_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);
}

VkResult
vn_CreateDevice(VkPhysicalDevice physicalDevice,
                const VkDeviceCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkDevice *pDevice)
{
   return vn_error(NULL, VK_ERROR_INCOMPATIBLE_DRIVER);
}

PFN_vkVoidFunction
vn_GetDeviceProcAddr(VkDevice device, const char *pName)
{
   return NULL;
}
