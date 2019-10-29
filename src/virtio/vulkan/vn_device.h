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

      struct vn_renderer_sync *sync;
      uint64_t sync_value;
   } cs_reply;
};
VK_DEFINE_HANDLE_CASTS(vn_instance,
                       base.base,
                       VkInstance,
                       VK_OBJECT_TYPE_INSTANCE)

struct vn_physical_device {
   struct vk_object_base base;

   struct vn_instance *instance;
};
VK_DEFINE_HANDLE_CASTS(vn_physical_device,
                       base,
                       VkPhysicalDevice,
                       VK_OBJECT_TYPE_PHYSICAL_DEVICE)

struct vn_device {
   struct vk_device base;

   struct vn_device_dispatch_table dispatch;
};
VK_DEFINE_HANDLE_CASTS(vn_device, base.base, VkDevice, VK_OBJECT_TYPE_DEVICE)

struct vn_queue {
   struct vk_object_base base;

   struct vn_device *device;
};
VK_DEFINE_HANDLE_CASTS(vn_queue, base, VkQueue, VK_OBJECT_TYPE_QUEUE)

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

struct vn_renderer_bo *
vn_instance_get_cs_reply_bo_locked(struct vn_instance *instance,
                                   size_t size,
                                   void **ptr);

static inline bool
vn_instance_submit_cs_locked(struct vn_instance *instance,
                             struct vn_renderer_bo *reply_bo,
                             uint64_t *reply_sync_val)
{
   struct vn_cs *cs = &instance->cs;

   if (unlikely(vn_cs_has_error(cs))) {
      vn_cs_reset(cs);
      return false;
   }

   vn_cs_end_out(cs);

   VkResult result;
   if (reply_bo) {
      *reply_sync_val = ++instance->cs_reply.sync_value;
      const struct vn_renderer_submit submit = {
         .cs = cs,
         .bos = &reply_bo,
         .bo_count = 1,
         .batches =
            &(const struct vn_renderer_submit_batch){
               .cs_size = vn_cs_get_out_len(cs),
               .sync_queue_cpu = true,
               .syncs = &instance->cs_reply.sync,
               .sync_values = reply_sync_val,
               .sync_count = 1,
            },
         .batch_count = 1,
      };
      result = vn_renderer_submit(instance->renderer, &submit);
   } else {
      result = vn_renderer_submit_cs(instance->renderer, cs);
   }

   vn_cs_reset(cs);

   return result == VK_SUCCESS;
}

static inline void
vn_instance_unlock_cs(struct vn_instance *instance)
{
   assert(mtx_trylock(&instance->cs_mutex) == thrd_busy);
   mtx_unlock(&instance->cs_mutex);
}

static inline void
vn_instance_wait_cs_reply(struct vn_instance *instance,
                          uint64_t reply_sync_val)
{
   const struct vn_renderer_wait wait = {
      .timeout = UINT64_MAX,
      .syncs = &instance->cs_reply.sync,
      .sync_values = &reply_sync_val,
      .sync_count = 1,
   };
   vn_renderer_wait(instance->renderer, &wait);
}

static inline void
vn_instance_free_cs_reply_bo(struct vn_instance *instance,
                             struct vn_renderer_bo *bo)
{
   vn_renderer_bo_unref(bo, &instance->allocator);
}

#endif /* VN_DEVICE_H */
