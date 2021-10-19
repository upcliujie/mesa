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

#include "vk_queue.h"

#include "util/debug.h"

#include "vk_alloc.h"
#include "vk_command_buffer.h"
#include "vk_common_entrypoints.h"
#include "vk_device.h"
#include "vk_fence.h"
#include "vk_log.h"
#include "vk_physical_device.h"
#include "vk_semaphore.h"
#include "vk_sync.h"
#include "vk_timeline.h"
#include "vk_util.h"

VkResult
vk_queue_init(struct vk_queue *queue, struct vk_device *device,
              const VkDeviceQueueCreateInfo *pCreateInfo,
              uint32_t index_in_family)
{
   memset(queue, 0, sizeof(*queue));
   vk_object_base_init(device, &queue->base, VK_OBJECT_TYPE_QUEUE);

   list_addtail(&queue->link, &device->queues);

   queue->flags = pCreateInfo->flags;
   queue->queue_family_index = pCreateInfo->queueFamilyIndex;

   assert(index_in_family < pCreateInfo->queueCount);
   queue->index_in_family = index_in_family;

   util_dynarray_init(&queue->labels, NULL);
   queue->region_begin = true;

   return VK_SUCCESS;
}

static void vk_queue_disable_submit_thread(struct vk_queue *queue);

void
vk_queue_finish(struct vk_queue *queue)
{
   if (vk_queue_has_submit_thread(queue))
      vk_queue_disable_submit_thread(queue);
   util_dynarray_fini(&queue->labels);
   list_del(&queue->link);
   vk_object_base_finish(&queue->base);
}

VkResult
_vk_queue_set_lost(struct vk_queue *queue,
                   const char *file, int line,
                   const char *msg, ...)
{
   if (queue->_lost.lost)
      return VK_ERROR_DEVICE_LOST;

   queue->_lost.lost = true;
   queue->_lost.error_file = file;
   queue->_lost.error_line = line;

   va_list ap;
   va_start(ap, msg);
   vsnprintf(queue->_lost.error_msg, sizeof(queue->_lost.error_msg), msg, ap);
   va_end(ap);

   p_atomic_inc(&queue->base.device->_lost.lost);

   if (env_var_as_boolean("VK_ABORT_ON_DEVICE_LOSS", false))
      abort();

   return VK_ERROR_DEVICE_LOST;
}

static struct vk_queue_submit *
vk_queue_submit_alloc(struct vk_queue *queue,
                      uint32_t wait_count,
                      uint32_t command_buffer_count,
                      uint32_t signal_count)
{
   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, struct vk_queue_submit, submit, 1);
   VK_MULTIALLOC_DECL(&ma, struct vk_sync_wait, waits, wait_count);
   VK_MULTIALLOC_DECL(&ma, struct vk_command_buffer *, command_buffers,
                      command_buffer_count);
   VK_MULTIALLOC_DECL(&ma, struct vk_sync_signal, signals, signal_count);
   VK_MULTIALLOC_DECL(&ma, struct vk_sync *, wait_temps, wait_count);
   VK_MULTIALLOC_DECL(&ma, struct vk_timeline_point *, wait_points, wait_count);
   VK_MULTIALLOC_DECL(&ma, struct vk_timeline_point *, signal_points, signal_count);

   if (!vk_multialloc_zalloc(&ma, &queue->base.device->alloc,
                             VK_SYSTEM_ALLOCATION_SCOPE_DEVICE))
      return NULL;

   submit->wait_count            = wait_count;
   submit->command_buffer_count  = command_buffer_count;
   submit->signal_count          = signal_count;

   submit->waits           = waits;
   submit->command_buffers = command_buffers;
   submit->signals         = signals;
   submit->_wait_temps     = wait_temps;
   submit->_wait_points    = wait_points;
   submit->_signal_points  = signal_points;

   return submit;
}

