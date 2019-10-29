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

#include <stdio.h>

#include "git_sha1.h"
#include "util/mesa-sha1.h"
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

static struct vn_physical_device *
vn_instance_find_physical_device(struct vn_instance *instance,
                                 vn_cs_object_id id)
{
   for (uint32_t i = 0; i < instance->physical_device_count; i++) {
      if (instance->physical_devices[i].base.id == id)
         return &instance->physical_devices[i];
   }
   return NULL;
}

static void
vn_physical_device_init_features(struct vn_physical_device *physical_dev)
{
   struct vn_instance *instance = physical_dev->instance;
   struct {
      /* Vulkan 1.1 */
      VkPhysicalDevice16BitStorageFeatures sixteen_bit_storage;
      VkPhysicalDeviceMultiviewFeatures multiview;
      VkPhysicalDeviceVariablePointersFeatures variable_pointers;
      VkPhysicalDeviceProtectedMemoryFeatures protected_memory;
      VkPhysicalDeviceSamplerYcbcrConversionFeatures sampler_ycbcr_conversion;
      VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters;
   } local_feats;

   physical_dev->features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   if (physical_dev->renderer_version >= VK_API_VERSION_1_2) {
      physical_dev->features.pNext = &physical_dev->vulkan_1_1_features;

      physical_dev->vulkan_1_1_features.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
      physical_dev->vulkan_1_1_features.pNext =
         &physical_dev->vulkan_1_2_features;
      physical_dev->vulkan_1_2_features.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
      physical_dev->vulkan_1_2_features.pNext = NULL;
   } else {
      physical_dev->features.pNext = &local_feats.sixteen_bit_storage;

      local_feats.sixteen_bit_storage.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
      local_feats.sixteen_bit_storage.pNext = &local_feats.multiview;
      local_feats.multiview.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
      local_feats.multiview.pNext = &local_feats.variable_pointers;
      local_feats.variable_pointers.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES;
      local_feats.variable_pointers.pNext = &local_feats.protected_memory;
      local_feats.protected_memory.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES;
      local_feats.protected_memory.pNext =
         &local_feats.sampler_ycbcr_conversion;
      local_feats.sampler_ycbcr_conversion.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
      local_feats.sampler_ycbcr_conversion.pNext =
         &local_feats.shader_draw_parameters;
      local_feats.shader_draw_parameters.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
      local_feats.shader_draw_parameters.pNext = NULL;
   }

   vn_call_vkGetPhysicalDeviceFeatures2(
      instance, vn_physical_device_to_handle(physical_dev),
      &physical_dev->features);

   struct VkPhysicalDeviceVulkan11Features *vk11_feats =
      &physical_dev->vulkan_1_1_features;

   if (physical_dev->renderer_version < VK_API_VERSION_1_2) {
      vk11_feats->storageBuffer16BitAccess =
         local_feats.sixteen_bit_storage.storageBuffer16BitAccess;
      vk11_feats->uniformAndStorageBuffer16BitAccess =
         local_feats.sixteen_bit_storage.uniformAndStorageBuffer16BitAccess;
      vk11_feats->storagePushConstant16 =
         local_feats.sixteen_bit_storage.storagePushConstant16;
      vk11_feats->storageInputOutput16 =
         local_feats.sixteen_bit_storage.storageInputOutput16;

      vk11_feats->multiview = local_feats.multiview.multiview;
      vk11_feats->multiviewGeometryShader =
         local_feats.multiview.multiviewGeometryShader;
      vk11_feats->multiviewTessellationShader =
         local_feats.multiview.multiviewTessellationShader;

      vk11_feats->variablePointersStorageBuffer =
         local_feats.variable_pointers.variablePointersStorageBuffer;
      vk11_feats->variablePointers =
         local_feats.variable_pointers.variablePointers;

      vk11_feats->protectedMemory =
         local_feats.protected_memory.protectedMemory;

      vk11_feats->samplerYcbcrConversion =
         local_feats.sampler_ycbcr_conversion.samplerYcbcrConversion;

      vk11_feats->shaderDrawParameters =
         local_feats.shader_draw_parameters.shaderDrawParameters;
   }
}

static void
vn_physical_device_init_uuids(struct vn_physical_device *physical_dev)
{
   struct VkPhysicalDeviceProperties *props =
      &physical_dev->properties.properties;
   struct VkPhysicalDeviceVulkan11Properties *vk11_props =
      &physical_dev->vulkan_1_1_properties;
   struct VkPhysicalDeviceVulkan12Properties *vk12_props =
      &physical_dev->vulkan_1_2_properties;
   struct mesa_sha1 sha1_ctx;
   uint8_t sha1[SHA1_DIGEST_LENGTH];

   static_assert(VK_UUID_SIZE <= SHA1_DIGEST_LENGTH, "");

   /* keep props->pipelineCacheUUID? */

   _mesa_sha1_init(&sha1_ctx);
   _mesa_sha1_update(&sha1_ctx, &props->vendorID, sizeof(props->vendorID));
   _mesa_sha1_update(&sha1_ctx, &props->deviceID, sizeof(props->deviceID));
   _mesa_sha1_final(&sha1_ctx, sha1);

   memcpy(vk11_props->deviceUUID, sha1, VK_UUID_SIZE);

   _mesa_sha1_init(&sha1_ctx);
   _mesa_sha1_update(&sha1_ctx, vk12_props->driverName,
                     strlen(vk12_props->driverName));
   _mesa_sha1_update(&sha1_ctx, vk12_props->driverInfo,
                     strlen(vk12_props->driverInfo));
   _mesa_sha1_final(&sha1_ctx, sha1);

   memcpy(vk11_props->driverUUID, sha1, VK_UUID_SIZE);

   memset(vk11_props->deviceLUID, 0, VK_LUID_SIZE);
   vk11_props->deviceNodeMask = 0;
   vk11_props->deviceLUIDValid = false;
}

