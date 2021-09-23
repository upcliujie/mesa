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

#ifndef VK_QUEUE_H
#define VK_QUEUE_H

#include "vk_object.h"

#include "util/list.h"
#include "util/u_dynarray.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vk_queue {
   struct vk_object_base base;

   /* Link in vk_device::queues */
   struct list_head link;

   /* VkDeviceQueueCreateInfo::flags */
   VkDeviceQueueCreateFlags flags;

   /* VkDeviceQueueCreateInfo::queueFamilyIndex */
   uint32_t queue_family_index;

   /* Which queue this is within the queue family */
   uint32_t index_in_family;
};

VK_DEFINE_HANDLE_CASTS(vk_queue, base, VkQueue, VK_OBJECT_TYPE_QUEUE)

VkResult MUST_CHECK
vk_queue_init(struct vk_queue *queue, struct vk_device *device,
              const VkDeviceQueueCreateInfo *pCreateInfo,
              uint32_t index_in_family);

void
vk_queue_finish(struct vk_queue *queue);

#define vk_foreach_queue(queue, device) \
   list_for_each_entry(struct vk_queue, queue, &(device)->queues, link)

#define vk_foreach_queue_safe(queue, device) \
   list_for_each_entry_safe(struct vk_queue, queue, &(device)->queues, link)

#ifdef __cplusplus
}
#endif

#endif  /* VK_QUEUE_H */
