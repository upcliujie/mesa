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
#ifndef VK_IMPLICIT_MEMORY_SYNC_H
#define VK_IMPLICIT_MEMORY_SYNC_H

#include "vk_sync.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vk_implicit_memory_sync {
   struct vk_sync sync;

   VkDeviceMemory memory;
};

VkResult MUST_CHECK
vk_implicit_memory_sync_create(struct vk_device *device,
                               VkDeviceMemory memory,
                               struct vk_sync **sync_out);

void vk_implicit_memory_sync_finish(struct vk_device *device,
                                    struct vk_sync *sync);

static inline bool
vk_sync_type_is_implicit_memory_sync(const struct vk_sync_type *type)
{
   return type->finish == vk_implicit_memory_sync_finish;
}

static inline struct vk_implicit_memory_sync *
vk_sync_as_implicit_memory_sync(struct vk_sync *sync)
{
   if (!vk_sync_type_is_implicit_memory_sync(sync->type))
      return NULL;

   return container_of(sync, struct vk_implicit_memory_sync, sync);
}

#ifdef __cplusplus
}
#endif

#endif /* VK_IMPLICIT_MEMORY_SYNC_H */
