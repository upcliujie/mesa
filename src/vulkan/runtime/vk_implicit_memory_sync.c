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

#include "vk_implicit_memory_sync.h"

#include "vk_alloc.h"
#include "vk_device.h"
#include "vk_log.h"

void
vk_implicit_memory_sync_finish(struct vk_device *device,
                               struct vk_sync *sync)
{ }

static const struct vk_sync_type vk_implicit_memory_sync_type = {
   .features = VK_SYNC_FEATURE_BINARY |
               VK_SYNC_FEATURE_GPU_WAIT |
               VK_SYNC_FEATURE_GPU_MULTI_WAIT,
   .size = sizeof(struct vk_implicit_memory_sync),
   .finish = vk_implicit_memory_sync_finish,
};

VkResult
vk_implicit_memory_sync_create(struct vk_device *device,
                               VkDeviceMemory memory,
                               struct vk_sync **sync_out)
{
   struct vk_implicit_memory_sync *mem_sync;

   mem_sync = vk_zalloc(&device->alloc, sizeof(*mem_sync), 8,
                        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (unlikely(mem_sync == NULL))
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   mem_sync->sync.type = &vk_implicit_memory_sync_type;
   mem_sync->memory = memory;

   *sync_out = &mem_sync->sync;

   return VK_SUCCESS;
}
