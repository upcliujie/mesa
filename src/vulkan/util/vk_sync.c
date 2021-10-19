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

#include "vk_sync.h"

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "util/macros.h"
#include "util/os_time.h"

#include "vk_alloc.h"
#include "vk_device.h"
#include "vk_log.h"

VkResult
vk_sync_init(struct vk_device *device,
             struct vk_sync *sync,
             const struct vk_sync_type *type,
             uint64_t initial_value)
{
   assert(type->size >= sizeof(*sync));
   memset(sync, 0, type->size);

   assert(type->init);
   assert(type->finish);
   if (type->is_timeline) {
      assert(type->signal);
      assert(type->get_value);
      assert(vk_sync_type_has_cpu_wait(type));
      assert(!type->import_sync_file);
      assert(!type->export_sync_file);
   }

   sync->type = type;

   return type->init(device, sync, initial_value);
}

void
vk_sync_finish(struct vk_device *device,
               struct vk_sync *sync)
{
   sync->type->finish(device, sync);
}

VkResult
vk_sync_create(struct vk_device *device,
               const struct vk_sync_type *type,
               uint64_t initial_value,
               struct vk_sync **sync_out)
{
   struct vk_sync *sync;

   sync = vk_alloc(&device->alloc, type->size, 8,
                   VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (sync == NULL)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   VkResult result = vk_sync_init(device, sync, type, initial_value);
   if (result != VK_SUCCESS) {
      vk_free(&device->alloc, sync);
      return result;
   }

   *sync_out = sync;

   return VK_SUCCESS;
}

void
vk_sync_destroy(struct vk_device *device,
                struct vk_sync *sync)
{
   vk_sync_finish(device, sync);
   vk_free(&device->alloc, sync);
}

VkResult
vk_sync_signal(struct vk_device *device,
               struct vk_sync *sync,
               uint64_t value)
{
   if (sync->type->is_timeline)
      assert(value > 0);
   else
      assert(value == 0);

   return sync->type->signal(device, sync, value);
}

VkResult
vk_sync_get_value(struct vk_device *device,
                  struct vk_sync *sync,
                  uint64_t *value)
{
   assert(sync->type->is_timeline);
   return sync->type->get_value(device, sync, value);
}

VkResult
vk_sync_reset(struct vk_device *device,
              struct vk_sync *sync)
{
   assert(!sync->type->is_timeline);
   return sync->type->reset(device, sync);
}

VkResult vk_sync_move(struct vk_device *device,
                      struct vk_sync *dst,
                      struct vk_sync *src)
{
   assert(!dst->type->is_timeline);
   assert(!src->type->is_timeline);
   assert(dst->type == src->type);

   return src->type->move(device, dst, src);
}

VkResult
vk_sync_wait(struct vk_device *device,
             struct vk_sync *sync,
             uint64_t wait_value,
             enum vk_sync_wait_type wait_type,
             uint64_t abs_timeout_ns)
{
   /* One of wait or wait_all is required for CPU waits */
   assert(sync->type->wait || sync->type->wait_all);
   assert(sync->type->is_timeline || wait_value == 0);

   if (sync->type->wait) {
      return sync->type->wait(device, sync, wait_value,
                              wait_type, abs_timeout_ns);
   } else {
      struct vk_sync_wait wait = {
         .sync = sync,
         .stage_mask = ~(VkPipelineStageFlags2KHR)0,
         .wait_value = wait_value,
      };
      return sync->type->wait_all(device, 1, &wait, wait_type, abs_timeout_ns);
   }
}

VkResult
vk_sync_wait_all(struct vk_device *device,
                 uint32_t wait_count,
                 const struct vk_sync_wait *waits,
                 enum vk_sync_wait_type wait_type,
                 uint64_t abs_timeout_ns)
{
   if (wait_count == 0)
      return VK_SUCCESS;

   if (wait_count == 1) {
      return vk_sync_wait(device, waits[0].sync, waits[0].wait_value,
                          wait_type, abs_timeout_ns);
   }

   bool all_same_type = true;
   for (uint32_t i = 0; i < wait_count; i++) {
      assert(waits[i].sync->type->is_timeline || waits[i].wait_value == 0);
      if (waits[i].sync->type != waits[0].sync->type)
         all_same_type = false;
   }

   if (all_same_type && waits[0].sync->type->wait_all) {
      return waits[0].sync->type->wait_all(device, wait_count, waits,
                                           wait_type, abs_timeout_ns);
   } else {
      for (uint32_t i = 0; i < wait_count; i++) {
         VkResult result = vk_sync_wait(device, waits[i].sync,
                                        waits[i].wait_value,
                                        wait_type, abs_timeout_ns);
         if (result != VK_SUCCESS)
            return result;
      }
      return VK_SUCCESS;
   }
}

VkResult
vk_sync_wait_any(struct vk_device *device,
                 uint32_t wait_count,
                 const struct vk_sync_wait *waits,
                 enum vk_sync_wait_type wait_type,
                 uint64_t abs_timeout_ns)
{
   if (wait_count == 0)
      return VK_SUCCESS;

   if (wait_count == 1) {
      return vk_sync_wait(device, waits[0].sync, waits[0].wait_value,
                          wait_type, abs_timeout_ns);
   }

   bool all_same_type = true;
   for (uint32_t i = 0; i < wait_count; i++) {
      assert(waits[i].sync->type->is_timeline || waits[i].wait_value == 0);
      if (waits[i].sync->type != waits[0].sync->type)
         all_same_type = false;
   }

   if (waits[0].sync->type->wait_any && all_same_type) {
      return waits[0].sync->type->wait_any(device, wait_count, waits,
                                           wait_type, abs_timeout_ns);
   } else {
      /* If we have multiple syncs and they don't support wait_any or they're
       * not all the same type, there's nothing better we can do than spin.
       */
      do {
         for (uint32_t i = 0; i < wait_count; i++) {
            VkResult result = vk_sync_wait(device, waits[i].sync,
                                           waits[i].wait_value, wait_type,
                                           0 /* abs_timeout_ns */);
            if (result != VK_TIMEOUT)
               return result;
         }
      } while (os_time_get_nano() < abs_timeout_ns);

      return VK_TIMEOUT;
   }
}

VkResult
vk_sync_import_opaque_fd(struct vk_device *device,
                         struct vk_sync *sync,
                         int fd)
{
   return sync->type->import_opaque_fd(device, sync, fd);
}

VkResult
vk_sync_export_opaque_fd(struct vk_device *device,
                         struct vk_sync *sync,
                         int *fd)
{
   return sync->type->export_opaque_fd(device, sync, fd);
}

VkResult
vk_sync_import_sync_file(struct vk_device *device,
                         struct vk_sync *sync,
                         int sync_file)
{
   assert(!sync->type->is_timeline);

   /* Silently handle negative file descriptors in case the driver doesn't
    * want to bother.
    */
   if (sync_file < 0 && sync->type->signal)
      return sync->type->signal(device, sync, 0);

   return sync->type->import_sync_file(device, sync, sync_file);
}

VkResult
vk_sync_export_sync_file(struct vk_device *device,
                         struct vk_sync *sync,
                         int *sync_file)
{
   assert(!sync->type->is_timeline);
   return sync->type->export_sync_file(device, sync, sync_file);
}