static void
vn_physical_device_init_properties(struct vn_physical_device *physical_dev)
{
   struct vn_instance *instance = physical_dev->instance;
   struct {
      /* Vulkan 1.1 */
      VkPhysicalDeviceIDProperties id;
      VkPhysicalDeviceSubgroupProperties subgroup;
      VkPhysicalDevicePointClippingProperties point_clipping;
      VkPhysicalDeviceMultiviewProperties multiview;
      VkPhysicalDeviceProtectedMemoryProperties protected_memory;
      VkPhysicalDeviceMaintenance3Properties maintenance_3;
   } local_props;

   physical_dev->properties.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
   if (physical_dev->renderer_version >= VK_API_VERSION_1_2) {
      physical_dev->properties.pNext = &physical_dev->vulkan_1_1_properties;

      physical_dev->vulkan_1_1_properties.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
      physical_dev->vulkan_1_1_properties.pNext =
         &physical_dev->vulkan_1_2_properties;
      physical_dev->vulkan_1_2_properties.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
      physical_dev->vulkan_1_2_properties.pNext = NULL;
   } else {
      physical_dev->properties.pNext = &local_props.id;

      local_props.id.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
      local_props.id.pNext = &local_props.subgroup;
      local_props.subgroup.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
      local_props.subgroup.pNext = &local_props.point_clipping;
      local_props.point_clipping.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES;
      local_props.point_clipping.pNext = &local_props.multiview;
      local_props.multiview.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
      local_props.multiview.pNext = &local_props.protected_memory;
      local_props.protected_memory.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES;
      local_props.protected_memory.pNext = &local_props.maintenance_3;
      local_props.maintenance_3.sType =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
      local_props.maintenance_3.pNext = NULL;
   }

   vn_call_vkGetPhysicalDeviceProperties2(
      instance, vn_physical_device_to_handle(physical_dev),
      &physical_dev->properties);

   struct VkPhysicalDeviceProperties *props =
      &physical_dev->properties.properties;
   struct VkPhysicalDeviceVulkan11Properties *vk11_props =
      &physical_dev->vulkan_1_1_properties;
   struct VkPhysicalDeviceVulkan12Properties *vk12_props =
      &physical_dev->vulkan_1_2_properties;

   if (physical_dev->renderer_version < VK_API_VERSION_1_2) {
      memcpy(vk11_props->deviceUUID, local_props.id.deviceUUID,
             sizeof(vk11_props->deviceUUID));
      memcpy(vk11_props->driverUUID, local_props.id.driverUUID,
             sizeof(vk11_props->driverUUID));
      memcpy(vk11_props->deviceLUID, local_props.id.deviceLUID,
             sizeof(vk11_props->deviceLUID));
      vk11_props->deviceNodeMask = local_props.id.deviceNodeMask;
      vk11_props->deviceLUIDValid = local_props.id.deviceLUIDValid;

      vk11_props->subgroupSize = local_props.subgroup.subgroupSize;
      vk11_props->subgroupSupportedStages =
         local_props.subgroup.supportedStages;
      vk11_props->subgroupSupportedOperations =
         local_props.subgroup.supportedOperations;
      vk11_props->subgroupQuadOperationsInAllStages =
         local_props.subgroup.quadOperationsInAllStages;

      vk11_props->pointClippingBehavior =
         local_props.point_clipping.pointClippingBehavior;

      vk11_props->maxMultiviewViewCount =
         local_props.multiview.maxMultiviewViewCount;
      vk11_props->maxMultiviewInstanceIndex =
         local_props.multiview.maxMultiviewInstanceIndex;

      vk11_props->protectedNoFault =
         local_props.protected_memory.protectedNoFault;

      vk11_props->maxPerSetDescriptors =
         local_props.maintenance_3.maxPerSetDescriptors;
      vk11_props->maxMemoryAllocationSize =
         local_props.maintenance_3.maxMemoryAllocationSize;
   }

   const uint32_t max_api_version =
      vn_physical_device_api_version(physical_dev);
   if (props->apiVersion > max_api_version)
      props->apiVersion = max_api_version;

   props->driverVersion = vk_get_driver_version();
   props->vendorID = instance->renderer_info.pci.vendor_id;
   props->deviceID = instance->renderer_info.pci.device_id;
   /* some apps don't like VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU */
   props->deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
   snprintf(props->deviceName, sizeof(props->deviceName), "Virtio GPU");

   vk12_props->driverID = 0;
   snprintf(vk12_props->driverName, sizeof(vk12_props->driverName), "venus");
   snprintf(vk12_props->driverInfo, sizeof(vk12_props->driverInfo),
            "Mesa " PACKAGE_VERSION MESA_GIT_SHA1);
   vk12_props->conformanceVersion = (VkConformanceVersionKHR){
      .major = 0,
      .minor = 0,
      .subminor = 0,
      .patch = 0,
   };

   vn_physical_device_init_uuids(physical_dev);
}

static VkResult
vn_physical_device_init_queue_family_properties(
   struct vn_physical_device *physical_dev)
{
   struct vn_instance *instance = physical_dev->instance;
   const VkAllocationCallbacks *alloc = &instance->allocator;
   uint32_t count;

   vn_call_vkGetPhysicalDeviceQueueFamilyProperties2(
      instance, vn_physical_device_to_handle(physical_dev), &count, NULL);

   uint32_t *sync_queue_bases;
   VkQueueFamilyProperties2 *props =
      vk_alloc(alloc, (sizeof(*props) + sizeof(*sync_queue_bases)) * count,
               VN_DEFAULT_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!props)
      return VK_ERROR_OUT_OF_HOST_MEMORY;
   sync_queue_bases = (uint32_t *)&props[count];

   for (uint32_t i = 0; i < count; i++) {
      props[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
      /* define an extension to query sync queue base? */
      props[i].pNext = NULL;
   }
   vn_call_vkGetPhysicalDeviceQueueFamilyProperties2(
      instance, vn_physical_device_to_handle(physical_dev), &count, props);

   physical_dev->queue_family_properties = props;
   /* sync_queue_bases will be initialized later */
   physical_dev->queue_family_sync_queue_bases = sync_queue_bases;
   physical_dev->queue_family_count = count;

   return VK_SUCCESS;
}

static void
vn_physical_device_init_memory_properties(
   struct vn_physical_device *physical_dev)
{
   struct vn_instance *instance = physical_dev->instance;

   physical_dev->memory_properties.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;

   vn_call_vkGetPhysicalDeviceMemoryProperties2(
      instance, vn_physical_device_to_handle(physical_dev),
      &physical_dev->memory_properties);