static void
vk_queue_submit_destroy(struct vk_queue *queue,
                        struct vk_queue_submit *submit)
{
   for (uint32_t i = 0; i < submit->wait_count; i++) {
      if (submit->_wait_temps[i] != NULL)
         vk_sync_destroy(queue->base.device, submit->_wait_temps[i]);
   }

   for (uint32_t i = 0; i < submit->wait_count; i++) {
      if (unlikely(submit->_wait_points[i] != NULL))
         vk_timeline_point_release(queue->base.device, submit->_wait_points[i]);
   }

   for (uint32_t i = 0; i < submit->signal_count; i++) {
      if (unlikely(submit->_signal_points[i] != NULL))
         vk_timeline_point_free(queue->base.device, submit->_signal_points[i]);
   }

   vk_free(&queue->base.device->alloc, submit);
}

static VkResult
vk_queue_submit_final(struct vk_queue *queue,
                      struct vk_queue_submit *submit)
{
   VkResult result;

   /* Now that we know all our time points exist, fetch the time point syncs
    * from any vk_timelines.  While we're here, also compact down the list of
    * waits to get rid of any trivial timeline waits.
    */
   uint32_t wait_count = 0;
   for (uint32_t i = 0; i < submit->wait_count; i++) {
      /* A wait on 0 is always a no-op */
      if (submit->waits[i].sync->type->is_timeline &&
          submit->waits[i].wait_value == 0)
         continue;

      struct vk_timeline *timeline = vk_sync_as_timeline(submit->waits[i].sync);
      if (timeline) {
         result = vk_timeline_get_point(queue->base.device, timeline,
                                        submit->waits[i].wait_value,
                                        &submit->_wait_points[i]);
         if (unlikely(result != VK_SUCCESS))
            goto fail;

         /* This can happen if the point is long past */
         if (submit->_wait_points[i] == NULL)
            continue;

         submit->waits[i].sync = &submit->_wait_points[i]->sync;
      }

      assert(wait_count <= i);
      if (wait_count < i)
         submit->waits[wait_count] = submit->waits[i];
      wait_count++;
   }

   assert(wait_count <= submit->wait_count);
   submit->wait_count = wait_count;

   result = queue->submit(queue, submit);
   if (unlikely(result != VK_SUCCESS))
      goto fail;

   for (uint32_t i = 0; i < submit->signal_count; i++) {
      if (submit->_signal_points[i] == NULL)
         continue;

      vk_timeline_point_install(queue->base.device,
                                submit->_signal_points[i],
                                submit->signals[i].signal_value);
      submit->_signal_points[i] = NULL;
   }

   return VK_SUCCESS;

fail:
   vk_queue_submit_destroy(queue, submit);
   return result;
}

static int
vk_queue_submit_thread_func(void *_data)
{
   struct vk_queue *queue = _data;

   mtx_lock(&queue->threaded.mutex);

   while (queue->threaded.run) {
      if (list_is_empty(&queue->threaded.submits)) {
         int ret = cnd_wait(&queue->threaded.push, &queue->threaded.mutex);
         if (ret == thrd_error) {
            vk_queue_set_lost(queue, "cnd_wait failed");
            return 1;
         }
         continue;
      }

      struct vk_queue_submit *submit =
         list_first_entry(&queue->threaded.submits,
                          struct vk_queue_submit, link);

      list_del(&submit->link);
      cnd_broadcast(&queue->threaded.pop);

      /* Drop the lock while we wait */
      mtx_unlock(&queue->threaded.mutex);

      VkResult wait_result, submit_result = VK_SUCCESS;
      wait_result = vk_sync_wait_all(queue->base.device,
                                     submit->wait_count, submit->waits,
                                     VK_SYNC_WAIT_PENDING, UINT64_MAX);
      if (likely(wait_result == VK_SUCCESS))
         submit_result = vk_queue_submit_final(queue, submit);

      vk_queue_submit_destroy(queue, submit);

      mtx_lock(&queue->threaded.mutex);

      if (unlikely(wait_result != VK_SUCCESS)) {
         vk_queue_set_lost(queue, "Wait for time points failed");
         return 1;
      }

      if (unlikely(submit_result != VK_SUCCESS)) {
         vk_queue_set_lost(queue, "queue::submit failed");
         return 1;
      }

   }

   return 0;
}

