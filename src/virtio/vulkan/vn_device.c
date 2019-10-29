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

#include "venus-protocol/vn_protocol_driver.h"

#include "vn_icd.h"
#include "vn_renderer.h"

/* require and request at least Vulkan 1.1 at both instance and device levels
 */
#define VN_MIN_RENDERER_VERSION VK_API_VERSION_1_1

static void
vn_cs_object_init(struct vn_cs_object *obj,
                  VkObjectType type,
                  struct vn_cs_device *dev)
{
   vk_object_base_init(&dev->base, &obj->base, type);
   assert(sizeof(obj->id) >= sizeof(obj));
   obj->id = (uintptr_t)obj;
}

static void
vn_cs_object_fini(struct vn_cs_object *obj)
{
   vk_object_base_finish(&obj->base);
}

static uint32_t
get_instance_api_version(const VkInstanceCreateInfo *create_info)
{
   return (create_info->pApplicationInfo &&
           create_info->pApplicationInfo->apiVersion)
             ? create_info->pApplicationInfo->apiVersion
             : VK_API_VERSION_1_0;
}

static int
get_instance_extension_index(const char *name)
{
   for (int i = 0; i < VN_INSTANCE_EXTENSION_COUNT; i++) {
      if (!strcmp(name, vn_instance_extensions[i].extensionName))
         return i;
   }
   return -1;
}

static VkResult
vn_instance_init_extensions(struct vn_instance *instance,
                            const char *const *names,
                            uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) {
      const int index = get_instance_extension_index(names[i]);
      if (index < 0 || !vn_instance_extensions_supported.extensions[index])
         return VK_ERROR_EXTENSION_NOT_PRESENT;
      instance->enabled_extensions.extensions[index] = true;
   }
   return VK_SUCCESS;
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

static VkResult
vn_instance_init_renderer(struct vn_instance *instance)
{
   const VkAllocationCallbacks *alloc = &instance->allocator;

   VkResult result = vn_renderer_create(instance, alloc, &instance->renderer);
   if (result != VK_SUCCESS)
      return result;

   vn_renderer_get_info(instance->renderer, &instance->renderer_info);

   uint32_t version = vn_info_wire_format_version();
   if (instance->renderer_info.wire_format_version != version) {
      if (VN_DEBUG(INIT)) {
         vn_log(instance, "wire format version %d != %d",
                instance->renderer_info.wire_format_version, version);
      }
      return VK_ERROR_INITIALIZATION_FAILED;
   }

   version = vn_info_vk_xml_version();
   if (instance->renderer_info.vk_xml_version > version)
      instance->renderer_info.vk_xml_version = version;

   version = vn_info_extension_spec_version("VK_EXT_command_serialization");
   if (instance->renderer_info.vk_ext_command_serialization_spec_version >
       version) {
      instance->renderer_info.vk_ext_command_serialization_spec_version =
         version;
   }

   version = vn_info_extension_spec_version("VK_MESA_venus_protocol");
   if (instance->renderer_info.vk_mesa_venus_protocol_spec_version >
       version) {
      instance->renderer_info.vk_mesa_venus_protocol_spec_version = version;
   }

   if (VN_DEBUG(INIT)) {
      vn_log(instance, "connected to renderer");
      vn_log(instance, "wire format version %d",
             instance->renderer_info.wire_format_version);
      vn_log(instance, "vk xml version %d.%d.%d",
             VK_VERSION_MAJOR(instance->renderer_info.vk_xml_version),
             VK_VERSION_MINOR(instance->renderer_info.vk_xml_version),
             VK_VERSION_PATCH(instance->renderer_info.vk_xml_version));
      vn_log(
         instance, "VK_EXT_command_serialization spec version %d",
         instance->renderer_info.vk_ext_command_serialization_spec_version);
      vn_log(instance, "VK_MESA_venus_protocol spec version %d",
             instance->renderer_info.vk_mesa_venus_protocol_spec_version);
   }

   /* reply bo will be allocated on demand by
    * vn_instance_get_cs_reply_bo_locked
    */
   result = vn_renderer_sync_create_cpu(instance->renderer, alloc,
                                        VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE,
                                        &instance->cs_reply.sync);
   if (result != VK_SUCCESS) {
      if (VN_DEBUG(INIT))
         vn_log(instance, "failed to create reply sync");
      return result;
   }

   vn_cs_init(&instance->cs, alloc, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE,
              16 * 1024);

   uint32_t renderer_version = 0;
   result = vn_call_vkEnumerateInstanceVersion(instance, &renderer_version);
   if (result != VK_SUCCESS) {
      if (VN_DEBUG(INIT))
         vn_log(instance, "failed to enumerate renderer instance version");
      return result;
   }

   if (renderer_version < VN_MIN_RENDERER_VERSION) {
      if (VN_DEBUG(INIT)) {
         vn_log(instance, "unsupported renderer instance version %d.%d",
                VK_VERSION_MAJOR(instance->renderer_version),
                VK_VERSION_MINOR(instance->renderer_version));
      }
      return VK_ERROR_INITIALIZATION_FAILED;
   }

   instance->renderer_version =
      instance->api_version > VN_MIN_RENDERER_VERSION
         ? instance->api_version
         : VN_MIN_RENDERER_VERSION;

   if (VN_DEBUG(INIT)) {
      vn_log(instance, "vk instance version %d.%d.%d",
             VK_VERSION_MAJOR(instance->renderer_version),
             VK_VERSION_MINOR(instance->renderer_version),
             VK_VERSION_PATCH(instance->renderer_version));
   }

   return VK_SUCCESS;
}