   if (!instance->renderer_info.has_cache_management) {
      VkPhysicalDeviceMemoryProperties *props =
         &physical_dev->memory_properties.memoryProperties;
      const uint32_t host_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                  VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

      for (uint32_t i = 0; i < props->memoryTypeCount; i++) {
         const bool coherent = props->memoryTypes[i].propertyFlags &
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
         if (!coherent)
            props->memoryTypes[i].propertyFlags &= ~host_flags;
      }
   }
}

static VkResult
vn_physical_device_init_extensions(struct vn_physical_device *physical_dev)
{
   struct vn_instance *instance = physical_dev->instance;
   const VkAllocationCallbacks *alloc = &instance->allocator;

   /* get renderer extensions */
   uint32_t count;
   VkResult result = vn_call_vkEnumerateDeviceExtensionProperties(
      instance, vn_physical_device_to_handle(physical_dev), NULL, &count,
      NULL);
   if (result != VK_SUCCESS)
      return result;

   VkExtensionProperties *exts = NULL;
   if (count) {
      exts = vk_alloc(alloc, sizeof(*exts) * count, VN_DEFAULT_ALIGN,
                      VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
      if (!exts)
         return VK_ERROR_OUT_OF_HOST_MEMORY;

      result = vn_call_vkEnumerateDeviceExtensionProperties(
         instance, vn_physical_device_to_handle(physical_dev), NULL, &count,
         exts);
      if (result < VK_SUCCESS) {
         vk_free(alloc, exts);
         return result;
      }
   }

   struct vn_device_extension_table supported;
   vn_physical_device_get_supported_extensions(physical_dev, &supported);

   physical_dev->extension_spec_versions =
      vk_zalloc(alloc,
                sizeof(*physical_dev->extension_spec_versions) *
                   VN_DEVICE_EXTENSION_COUNT,
                VN_DEFAULT_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!physical_dev->extension_spec_versions) {
      vk_free(alloc, exts);
      return VK_ERROR_OUT_OF_HOST_MEMORY;
   }

   for (uint32_t i = 0; i < VN_DEVICE_EXTENSION_COUNT; i++) {
      const VkExtensionProperties *props = &vn_device_extensions[i];
      const VkExtensionProperties *renderer_props = NULL;

      for (uint32_t j = 0; j < count; j++) {
         if (!strcmp(props->extensionName, exts[j].extensionName)) {
            physical_dev->renderer_extensions.extensions[i] = true;
            renderer_props = &exts[j];
            break;
         }
      }

      /* no driver support */
      if (!supported.extensions[i])
         continue;

      /* does not depend on renderer (e.g., WSI) */
      if (props->specVersion) {
         physical_dev->supported_extensions.extensions[i] = true;
         continue;
      }

      /* check renderer support */
      if (!renderer_props)
         continue;

      /* check encoder support */
      const uint32_t spec_version =
         vn_info_extension_spec_version(props->extensionName);
      if (!spec_version)
         continue;

      physical_dev->supported_extensions.extensions[i] = true;
      physical_dev->extension_spec_versions[i] =
         MIN2(renderer_props->specVersion, spec_version);
   }

   vk_free(alloc, exts);

   return VK_SUCCESS;
}

static VkResult
vn_physical_device_init_version(struct vn_physical_device *physical_dev)
{
   struct vn_instance *instance = physical_dev->instance;

   /*
    * We either check and enable VK_KHR_get_physical_device_properties2, or we
    * must use vkGetPhysicalDeviceProperties to get the device-level version.
    */
   VkPhysicalDeviceProperties props;
   vn_call_vkGetPhysicalDeviceProperties(
      instance, vn_physical_device_to_handle(physical_dev), &props);
   if (props.apiVersion < VN_MIN_RENDERER_VERSION) {
      if (VN_DEBUG(INIT)) {
         vn_log(instance, "unsupported renderer device version %d.%d",
                VK_VERSION_MAJOR(props.apiVersion),
                VK_VERSION_MINOR(props.apiVersion));
      }
      return VK_ERROR_INITIALIZATION_FAILED;
   }

   physical_dev->renderer_version = props.apiVersion;
   if (physical_dev->renderer_version > instance->renderer_version)
      physical_dev->renderer_version = instance->renderer_version;

   return VK_SUCCESS;
}

static VkResult
vn_physical_device_init(struct vn_physical_device *physical_dev)
{
   struct vn_instance *instance = physical_dev->instance;
   const VkAllocationCallbacks *alloc = &instance->allocator;

   VkResult result = vn_physical_device_init_version(physical_dev);
   if (result != VK_SUCCESS)
      return result;

   result = vn_physical_device_init_extensions(physical_dev);
   if (result != VK_SUCCESS)
      return result;

   /* TODO query all caps with minimal round trips */
   vn_physical_device_init_features(physical_dev);
   vn_physical_device_init_properties(physical_dev);

   result = vn_physical_device_init_queue_family_properties(physical_dev);
   if (result != VK_SUCCESS)
      goto fail;

   vn_physical_device_init_memory_properties(physical_dev);

   return VK_SUCCESS;

fail:
   vk_free(alloc, physical_dev->extension_spec_versions);
   vk_free(alloc, physical_dev->queue_family_properties);
   return result;
}

static void
vn_physical_device_fini(struct vn_physical_device *physical_dev)
{
   struct vn_instance *instance = physical_dev->instance;
   const VkAllocationCallbacks *alloc = &instance->allocator;

   vk_free(alloc, physical_dev->extension_spec_versions);
   vk_free(alloc, physical_dev->queue_family_properties);

   vn_cs_object_fini(&physical_dev->base);
}

static VkResult
vn_instance_enumerate_physical_devices(struct vn_instance *instance)
{
   const VkAllocationCallbacks *alloc = &instance->allocator;
   VkResult result;

   mtx_lock(&instance->physical_device_mutex);

   if (instance->physical_devices) {
      result = VK_SUCCESS;
      goto out;
   }

   uint32_t count;
   result = vn_call_vkEnumeratePhysicalDevices(
      instance, vn_instance_to_handle(instance), &count, NULL);
   if (result != VK_SUCCESS || !count)
      goto out;

   struct vn_physical_device *physical_devs =
      vk_zalloc(alloc, sizeof(*physical_devs) * count, VN_DEFAULT_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!physical_devs) {
      result = VK_ERROR_OUT_OF_HOST_MEMORY;
      goto out;
   }

   VkPhysicalDevice *handles =
      vk_alloc(alloc, sizeof(*handles) * count, VN_DEFAULT_ALIGN,
               VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   if (!handles) {
      vk_free(alloc, physical_devs);
      result = VK_ERROR_OUT_OF_HOST_MEMORY;
      goto out;
   }

   for (uint32_t i = 0; i < count; i++) {
      struct vn_physical_device *physical_dev = &physical_devs[i];

      vn_cs_object_init(&physical_dev->base, VK_OBJECT_TYPE_PHYSICAL_DEVICE,
                        NULL);
      physical_dev->instance = instance;

      handles[i] = vn_physical_device_to_handle(physical_dev);
   }

   result = vn_call_vkEnumeratePhysicalDevices(
      instance, vn_instance_to_handle(instance), &count, handles);
   vk_free(alloc, handles);

   if (result != VK_SUCCESS) {
      vk_free(alloc, physical_devs);
      goto out;
   }

   uint32_t sync_queue_base = 0;
   uint32_t i = 0;
   while (i < count) {
      struct vn_physical_device *physical_dev = &physical_devs[i];

      result = vn_physical_device_init(physical_dev);
      if (result == VK_SUCCESS) {
         /* TODO assign sync queues more fairly */
         for (uint32_t j = 0; j < physical_dev->queue_family_count; j++) {
            const VkQueueFamilyProperties *props =
               &physical_dev->queue_family_properties[j].queueFamilyProperties;

            if (sync_queue_base + props->queueCount >
                instance->renderer_info.max_sync_queue_count) {
               if (VN_DEBUG(INIT)) {
                  vn_log(instance, "not enough sync queues (max %d)",
                         instance->renderer_info.max_sync_queue_count);
               }
               result = VK_ERROR_INITIALIZATION_FAILED;
               break;
            }

            physical_dev->queue_family_sync_queue_bases[j] = sync_queue_base;
            sync_queue_base += props->queueCount;
         }
      }

      if (result != VK_SUCCESS) {
         memmove(&physical_devs[i], &physical_devs[i + 1],
                 sizeof(*physical_devs) * (count - i - 1));
         count--;
         continue;
      }

      i++;
   }

   instance->physical_devices = physical_devs;
   instance->physical_device_count = count;

out:
   mtx_unlock(&instance->physical_device_mutex);
   return result;
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
   mtx_init(&instance->physical_device_mutex, mtx_plain);

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
   mtx_destroy(&instance->physical_device_mutex);
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

   if (instance->physical_devices) {
      for (uint32_t i = 0; i < instance->physical_device_count; i++)
         vn_physical_device_fini(&instance->physical_devices[i]);
      vk_free(alloc, instance->physical_devices);
   }

   vn_call_vkDestroyInstance(instance, _instance, NULL);

   vn_renderer_bo_unref(instance->cs_reply.bo, alloc);
   vn_renderer_sync_destroy(instance->cs_reply.sync, alloc);

   vn_renderer_destroy(instance->renderer, alloc);
   vn_cs_fini(&instance->cs);
   mtx_destroy(&instance->cs_mutex);
   mtx_destroy(&instance->physical_device_mutex);

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

   VkResult result = vn_instance_enumerate_physical_devices(instance);
   if (result != VK_SUCCESS)
      return vn_error(instance, result);

   VK_OUTARRAY_MAKE(out, pPhysicalDevices, pPhysicalDeviceCount);
   for (uint32_t i = 0; i < instance->physical_device_count; i++) {
      vk_outarray_append (&out, physical_dev) {
         *physical_dev =
            vn_physical_device_to_handle(&instance->physical_devices[i]);
      }
   }

   return vk_outarray_status(&out);
}

VkResult
vn_EnumeratePhysicalDeviceGroups(
   VkInstance _instance,
   uint32_t *pPhysicalDeviceGroupCount,
   VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties)
{
   struct vn_instance *instance = vn_instance_from_handle(_instance);
   const VkAllocationCallbacks *alloc = &instance->allocator;
   struct vn_cs_object *dummy = NULL;
   VkResult result;

   result = vn_instance_enumerate_physical_devices(instance);
   if (result != VK_SUCCESS)
      return vn_error(instance, result);

   /* make sure VkPhysicalDevice point to objects, as they are considered
    * inputs by the encoder
    */
   if (pPhysicalDeviceGroupProperties) {
      const uint32_t count = *pPhysicalDeviceGroupCount;
      const size_t size = sizeof(*dummy) * VK_MAX_DEVICE_GROUP_SIZE * count;

      dummy = vk_zalloc(alloc, size, VN_DEFAULT_ALIGN,
                        VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
      if (!dummy)
         return vn_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);

      for (uint32_t i = 0; i < count; i++) {
         VkPhysicalDeviceGroupProperties *props =
            &pPhysicalDeviceGroupProperties[i];

         for (uint32_t j = 0; j < VK_MAX_DEVICE_GROUP_SIZE; j++) {
            props->physicalDevices[j] =
               (VkPhysicalDevice)&dummy[VK_MAX_DEVICE_GROUP_SIZE * i + j];
         }
      }
   }

   result = vn_call_vkEnumeratePhysicalDeviceGroups(
      instance, vn_instance_to_handle(instance), pPhysicalDeviceGroupCount,
      pPhysicalDeviceGroupProperties);
   if (result != VK_SUCCESS) {
      if (dummy)
         vk_free(alloc, dummy);
      return vn_error(instance, result);
   }

   if (pPhysicalDeviceGroupProperties) {
      for (uint32_t i = 0; i < *pPhysicalDeviceGroupCount; i++) {
         VkPhysicalDeviceGroupProperties *props =
            &pPhysicalDeviceGroupProperties[i];
         for (uint32_t j = 0; j < props->physicalDeviceCount; j++) {
            const vn_cs_object_id id =
               dummy[VK_MAX_DEVICE_GROUP_SIZE * i + j].id;
            struct vn_physical_device *physical_dev =
               vn_instance_find_physical_device(instance, id);
            props->physicalDevices[j] =
               vn_physical_device_to_handle(physical_dev);
         }
      }
   }

   if (dummy)
      vk_free(alloc, dummy);

   return VK_SUCCESS;
}

void
vn_GetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice,
                             VkPhysicalDeviceFeatures *pFeatures)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   *pFeatures = physical_dev->features.features;
}

void
vn_GetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                               VkPhysicalDeviceProperties *pProperties)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   *pProperties = physical_dev->properties.properties;
}

