/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 */

#ifndef VN_DEVICE_H
#define VN_DEVICE_H

#include "vn_common.h"

#include "vn_cs.h"
#include "vn_renderer.h"

struct vn_instance {
   struct vn_cs_object base;

   VkAllocationCallbacks allocator;

   uint32_t api_version;
   struct vn_instance_extension_table enabled_extensions;

   struct vn_instance_dispatch_table dispatch;
   struct vn_physical_device_dispatch_table physical_device_dispatch;
   struct vn_device_dispatch_table device_dispatch;

   struct vn_renderer *renderer;
   struct vn_renderer_info renderer_info;
   uint32_t renderer_version;

   mtx_t cs_mutex;
   struct vn_cs cs;
   struct {
      struct vn_renderer_bo *bo;
      size_t size;
      size_t used;
      void *ptr;
   } cs_reply;

   mtx_t physical_device_mutex;
   struct vn_physical_device *physical_devices;
   uint32_t physical_device_count;
};
VK_DEFINE_HANDLE_CASTS(vn_instance,
                       base.base,
                       VkInstance,
                       VK_OBJECT_TYPE_INSTANCE)

struct vn_physical_device {
   struct vn_cs_object base;

   struct vn_instance *instance;

   uint32_t renderer_version;
   struct vn_device_extension_table renderer_extensions;

   struct vn_device_extension_table supported_extensions;
   uint32_t *extension_spec_versions;

   VkPhysicalDeviceFeatures2 features;
   VkPhysicalDeviceVulkan11Features vulkan_1_1_features;
   VkPhysicalDeviceVulkan12Features vulkan_1_2_features;

   VkPhysicalDeviceProperties2 properties;
   VkPhysicalDeviceVulkan11Properties vulkan_1_1_properties;
   VkPhysicalDeviceVulkan12Properties vulkan_1_2_properties;

   VkQueueFamilyProperties2 *queue_family_properties;
   uint32_t *queue_family_sync_queue_bases;
   uint32_t queue_family_count;

   VkPhysicalDeviceMemoryProperties2 memory_properties;
};
VK_DEFINE_HANDLE_CASTS(vn_physical_device,
                       base.base,
                       VkPhysicalDevice,
                       VK_OBJECT_TYPE_PHYSICAL_DEVICE)

struct vn_device {
   struct vn_cs_device base;

   VkAllocationCallbacks allocator;

   struct vn_instance *instance;
   struct vn_physical_device *physical_device;
   struct vn_device_extension_table enabled_extensions;

   struct vn_device_dispatch_table dispatch;

   struct vn_queue *queues;
   uint32_t queue_count;
};
VK_DEFINE_HANDLE_CASTS(vn_device,
                       base.base.base,
                       VkDevice,
                       VK_OBJECT_TYPE_DEVICE)

struct vn_queue {
   struct vn_cs_object base;

   struct vn_device *device;
   uint32_t family;
   uint32_t index;
   uint32_t flags;

   uint32_t sync_queue_index;

   struct vn_renderer_sync *sync;
   uint64_t sync_point;
};
VK_DEFINE_HANDLE_CASTS(vn_queue, base.base, VkQueue, VK_OBJECT_TYPE_QUEUE)

enum vn_sync_type {
   VN_SYNC_TYPE_INVALID,
   VN_SYNC_TYPE_SYNC,
   VN_SYNC_TYPE_DEVICE_ONLY,
   VN_SYNC_TYPE_WSI_SIGNALED,
};

struct vn_sync_payload {
   enum vn_sync_type type;

   /* When we signal or reset, we update both the device object and the
    * renderer sync.  When we wait or query, we use the renderer sync only.
    */
   struct vn_renderer_sync *sync;
};

struct vn_fence {
   struct vn_cs_object base;

   struct vn_sync_payload *payload;

   struct vn_sync_payload permanent;
   struct vn_sync_payload temporary;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_fence,
                               base.base,
                               VkFence,
                               VK_OBJECT_TYPE_FENCE)

struct vn_semaphore {
   struct vn_cs_object base;

   VkSemaphoreType type;

   struct vn_sync_payload *payload;

   struct vn_sync_payload permanent;
   struct vn_sync_payload temporary;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_semaphore,
                               base.base,
                               VkSemaphore,
                               VK_OBJECT_TYPE_SEMAPHORE)

struct vn_command_buffer {
   struct vk_object_base base;

   struct vn_device *device;
};
VK_DEFINE_HANDLE_CASTS(vn_command_buffer,
                       base,
                       VkCommandBuffer,
                       VK_OBJECT_TYPE_COMMAND_BUFFER)

static inline struct vn_cs *
vn_instance_lock_cs(struct vn_instance *instance)
{
   mtx_lock(&instance->cs_mutex);
   return &instance->cs;
}

static inline void
vn_instance_unlock_cs(struct vn_instance *instance)
{
   assert(mtx_trylock(&instance->cs_mutex) == thrd_busy);
   mtx_unlock(&instance->cs_mutex);
}

bool
vn_instance_grow_cs_reply_locked(struct vn_instance *instance, size_t size);

static inline struct vn_renderer_bo *
vn_instance_alloc_cs_reply_locked(struct vn_instance *instance,
                                  size_t size,
                                  size_t *offset,
                                  void **ptr)
{
   if (unlikely(instance->cs_reply.used + size > instance->cs_reply.size)) {
      if (!vn_instance_grow_cs_reply_locked(instance, size))
         return NULL;
   }

   *offset = instance->cs_reply.used;
   *ptr = instance->cs_reply.ptr + instance->cs_reply.used;
   instance->cs_reply.used += size;

   return vn_renderer_bo_ref(instance->cs_reply.bo);
}

static inline void
vn_instance_free_cs_reply(struct vn_instance *instance,
                          struct vn_renderer_bo *bo)
{
   vn_renderer_bo_unref(bo, &instance->allocator);
}

void
vn_fence_signal_wsi(struct vn_device *dev, struct vn_fence *fence);

void
vn_semaphore_signal_wsi(struct vn_device *dev, struct vn_semaphore *sem);

#endif /* VN_DEVICE_H */