static VkResult
vk_queue_drain(struct vk_queue *queue)
{
   if (!vk_queue_has_submit_thread(queue))
      return VK_SUCCESS;

   VkResult result = VK_SUCCESS;

   mtx_lock(&queue->threaded.mutex);
   while (!list_is_empty(&queue->threaded.submits)) {
      if (vk_device_is_lost(queue->base.device)) {
         result = VK_ERROR_DEVICE_LOST;
         break;
      }

      int ret = cnd_wait(&queue->threaded.pop, &queue->threaded.mutex);
      if (ret == thrd_error) {
         result = vk_queue_set_lost(queue, "cnd_wait failed");
         break;
      }
   }
   mtx_unlock(&queue->threaded.mutex);

   return result;
}

static VkResult
vk_queue_enable_submit_thread(struct vk_queue *queue)
{
   VkResult result;
   int ret;

   list_inithead(&queue->threaded.submits);

   ret = mtx_init(&queue->threaded.mutex, mtx_plain);
   if (ret == thrd_error) {
      result = vk_errorf(queue, VK_ERROR_UNKNOWN, "mtx_init failed");
      goto fail_mutex;
   }

   ret = cnd_init(&queue->threaded.push);
   if (ret == thrd_error) {
      result = vk_errorf(queue, VK_ERROR_UNKNOWN, "cnd_init failed");
      goto fail_push;
   }

   ret = cnd_init(&queue->threaded.pop);
   if (ret == thrd_error) {
      result = vk_errorf(queue, VK_ERROR_UNKNOWN, "cnd_init failed");
      goto fail_pop;
   }

   queue->threaded.run = true;

   ret = thrd_create(&queue->threaded.thread,
                     vk_queue_submit_thread_func,
                     queue);
   if (ret == thrd_error) {
      result = vk_errorf(queue, VK_ERROR_UNKNOWN, "thrd_create failed");
      goto fail_thread;
   }

   queue->threaded.has_thread = true;

   return VK_SUCCESS;

fail_thread:
   cnd_destroy(&queue->threaded.pop);
fail_pop:
   cnd_destroy(&queue->threaded.push);
fail_push:
   mtx_destroy(&queue->threaded.mutex);
fail_mutex:
   return result;
}

static void
vk_queue_disable_submit_thread(struct vk_queue *queue)
{
   vk_queue_drain(queue);

   /* Kick the thread to disable it */
   mtx_lock(&queue->threaded.mutex);
   queue->threaded.run = false;
   cnd_signal(&queue->threaded.push);
   mtx_unlock(&queue->threaded.mutex);

   thrd_join(queue->threaded.thread, NULL);

   cnd_destroy(&queue->threaded.pop);
   cnd_destroy(&queue->threaded.push);
   mtx_destroy(&queue->threaded.mutex);

   if (!list_is_empty(&queue->threaded.submits))
      assert(vk_device_is_lost_no_report(queue->base.device));

   while (!list_is_empty(&queue->threaded.submits)) {
      assert(vk_device_is_lost_no_report(queue->base.device));

      struct vk_queue_submit *submit =
         list_first_entry(&queue->threaded.submits,
                          struct vk_queue_submit, link);

      list_del(&submit->link);
      vk_queue_submit_destroy(queue, submit);
   }
}

static VkResult
vk_queue_submit(struct vk_queue *queue,
                const VkSubmitInfo2KHR *info,
                struct vk_fence *fence)
{
   VkResult result;