void
vn_GetPhysicalDeviceQueueFamilyProperties(
   VkPhysicalDevice physicalDevice,
   uint32_t *pQueueFamilyPropertyCount,
   VkQueueFamilyProperties *pQueueFamilyProperties)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   VK_OUTARRAY_MAKE(out, pQueueFamilyProperties, pQueueFamilyPropertyCount);
   for (uint32_t i = 0; i < physical_dev->queue_family_count; i++) {
      vk_outarray_append (&out, props) {
         *props =
            physical_dev->queue_family_properties[i].queueFamilyProperties;
      }
   }
}

void
vn_GetPhysicalDeviceMemoryProperties(
   VkPhysicalDevice physicalDevice,
   VkPhysicalDeviceMemoryProperties *pMemoryProperties)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   *pMemoryProperties = physical_dev->memory_properties.memoryProperties;
}

void
vn_GetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice,
                                     VkFormat format,
                                     VkFormatProperties *pFormatProperties)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   /* TODO query all formats during init */
   vn_call_vkGetPhysicalDeviceFormatProperties(
      physical_dev->instance, physicalDevice, format, pFormatProperties);
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
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   /* TODO per-device cache */
   VkResult result = vn_call_vkGetPhysicalDeviceImageFormatProperties(
      physical_dev->instance, physicalDevice, format, type, tiling, usage,
      flags, pImageFormatProperties);

   return vn_result(physical_dev->instance, result);
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
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   /* TODO per-device cache */
   vn_call_vkGetPhysicalDeviceSparseImageFormatProperties(
      physical_dev->instance, physicalDevice, format, type, samples, usage,
      tiling, pPropertyCount, pProperties);
}