static bool
vn_instance_grow_cs_reply_bo_locked(struct vn_instance *instance, size_t size)
{
   const size_t min_bo_size = 1 << 20;
   const VkAllocationCallbacks *alloc = &instance->allocator;

   size_t bo_size =
      instance->cs_reply.size ? instance->cs_reply.size : min_bo_size;
   while (bo_size < size) {
      bo_size <<= 1;
      if (!bo_size)
         return false;
   }

   struct vn_renderer_bo *bo;
   VkResult result =
      vn_renderer_bo_create_cpu(instance->renderer, bo_size, alloc,
                                VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE, &bo);
   if (result != VK_SUCCESS)
      return false;

   void *ptr = vn_renderer_bo_map(bo);
   if (!ptr) {
      vn_renderer_bo_unref(bo, alloc);
      return false;
   }

   if (instance->cs_reply.bo)
      vn_renderer_bo_unref(instance->cs_reply.bo, alloc);
   instance->cs_reply.bo = bo;
   instance->cs_reply.size = bo_size;
   instance->cs_reply.used = 0;
   instance->cs_reply.ptr = ptr;

   return true;
}

struct vn_renderer_bo *
vn_instance_get_cs_reply_bo_locked(struct vn_instance *instance,
                                   size_t size,
                                   void **ptr)
{
   struct vn_cs *cs = &instance->cs;

   if (unlikely(instance->cs_reply.used + size > instance->cs_reply.size)) {
      if (!vn_instance_grow_cs_reply_bo_locked(instance, size))
         return NULL;

      const struct VkCommandStreamDescriptionMESA stream = {
         .resourceId = instance->cs_reply.bo->res_id,
         .size = instance->cs_reply.size,
      };
      const size_t cmd_size = vn_sizeof_vkSetReplyCommandStreamMESA(&stream);
      if (vn_cs_reserve_out(cs, cmd_size))
         vn_encode_vkSetReplyCommandStreamMESA(cs, 0, &stream);
   }

   /* TODO can we avoid this seek command? */
   const size_t offset = instance->cs_reply.used;
   const size_t cmd_size = vn_sizeof_vkSeekReplyCommandStreamMESA(offset);
   if (vn_cs_reserve_out(cs, cmd_size))
      vn_encode_vkSeekReplyCommandStreamMESA(cs, 0, offset);

   *ptr = instance->cs_reply.ptr + offset;
   instance->cs_reply.used += size;

   return vn_renderer_bo_ref(instance->cs_reply.bo);
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
         vk_outarray_append (&out, prop) {
            *prop = vn_instance_extensions[i];
         }
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

   vn_cs_object_init(&instance->base, VK_OBJECT_TYPE_INSTANCE, NULL);

   instance->allocator = *alloc;
   instance->api_version = get_instance_api_version(pCreateInfo);

   mtx_init(&instance->cs_mutex, mtx_plain);

   if (!vn_icd_supports_api_version(instance->api_version)) {
      result = VK_ERROR_INCOMPATIBLE_DRIVER;
      goto fail;
   }

   if (pCreateInfo->enabledLayerCount) {
      result = VK_ERROR_LAYER_NOT_PRESENT;
      goto fail;
   }

   result = vn_instance_init_extensions(instance,
                                        pCreateInfo->ppEnabledExtensionNames,
                                        pCreateInfo->enabledExtensionCount);
   if (result != VK_SUCCESS)
      goto fail;

   vn_instance_init_dispatch(instance);

   result = vn_instance_init_renderer(instance);
   if (result != VK_SUCCESS)
      goto fail;

   VkInstanceCreateInfo local_create_info;
   local_create_info = *pCreateInfo;
   local_create_info.ppEnabledExtensionNames = NULL;
   local_create_info.enabledExtensionCount = 0;
   pCreateInfo = &local_create_info;

   /* request at least instance->renderer_version */
   VkApplicationInfo local_app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .apiVersion = instance->renderer_version,
   };
   if (instance->api_version < instance->renderer_version) {
      if (pCreateInfo->pApplicationInfo) {
         local_app_info = *pCreateInfo->pApplicationInfo;
         local_app_info.apiVersion = instance->renderer_version;
      }
      local_create_info.pApplicationInfo = &local_app_info;
   }

   VkInstance instance_handle = vn_instance_to_handle(instance);
   result =
      vn_call_vkCreateInstance(instance, pCreateInfo, NULL, &instance_handle);
   if (result != VK_SUCCESS)
      return result;

   *pInstance = instance_handle;

   return VK_SUCCESS;

fail:
   if (instance->cs_reply.bo)
      vn_renderer_bo_unref(instance->cs_reply.bo, alloc);
   if (instance->cs_reply.sync)
      vn_renderer_sync_destroy(instance->cs_reply.sync, alloc);

   if (instance->renderer) {
      vn_renderer_destroy(instance->renderer, alloc);
      vn_cs_fini(&instance->cs);
   }

   mtx_destroy(&instance->cs_mutex);
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

   vn_call_vkDestroyInstance(instance, _instance, NULL);

   vn_renderer_bo_unref(instance->cs_reply.bo, alloc);
   vn_renderer_sync_destroy(instance->cs_reply.sync, alloc);

   vn_renderer_destroy(instance->renderer, alloc);
   vn_cs_fini(&instance->cs);
   mtx_destroy(&instance->cs_mutex);

   vn_cs_object_fini(&instance->base);
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