   struct vk_queue_submit *submit =
      vk_queue_submit_alloc(queue, info->waitSemaphoreInfoCount,
                            info->commandBufferInfoCount,
                            info->signalSemaphoreInfoCount + (fence != NULL));
   if (unlikely(submit == NULL))
      return vk_error(queue, VK_ERROR_OUT_OF_HOST_MEMORY);

   const VkPerformanceQuerySubmitInfoKHR *perf_info =
      vk_find_struct_const(info->pNext, PERFORMANCE_QUERY_SUBMIT_INFO_KHR);
   submit->perf_pass_index = perf_info ? perf_info->counterPassIndex : 0;

   for (uint32_t i = 0; i < info->waitSemaphoreInfoCount; i++) {
      VK_FROM_HANDLE(vk_semaphore, semaphore,
                     info->pWaitSemaphoreInfos[i].semaphore);

      submit->waits[i] = (struct vk_sync_wait) {
         .sync = vk_semaphore_get_active_sync(semaphore),
         .stage_mask = info->pWaitSemaphoreInfos[i].stageMask,
         .wait_value = info->pWaitSemaphoreInfos[i].value,
      };
   }

   if (!vk_queue_has_submit_thread(queue)) {
      result = vk_sync_wait_all(queue->base.device,
                                submit->wait_count, submit->waits,
                                VK_SYNC_WAIT_PENDING, 0);
      if (result == VK_TIMEOUT)
         result = vk_queue_enable_submit_thread(queue);

      if (unlikely(result != VK_SUCCESS))
         goto fail;
   }

   if (vk_queue_has_submit_thread(queue)) {
      for (uint32_t i = 0; i < info->waitSemaphoreInfoCount; i++) {
         VK_FROM_HANDLE(vk_semaphore, semaphore,
                        info->pWaitSemaphoreInfos[i].semaphore);

         if (semaphore->type != VK_SEMAPHORE_TYPE_BINARY)
            continue;

         /* For threaded submit, we need to steal any binary semaphore
          * payloads here.  Otherwise, we may run into issues with the
          * wait-for-submit above in a future submit where it may not
          * wait for the right time point.
          */
         if (semaphore->temporary) {
            assert(submit->waits[i].sync == semaphore->temporary);
            submit->_wait_temps[i] = semaphore->temporary;
            semaphore->temporary = NULL;
         } else {
            assert(submit->waits[i].sync == &semaphore->permanent);

            result = vk_sync_create(queue->base.device,
                                    semaphore->permanent.type,
                                    0 /* initial value */,
                                    &submit->_wait_temps[i]);
            if (unlikely(result != VK_SUCCESS))
               goto fail;

            result = vk_sync_move(queue->base.device,
                                  submit->_wait_temps[i],
                                  &semaphore->permanent);
            if (unlikely(result != VK_SUCCESS))
               goto fail;

            submit->waits[i].sync = submit->_wait_temps[i];
         }
      }
   }

   for (uint32_t i = 0; i < info->commandBufferInfoCount; i++) {
      VK_FROM_HANDLE(vk_command_buffer, cmd_buffer,
                     info->pCommandBufferInfos[i].commandBuffer);
      assert(info->pCommandBufferInfos[i].deviceMask == 0);
      submit->command_buffers[i] = cmd_buffer;
   }

   for (uint32_t i = 0; i < info->signalSemaphoreInfoCount; i++) {
      VK_FROM_HANDLE(vk_semaphore, semaphore,
                     info->pSignalSemaphoreInfos[i].semaphore);

      submit->signals[i] = (struct vk_sync_signal) {
         .sync = vk_semaphore_get_active_sync(semaphore),
         .stage_mask = info->pSignalSemaphoreInfos[i].stageMask,
         .signal_value = info->pSignalSemaphoreInfos[i].value,
      };

      if (submit->signals[i].sync->type->is_timeline &&
          submit->signals[i].signal_value == 0) {
         result = vk_queue_set_lost(queue,
            "Tried to signal a timeline with value 0");
         goto fail;
      }

      struct vk_timeline *timeline =
         vk_sync_as_timeline(submit->signals[i].sync);
      if (timeline) {
         /* Allocate the point now so we don't have to later */
         result = vk_timeline_alloc_point(queue->base.device, timeline,
                                          &submit->_signal_points[i]);
         if (unlikely(result != VK_SUCCESS))
            goto fail;

         submit->signals[i].sync = &submit->_signal_points[i]->sync;
      }
   }