void
vn_GetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice,
                              VkPhysicalDeviceFeatures2 *pFeatures)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);
   const struct VkPhysicalDeviceVulkan11Features *vk11_feats =
      &physical_dev->vulkan_1_1_features;
   const struct VkPhysicalDeviceVulkan12Features *vk12_feats =
      &physical_dev->vulkan_1_2_features;
   union {
      VkBaseOutStructure *pnext;

      /* Vulkan 1.1 */
      VkPhysicalDevice16BitStorageFeatures *sixteen_bit_storage;
      VkPhysicalDeviceMultiviewFeatures *multiview;
      VkPhysicalDeviceVariablePointersFeatures *variable_pointers;
      VkPhysicalDeviceProtectedMemoryFeatures *protected_memory;
      VkPhysicalDeviceSamplerYcbcrConversionFeatures *sampler_ycbcr_conversion;
      VkPhysicalDeviceShaderDrawParametersFeatures *shader_draw_parameters;

      /* Vulkan 1.2 */
      VkPhysicalDevice8BitStorageFeatures *eight_bit_storage;
      VkPhysicalDeviceShaderAtomicInt64Features *shader_atomic_int64;
      VkPhysicalDeviceShaderFloat16Int8Features *shader_float16_int8;
      VkPhysicalDeviceDescriptorIndexingFeatures *descriptor_indexing;
      VkPhysicalDeviceScalarBlockLayoutFeatures *scalar_block_layout;
      VkPhysicalDeviceImagelessFramebufferFeatures *imageless_framebuffer;
      VkPhysicalDeviceUniformBufferStandardLayoutFeatures
         *uniform_buffer_standard_layout;
      VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures
         *shader_subgroup_extended_types;
      VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures
         *separate_depth_stencil_layouts;
      VkPhysicalDeviceHostQueryResetFeatures *host_query_reset;
      VkPhysicalDeviceTimelineSemaphoreFeatures *timeline_semaphore;
      VkPhysicalDeviceBufferDeviceAddressFeatures *buffer_device_address;
      VkPhysicalDeviceVulkanMemoryModelFeatures *vulkan_memory_model;
   } u;

   u.pnext = (VkBaseOutStructure *)pFeatures;
   while (u.pnext) {
      void *saved = u.pnext->pNext;
      switch (u.pnext->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2:
         memcpy(u.pnext, &physical_dev->features,
                sizeof(physical_dev->features));
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
         memcpy(u.pnext, vk11_feats, sizeof(*vk11_feats));
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
         memcpy(u.pnext, vk12_feats, sizeof(*vk12_feats));
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
         u.sixteen_bit_storage->storageBuffer16BitAccess =
            vk11_feats->storageBuffer16BitAccess;
         u.sixteen_bit_storage->uniformAndStorageBuffer16BitAccess =
            vk11_feats->uniformAndStorageBuffer16BitAccess;
         u.sixteen_bit_storage->storagePushConstant16 =
            vk11_feats->storagePushConstant16;
         u.sixteen_bit_storage->storageInputOutput16 =
            vk11_feats->storageInputOutput16;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
         u.multiview->multiview = vk11_feats->multiview;
         u.multiview->multiviewGeometryShader =
            vk11_feats->multiviewGeometryShader;
         u.multiview->multiviewTessellationShader =
            vk11_feats->multiviewTessellationShader;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES:
         u.variable_pointers->variablePointersStorageBuffer =
            vk11_feats->variablePointersStorageBuffer;
         u.variable_pointers->variablePointers = vk11_feats->variablePointers;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
         u.protected_memory->protectedMemory = vk11_feats->protectedMemory;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
         u.sampler_ycbcr_conversion->samplerYcbcrConversion =
            vk11_feats->samplerYcbcrConversion;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
         u.shader_draw_parameters->shaderDrawParameters =
            vk11_feats->shaderDrawParameters;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES:
         u.eight_bit_storage->storageBuffer8BitAccess =
            vk12_feats->storageBuffer8BitAccess;
         u.eight_bit_storage->uniformAndStorageBuffer8BitAccess =
            vk12_feats->uniformAndStorageBuffer8BitAccess;
         u.eight_bit_storage->storagePushConstant8 =
            vk12_feats->storagePushConstant8;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES:
         u.shader_atomic_int64->shaderBufferInt64Atomics =
            vk12_feats->shaderBufferInt64Atomics;
         u.shader_atomic_int64->shaderSharedInt64Atomics =
            vk12_feats->shaderSharedInt64Atomics;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES:
         u.shader_float16_int8->shaderFloat16 = vk12_feats->shaderFloat16;
         u.shader_float16_int8->shaderInt8 = vk12_feats->shaderInt8;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES:
         u.descriptor_indexing->shaderInputAttachmentArrayDynamicIndexing =
            vk12_feats->shaderInputAttachmentArrayDynamicIndexing;
         u.descriptor_indexing->shaderUniformTexelBufferArrayDynamicIndexing =
            vk12_feats->shaderUniformTexelBufferArrayDynamicIndexing;
         u.descriptor_indexing->shaderStorageTexelBufferArrayDynamicIndexing =
            vk12_feats->shaderStorageTexelBufferArrayDynamicIndexing;
         u.descriptor_indexing->shaderUniformBufferArrayNonUniformIndexing =
            vk12_feats->shaderUniformBufferArrayNonUniformIndexing;
         u.descriptor_indexing->shaderSampledImageArrayNonUniformIndexing =
            vk12_feats->shaderSampledImageArrayNonUniformIndexing;
         u.descriptor_indexing->shaderStorageBufferArrayNonUniformIndexing =
            vk12_feats->shaderStorageBufferArrayNonUniformIndexing;
         u.descriptor_indexing->shaderStorageImageArrayNonUniformIndexing =
            vk12_feats->shaderStorageImageArrayNonUniformIndexing;
         u.descriptor_indexing->shaderInputAttachmentArrayNonUniformIndexing =
            vk12_feats->shaderInputAttachmentArrayNonUniformIndexing;
         u.descriptor_indexing
            ->shaderUniformTexelBufferArrayNonUniformIndexing =
            vk12_feats->shaderUniformTexelBufferArrayNonUniformIndexing;
         u.descriptor_indexing
            ->shaderStorageTexelBufferArrayNonUniformIndexing =
            vk12_feats->shaderStorageTexelBufferArrayNonUniformIndexing;
         u.descriptor_indexing->descriptorBindingUniformBufferUpdateAfterBind =
            vk12_feats->descriptorBindingUniformBufferUpdateAfterBind;
         u.descriptor_indexing->descriptorBindingSampledImageUpdateAfterBind =
            vk12_feats->descriptorBindingSampledImageUpdateAfterBind;
         u.descriptor_indexing->descriptorBindingStorageImageUpdateAfterBind =
            vk12_feats->descriptorBindingStorageImageUpdateAfterBind;
         u.descriptor_indexing->descriptorBindingStorageBufferUpdateAfterBind =
            vk12_feats->descriptorBindingStorageBufferUpdateAfterBind;
         u.descriptor_indexing
            ->descriptorBindingUniformTexelBufferUpdateAfterBind =
            vk12_feats->descriptorBindingUniformTexelBufferUpdateAfterBind;
         u.descriptor_indexing
            ->descriptorBindingStorageTexelBufferUpdateAfterBind =
            vk12_feats->descriptorBindingStorageTexelBufferUpdateAfterBind;
         u.descriptor_indexing->descriptorBindingUpdateUnusedWhilePending =
            vk12_feats->descriptorBindingUpdateUnusedWhilePending;
         u.descriptor_indexing->descriptorBindingPartiallyBound =
            vk12_feats->descriptorBindingPartiallyBound;
         u.descriptor_indexing->descriptorBindingVariableDescriptorCount =
            vk12_feats->descriptorBindingVariableDescriptorCount;
         u.descriptor_indexing->runtimeDescriptorArray =
            vk12_feats->runtimeDescriptorArray;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
         u.scalar_block_layout->scalarBlockLayout =
            vk12_feats->scalarBlockLayout;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES:
         u.imageless_framebuffer->imagelessFramebuffer =
            vk12_feats->imagelessFramebuffer;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES:
         u.uniform_buffer_standard_layout->uniformBufferStandardLayout =
            vk12_feats->uniformBufferStandardLayout;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES:
         u.shader_subgroup_extended_types->shaderSubgroupExtendedTypes =
            vk12_feats->shaderSubgroupExtendedTypes;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES:
         u.separate_depth_stencil_layouts->separateDepthStencilLayouts =
            vk12_feats->separateDepthStencilLayouts;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES:
         u.host_query_reset->hostQueryReset = vk12_feats->hostQueryReset;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
         u.timeline_semaphore->timelineSemaphore =
            vk12_feats->timelineSemaphore;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
         u.buffer_device_address->bufferDeviceAddress =
            vk12_feats->bufferDeviceAddress;
         u.buffer_device_address->bufferDeviceAddressCaptureReplay =
            vk12_feats->bufferDeviceAddressCaptureReplay;
         u.buffer_device_address->bufferDeviceAddressMultiDevice =
            vk12_feats->bufferDeviceAddressMultiDevice;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES:
         u.vulkan_memory_model->vulkanMemoryModel =
            vk12_feats->vulkanMemoryModel;
         u.vulkan_memory_model->vulkanMemoryModelDeviceScope =
            vk12_feats->vulkanMemoryModelDeviceScope;
         u.vulkan_memory_model->vulkanMemoryModelAvailabilityVisibilityChains =
            vk12_feats->vulkanMemoryModelAvailabilityVisibilityChains;
         break;
      default:
         break;
      }
      u.pnext->pNext = saved;

      u.pnext = u.pnext->pNext;
   }
}

