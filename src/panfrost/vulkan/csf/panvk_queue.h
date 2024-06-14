/*
 * Copyright © 2024 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_QUEUE_H
#define PANVK_QUEUE_H

#ifndef PAN_ARCH
#error "PAN_ARCH must be defined"
#endif

#include <stdint.h>

#include "vk_queue.h"

struct panvk_queue {
   struct vk_queue vk;

   /* Number of CS queues */
   int pqueue_count;

   uint32_t group_handle;

   /* Sync timeline, only used for debugging */
   struct {
      uint32_t handle;
      uint64_t point;
   } sync;
};

VK_DEFINE_HANDLE_CASTS(panvk_queue, vk.base, VkQueue, VK_OBJECT_TYPE_QUEUE)

static inline struct panvk_device *
panvk_queue_get_device(const struct panvk_queue *queue)
{
   return container_of(queue->vk.base.device, struct panvk_device, vk);
}

void panvk_per_arch(queue_finish)(struct panvk_queue *queue);

VkResult panvk_per_arch(queue_init)(struct panvk_device *device,
                                    struct panvk_queue *queue, int idx,
                                    const VkDeviceQueueCreateInfo *create_info);

#endif /* PANVK_QUEUE_H */