   if (fence != NULL) {
      uint32_t fence_idx = info->signalSemaphoreInfoCount;
      assert(submit->signal_count == fence_idx + 1);
      assert(submit->signals[fence_idx].sync == NULL);
      submit->signals[fence_idx] = (struct vk_sync_signal) {
         .sync = vk_fence_get_active_sync(fence),
         .stage_mask = ~(VkPipelineStageFlags2KHR)0,
      };
   }

   if (vk_queue_has_submit_thread(queue)) {
      mtx_lock(&queue->threaded.mutex);
      list_addtail(&submit->link, &queue->threaded.submits);
      cnd_signal(&queue->threaded.push);
      mtx_unlock(&queue->threaded.mutex);
      return VK_SUCCESS;
   } else {
      result = vk_queue_submit_final(queue, submit);
      vk_queue_submit_destroy(queue, submit);
      return result;
   }

fail:
   vk_queue_submit_destroy(queue, submit);
   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_QueueSubmit2KHR(VkQueue _queue,
                          uint32_t submitCount,
                          const VkSubmitInfo2KHR *pSubmits,
                          VkFence _fence)
{
   VK_FROM_HANDLE(vk_queue, queue, _queue);
   VK_FROM_HANDLE(vk_fence, fence, _fence);

   if (vk_device_is_lost(queue->base.device))
      return VK_ERROR_DEVICE_LOST;

   for (uint32_t i = 0; i < submitCount; i++) {
      VkResult result = vk_queue_submit(queue, &pSubmits[i],
                                        i == submitCount - 1 ? fence : NULL);
      if (unlikely(result != VK_SUCCESS))
         return result;
   }

   return VK_SUCCESS;
}

static const struct vk_sync_type *
get_cpu_wait_type(struct vk_physical_device *pdevice)
{
   const struct vk_sync_type *const *t;
   for (t = pdevice->supported_sync_types; *t; t++) {
      if (vk_sync_type_is_vk_timeline(*t))
         continue;

      if (vk_sync_type_has_cpu_wait(*t))
         return *t;
   }

   unreachable("You must have a non-vk_timeline CPU wait sync type");
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_QueueWaitIdle(VkQueue _queue)
{
   VK_FROM_HANDLE(vk_queue, queue, _queue);

   if (vk_device_is_lost(queue->base.device))
      return VK_ERROR_DEVICE_LOST;

   VkResult result = vk_queue_drain(queue);
   if (unlikely(result != VK_SUCCESS))
      return result;

   const struct vk_sync_type *sync_type =
      get_cpu_wait_type(queue->base.device->physical);

   struct vk_sync *sync;
   result = vk_sync_create(queue->base.device, sync_type, 0, &sync);
   if (unlikely(result != VK_SUCCESS))
      return result;

   struct vk_sync_signal signal = {
      .sync = sync,
      .stage_mask = ~(VkPipelineStageFlags2KHR)0,
      .signal_value = 1,
   };

   struct vk_queue_submit submit = {
      .signal_count = 1,
      .signals = &signal,
   };

   result = queue->submit(queue, &submit);
   if (likely(result == VK_SUCCESS)) {
      result = vk_sync_wait(queue->base.device, sync, 1,
                            VK_SYNC_WAIT_COMPLETE, UINT64_MAX);
   }

   vk_sync_destroy(queue->base.device, sync);

   return result;
}