void
vn_GetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                VkPhysicalDeviceProperties2 *pProperties)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);
   const struct VkPhysicalDeviceVulkan11Properties *vk11_props =
      &physical_dev->vulkan_1_1_properties;
   const struct VkPhysicalDeviceVulkan12Properties *vk12_props =
      &physical_dev->vulkan_1_2_properties;
   union {
      VkBaseOutStructure *pnext;

      /* Vulkan 1.1 */
      VkPhysicalDeviceIDProperties *id;
      VkPhysicalDeviceSubgroupProperties *subgroup;
      VkPhysicalDevicePointClippingProperties *point_clipping;
      VkPhysicalDeviceMultiviewProperties *multiview;
      VkPhysicalDeviceProtectedMemoryProperties *protected_memory;
      VkPhysicalDeviceMaintenance3Properties *maintenance_3;

      /* Vulkan 1.2 */
      VkPhysicalDeviceDriverProperties *driver;
      VkPhysicalDeviceFloatControlsProperties *float_controls;
      VkPhysicalDeviceDescriptorIndexingProperties *descriptor_indexing;
      VkPhysicalDeviceDepthStencilResolveProperties *depth_stencil_resolve;
      VkPhysicalDeviceSamplerFilterMinmaxProperties *sampler_filter_minmax;
      VkPhysicalDeviceTimelineSemaphoreProperties *timeline_semaphore;

      VkPhysicalDevicePCIBusInfoPropertiesEXT *pci_bus_info;
   } u;

   u.pnext = (VkBaseOutStructure *)pProperties;
   while (u.pnext) {
      void *saved = u.pnext->pNext;
      switch (u.pnext->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2:
         memcpy(u.pnext, &physical_dev->properties,
                sizeof(physical_dev->properties));
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES:
         memcpy(u.pnext, vk11_props, sizeof(*vk11_props));
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES:
         memcpy(u.pnext, vk12_props, sizeof(*vk12_props));
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES:
         memcpy(u.id->deviceUUID, vk11_props->deviceUUID,
                sizeof(vk11_props->deviceUUID));
         memcpy(u.id->driverUUID, vk11_props->driverUUID,
                sizeof(vk11_props->driverUUID));
         memcpy(u.id->deviceLUID, vk11_props->deviceLUID,
                sizeof(vk11_props->deviceLUID));
         u.id->deviceNodeMask = vk11_props->deviceNodeMask;
         u.id->deviceLUIDValid = vk11_props->deviceLUIDValid;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES:
         u.subgroup->subgroupSize = vk11_props->subgroupSize;
         u.subgroup->supportedStages = vk11_props->subgroupSupportedStages;
         u.subgroup->supportedOperations =
            vk11_props->subgroupSupportedOperations;
         u.subgroup->quadOperationsInAllStages =
            vk11_props->subgroupQuadOperationsInAllStages;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES:
         u.point_clipping->pointClippingBehavior =
            vk11_props->pointClippingBehavior;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES:
         u.multiview->maxMultiviewViewCount =
            vk11_props->maxMultiviewViewCount;
         u.multiview->maxMultiviewInstanceIndex =
            vk11_props->maxMultiviewInstanceIndex;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES:
         u.protected_memory->protectedNoFault = vk11_props->protectedNoFault;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES:
         u.maintenance_3->maxPerSetDescriptors =
            vk11_props->maxPerSetDescriptors;
         u.maintenance_3->maxMemoryAllocationSize =
            vk11_props->maxMemoryAllocationSize;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES:
         u.driver->driverID = vk12_props->driverID;
         memcpy(u.driver->driverName, vk12_props->driverName,
                sizeof(vk12_props->driverName));
         memcpy(u.driver->driverInfo, vk12_props->driverInfo,
                sizeof(vk12_props->driverInfo));
         u.driver->conformanceVersion = vk12_props->conformanceVersion;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES:
         u.float_controls->denormBehaviorIndependence =
            vk12_props->denormBehaviorIndependence;
         u.float_controls->roundingModeIndependence =
            vk12_props->roundingModeIndependence;
         u.float_controls->shaderSignedZeroInfNanPreserveFloat16 =
            vk12_props->shaderSignedZeroInfNanPreserveFloat16;
         u.float_controls->shaderSignedZeroInfNanPreserveFloat32 =
            vk12_props->shaderSignedZeroInfNanPreserveFloat32;
         u.float_controls->shaderSignedZeroInfNanPreserveFloat64 =
            vk12_props->shaderSignedZeroInfNanPreserveFloat64;
         u.float_controls->shaderDenormPreserveFloat16 =
            vk12_props->shaderDenormPreserveFloat16;
         u.float_controls->shaderDenormPreserveFloat32 =
            vk12_props->shaderDenormPreserveFloat32;
         u.float_controls->shaderDenormPreserveFloat64 =
            vk12_props->shaderDenormPreserveFloat64;
         u.float_controls->shaderDenormFlushToZeroFloat16 =
            vk12_props->shaderDenormFlushToZeroFloat16;
         u.float_controls->shaderDenormFlushToZeroFloat32 =
            vk12_props->shaderDenormFlushToZeroFloat32;
         u.float_controls->shaderDenormFlushToZeroFloat64 =
            vk12_props->shaderDenormFlushToZeroFloat64;
         u.float_controls->shaderRoundingModeRTEFloat16 =
            vk12_props->shaderRoundingModeRTEFloat16;
         u.float_controls->shaderRoundingModeRTEFloat32 =
            vk12_props->shaderRoundingModeRTEFloat32;
         u.float_controls->shaderRoundingModeRTEFloat64 =
            vk12_props->shaderRoundingModeRTEFloat64;
         u.float_controls->shaderRoundingModeRTZFloat16 =
            vk12_props->shaderRoundingModeRTZFloat16;
         u.float_controls->shaderRoundingModeRTZFloat32 =
            vk12_props->shaderRoundingModeRTZFloat32;
         u.float_controls->shaderRoundingModeRTZFloat64 =
            vk12_props->shaderRoundingModeRTZFloat64;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES:
         u.descriptor_indexing->maxUpdateAfterBindDescriptorsInAllPools =
            vk12_props->maxUpdateAfterBindDescriptorsInAllPools;
         u.descriptor_indexing
            ->shaderUniformBufferArrayNonUniformIndexingNative =
            vk12_props->shaderUniformBufferArrayNonUniformIndexingNative;
         u.descriptor_indexing
            ->shaderSampledImageArrayNonUniformIndexingNative =
            vk12_props->shaderSampledImageArrayNonUniformIndexingNative;
         u.descriptor_indexing
            ->shaderStorageBufferArrayNonUniformIndexingNative =
            vk12_props->shaderStorageBufferArrayNonUniformIndexingNative;
         u.descriptor_indexing
            ->shaderStorageImageArrayNonUniformIndexingNative =
            vk12_props->shaderStorageImageArrayNonUniformIndexingNative;
         u.descriptor_indexing
            ->shaderInputAttachmentArrayNonUniformIndexingNative =
            vk12_props->shaderInputAttachmentArrayNonUniformIndexingNative;
         u.descriptor_indexing->robustBufferAccessUpdateAfterBind =
            vk12_props->robustBufferAccessUpdateAfterBind;
         u.descriptor_indexing->quadDivergentImplicitLod =
            vk12_props->quadDivergentImplicitLod;
         u.descriptor_indexing->maxPerStageDescriptorUpdateAfterBindSamplers =
            vk12_props->maxPerStageDescriptorUpdateAfterBindSamplers;
         u.descriptor_indexing
            ->maxPerStageDescriptorUpdateAfterBindUniformBuffers =
            vk12_props->maxPerStageDescriptorUpdateAfterBindUniformBuffers;
         u.descriptor_indexing
            ->maxPerStageDescriptorUpdateAfterBindStorageBuffers =
            vk12_props->maxPerStageDescriptorUpdateAfterBindStorageBuffers;
         u.descriptor_indexing
            ->maxPerStageDescriptorUpdateAfterBindSampledImages =
            vk12_props->maxPerStageDescriptorUpdateAfterBindSampledImages;
         u.descriptor_indexing
            ->maxPerStageDescriptorUpdateAfterBindStorageImages =
            vk12_props->maxPerStageDescriptorUpdateAfterBindStorageImages;
         u.descriptor_indexing
            ->maxPerStageDescriptorUpdateAfterBindInputAttachments =
            vk12_props->maxPerStageDescriptorUpdateAfterBindInputAttachments;
         u.descriptor_indexing->maxPerStageUpdateAfterBindResources =
            vk12_props->maxPerStageUpdateAfterBindResources;
         u.descriptor_indexing->maxDescriptorSetUpdateAfterBindSamplers =
            vk12_props->maxDescriptorSetUpdateAfterBindSamplers;
         u.descriptor_indexing->maxDescriptorSetUpdateAfterBindUniformBuffers =
            vk12_props->maxDescriptorSetUpdateAfterBindUniformBuffers;
         u.descriptor_indexing
            ->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic =
            vk12_props->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic;
         u.descriptor_indexing->maxDescriptorSetUpdateAfterBindStorageBuffers =
            vk12_props->maxDescriptorSetUpdateAfterBindStorageBuffers;
         u.descriptor_indexing
            ->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic =
            vk12_props->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
         u.descriptor_indexing->maxDescriptorSetUpdateAfterBindSampledImages =
            vk12_props->maxDescriptorSetUpdateAfterBindSampledImages;
         u.descriptor_indexing->maxDescriptorSetUpdateAfterBindStorageImages =
            vk12_props->maxDescriptorSetUpdateAfterBindStorageImages;
         u.descriptor_indexing
            ->maxDescriptorSetUpdateAfterBindInputAttachments =
            vk12_props->maxDescriptorSetUpdateAfterBindInputAttachments;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES:
         u.depth_stencil_resolve->supportedDepthResolveModes =
            vk12_props->supportedDepthResolveModes;
         u.depth_stencil_resolve->supportedStencilResolveModes =
            vk12_props->supportedStencilResolveModes;
         u.depth_stencil_resolve->independentResolveNone =
            vk12_props->independentResolveNone;
         u.depth_stencil_resolve->independentResolve =
            vk12_props->independentResolve;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES:
         u.sampler_filter_minmax->filterMinmaxSingleComponentFormats =
            vk12_props->filterMinmaxSingleComponentFormats;
         u.sampler_filter_minmax->filterMinmaxImageComponentMapping =
            vk12_props->filterMinmaxImageComponentMapping;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES:
         u.timeline_semaphore->maxTimelineSemaphoreValueDifference =
            vk12_props->maxTimelineSemaphoreValueDifference;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT:
         /* this is used by WSI */
         if (physical_dev->instance->renderer_info.pci.has_bus_info) {
            u.pci_bus_info->pciDomain =
               physical_dev->instance->renderer_info.pci.domain;
            u.pci_bus_info->pciBus =
               physical_dev->instance->renderer_info.pci.bus;
            u.pci_bus_info->pciDevice =
               physical_dev->instance->renderer_info.pci.device;
            u.pci_bus_info->pciFunction =
               physical_dev->instance->renderer_info.pci.function;
         }
         break;
      default:
         break;
      }
      u.pnext->pNext = saved;

      u.pnext = u.pnext->pNext;
   }
}

