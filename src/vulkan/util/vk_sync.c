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

VkResult
vk_sync_init(struct vk_device *device,
             struct vk_sync *sync,
             const struct vk_sync_type *type,
             uint64_t initial_value)
{
   assert(type->size >= sizeof(*sync));
   memset(sync, 0, type->size);

   assert(sync->type->init);
   assert(sync->type->finish);
   if (type->is_timeline) {
      assert(sync->type->signal);
      assert(sync->type->get_value);
      assert(sync->type->wait_all);
      assert(!sync->type->import_sync_file);
      assert(!sync->type->export_sync_file);
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

VkResult
vk_sync_wait(struct vk_device *device,
             struct vk_sync *sync,
             uint64_t wait_value,
             enum vk_sync_wait_type wait_type,
             uint64_t abs_timeout_ns)
{
   /* One of wait or wait_all is required for CPU waits */
   assert(sync->type->wait || sync->type->wait_all);

   if (sync->type->wait) {
      return sync->type->wait(device, sync, wait_value,
                              wait_type, abs_timeout_ns);
   } else {
      return sync->type->wait_all(device, 1, &sync, &wait_value,
                                  wait_type, abs_timeout_ns);
   }
}

VkResult
vk_sync_wait_all(struct vk_device *device,
                 uint32_t sync_count,
                 struct vk_sync **const syncs,
                 uint64_t *wait_values,
                 enum vk_sync_wait_type wait_type,
                 uint64_t abs_timeout_ns)
{
   if (sync_count == 0)
      return VK_SUCCESS;

   if (sync_count == 1) {
      return vk_sync_wait(device, syncs[0], wait_values[0],
                          wait_type, abs_timeout_ns);
   }

   bool all_same_type = true;
   for (uint32_t i = 0; i < sync_count; i++) {
      assert(!syncs[i]->type->is_timeline || wait_values);
      if (syncs[i]->type != syncs[0]->type)
         all_same_type = false;
   }

   if (all_same_type && syncs[0]->type->wait_all) {
      return syncs[0]->type->wait_all(device, sync_count, syncs, wait_values,
                                      wait_type, abs_timeout_ns);
   } else {
      for (uint32_t i = 0; i < sync_count; i++) {
         uint64_t wait_value = wait_values ? wait_values[i] : 0;
         VkResult result = vk_sync_wait(device, syncs[i], wait_value,
                                        wait_type, abs_timeout_ns);
         if (result != VK_SUCCESS)
            return result;
      }
      return VK_SUCCESS;
   }
}

VkResult
vk_sync_wait_any(struct vk_device *device,
                 uint32_t sync_count,
                 struct vk_sync **const syncs,
                 uint64_t *wait_values,
                 enum vk_sync_wait_type wait_type,
                 uint64_t abs_timeout_ns)
{
   if (sync_count == 0)
      return VK_SUCCESS;

   if (sync_count == 1) {
      return vk_sync_wait(device, syncs[0], wait_values[0],
                          wait_type, abs_timeout_ns);
   }

   bool all_same_type = true;
   for (uint32_t i = 0; i < sync_count; i++) {
      assert(!syncs[i]->type->is_timeline || wait_values);
      if (syncs[i]->type != syncs[0]->type)
         all_same_type = false;
   }

   if (syncs[0]->type->wait_any && all_same_type) {
      return syncs[0]->type->wait_any(device, sync_count, syncs, wait_values,
                                      wait_type, abs_timeout_ns);
   } else {
      /* If we have multiple syncs and they don't support wait_any or they're
       * not all the same type, there's nothing better we can do than spin.
       */
      do {
         for (uint32_t i = 0; i < sync_count; i++) {
            uint64_t wait_value = wait_values ? wait_values[i] : 0;
            VkResult result = vk_sync_wait(device, syncs[i], wait_value,
                                           wait_type, 0 /* abs_timeout_ns */);
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
import_sync_file(struct vk_device *device,
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
export_sync_file(struct vk_device *device,
                 struct vk_sync *sync,
                 int *sync_file)
{
   assert(!sync->type->is_timeline);

   return sync->type->export_sync_file(device, sync, sync_file);
}

VkResult vk_sync_move(struct vk_device *device,
                      struct vk_sync *dst,
                      struct vk_sync *src)
{
   if (dst->type == src->type && dst->type->move) {
      return dst->type->move(device, dst, src);
   } else if (dst->type->import_sync_file && src->type->export_sync_file) {
      int fd;
      VkResult result = src->type->export_sync_file(device, src, &fd);
      if (result != VK_SUCCESS)
         return result;

      result = dst->type->import_sync_file(device, dst, fd);
      if (fd >= 0)
         close(fd);
      if (result != VK_SUCCESS)
         return result;

      return src->type->reset(device, src);
   }

   unreachable("Unsupported vk_sync move");
}