void
vn_GetPhysicalDeviceQueueFamilyProperties2(
   VkPhysicalDevice physicalDevice,
   uint32_t *pQueueFamilyPropertyCount,
   VkQueueFamilyProperties2 *pQueueFamilyProperties)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   VK_OUTARRAY_MAKE(out, pQueueFamilyProperties, pQueueFamilyPropertyCount);
   for (uint32_t i = 0; i < physical_dev->queue_family_count; i++) {
      vk_outarray_append (&out, props) {
         *props = physical_dev->queue_family_properties[i];
      }
   }
}

void
vn_GetPhysicalDeviceMemoryProperties2(
   VkPhysicalDevice physicalDevice,
   VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   pMemoryProperties->memoryProperties =
      physical_dev->memory_properties.memoryProperties;
}

void
vn_GetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice,
                                      VkFormat format,
                                      VkFormatProperties2 *pFormatProperties)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   /* TODO query all formats during init */
   vn_call_vkGetPhysicalDeviceFormatProperties2(
      physical_dev->instance, physicalDevice, format, pFormatProperties);
}

VkResult
vn_GetPhysicalDeviceImageFormatProperties2(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo,
   VkImageFormatProperties2 *pImageFormatProperties)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   VkResult result;
   /* TODO per-device cache */
   result = vn_call_vkGetPhysicalDeviceImageFormatProperties2(
      physical_dev->instance, physicalDevice, pImageFormatInfo,
      pImageFormatProperties);

   VkExternalImageFormatProperties *props = vk_find_struct(
      pImageFormatProperties->pNext, EXTERNAL_IMAGE_FORMAT_PROPERTIES);
   if (props) {
      memset(&props->externalMemoryProperties, 0,
             sizeof(props->externalMemoryProperties));
   }

   return vn_result(physical_dev->instance, result);
}

void
vn_GetPhysicalDeviceSparseImageFormatProperties2(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceSparseImageFormatInfo2 *pFormatInfo,
   uint32_t *pPropertyCount,
   VkSparseImageFormatProperties2 *pProperties)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);

   /* TODO per-device cache */
   vn_call_vkGetPhysicalDeviceSparseImageFormatProperties2(
      physical_dev->instance, physicalDevice, pFormatInfo, pPropertyCount,
      pProperties);
}

void
vn_GetPhysicalDeviceExternalBufferProperties(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo,
   VkExternalBufferProperties *pExternalBufferProperties)
{
   VkExternalMemoryProperties *props =
      &pExternalBufferProperties->externalMemoryProperties;

   props->compatibleHandleTypes = pExternalBufferInfo->handleType;
   props->exportFromImportedHandleTypes = 0;
   props->externalMemoryFeatures = 0;
}

void
vn_GetPhysicalDeviceExternalFenceProperties(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo,
   VkExternalFenceProperties *pExternalFenceProperties)
{
   pExternalFenceProperties->compatibleHandleTypes =
      pExternalFenceInfo->handleType;
   pExternalFenceProperties->exportFromImportedHandleTypes = 0;
   pExternalFenceProperties->externalFenceFeatures = 0;
}

void
vn_GetPhysicalDeviceExternalSemaphoreProperties(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo,
   VkExternalSemaphoreProperties *pExternalSemaphoreProperties)
{
   pExternalSemaphoreProperties->compatibleHandleTypes =
      pExternalSemaphoreInfo->handleType;
   pExternalSemaphoreProperties->exportFromImportedHandleTypes = 0;
   pExternalSemaphoreProperties->externalSemaphoreFeatures = 0;
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
