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

#include "vk_alloc.h"
#include "vk_command_buffer.h"
#include "vk_common_entrypoints.h"
#include "vk_device.h"
#include "vk_fence.h"
#include "vk_log.h"
#include "vk_semaphore.h"
#include "vk_sync.h"
#include "vk_timeline.h"

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
   vk_queue_disable_submit_thread(queue);
   util_dynarray_fini(&queue->labels);
   list_del(&queue->link);
   vk_object_base_finish(&queue->base);
}

static void
vk_queue_submit_destroy(struct vk_queue *queue,
                        struct vk_queue_submit *submit)
{
   for (uint32_t i = 0; i < submit->temp_sync_count; i++) {
      if (submit->temp_syncs[i] != NULL)
         vk_sync_destroy(queue->base.device, submit->temp_syncs[i]);
   }

   for (uint32_t i = 0; i < submit->wait_count; i++) {
      if (unlikely(submit->wait_points[i] != NULL))
         vk_timeline_point_release(queue->base.device, submit->wait_points[i]);
   }

   for (uint32_t i = 0; i < submit->signal_count; i++) {
      if (unlikely(submit->signal_points[i] != NULL))
         vk_timeline_point_free(queue->base.device, submit->signal_points[i]);
   }

   vk_free(&queue->base.device->alloc, submit);
}

static VkResult
vk_queue_submit_final(struct vk_queue *queue,
                      struct vk_queue_submit *submit)
{
   VkResult result;

   for (uint32_t i = 0; i < submit->wait_count; i++) {
      struct vk_timeline *timeline = vk_sync_as_timeline(submit->waits[i].sync);
      if (timeline) {
         result = vk_timeline_get_point(queue->base.device, timeline,
                                        submit->waits[i].wait_value,
                                        &submit->wait_points[i]);
         if (unlikely(result != VK_SUCCESS))
            goto fail;

         submit->waits[i].sync = &submit->wait_points[i]->sync;
      }
   }

   result = queue->submit(queue, submit);
   if (unlikely(result != VK_SUCCESS))
      goto fail;

   for (uint32_t i = 0; i < submit->signal_count; i++) {
      if (submit->signal_points[i] == NULL)
         continue;

      vk_timeline_point_install(queue->base.device,
                                submit->signal_points[i],
                                submit->signals[i].signal_value);
      submit->signal_points[i] = NULL;
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
   VkResult result;

   mtx_lock(&queue->threaded.mutex);

   while (1) {
      if (list_is_empty(&queue->threaded.submits)) {
         int ret = cnd_wait(&queue->threaded.cond, &queue->threaded.mutex);
         if (ret == thrd_error) {
            mtx_unlock(&queue->threaded.mutex);
            vk_queue_set_lost(queue, "cnd_wait failed");
            return 1;
         }
         continue;
      }

      struct vk_queue_submit *submit =
         list_first_entry(&queue->threaded.submits,
                          struct vk_queue_submit, link);
      list_del(&submit->link);

      /* Drop the lock while we wait */
      mtx_unlock(&queue->threaded.mutex);

      result = vk_sync_wait_all(queue->base.device,
                                submit->wait_count, submit->waits,
                                VK_SYNC_WAIT_PENDING, UINT64_MAX);
      if (result != VK_SUCCESS) {
         vk_queue_set_lost(queue, "Wait for time points failed");
         return 1;
      }

      result = vk_queue_submit_final(queue, submit);
      vk_queue_submit_destroy(queue, submit);
      if (result != VK_SUCCESS) {
         vk_queue_set_lost(queue, "queue::submit failed");
         return 1;
      }

      mtx_lock(&queue->threaded.mutex);
   }
}

static bool
vk_queue_has_submit_thread(struct vk_queue *queue)
{
   return queue->threaded.has_thread;
}

static VkResult
vk_queue_enable_submit_thread(struct vk_queue *queue)
{
   int ret;

   ret = mtx_init(&queue->threaded.mutex, mtx_plain);
   if (ret == thrd_error)
      return vk_errorf(queue, VK_ERROR_UNKNOWN, "mtx_init failed");

   ret = cnd_init(&queue->threaded.cond);
   if (ret == thrd_error) {
      mtx_destroy(&queue->threaded.mutex);
      return vk_errorf(queue, VK_ERROR_UNKNOWN, "cnd_init failed");
   }

   ret = thrd_create(&queue->threaded.thread,
                     vk_queue_submit_thread_func,
                     queue);
   if (ret == thrd_error) {
      cnd_destroy(&queue->threaded.cond);
      mtx_destroy(&queue->threaded.mutex);
      return vk_errorf(queue, VK_ERROR_UNKNOWN, "thrd_create failed");
   }

   queue->threaded.has_thread = true;

   return VK_SUCCESS;
}

static void
vk_queue_disable_submit_thread(struct vk_queue *queue)
{
   thrd_join(queue->threaded.thread, NULL);
   cnd_destroy(&queue->threaded.cond);
   mtx_destroy(&queue->threaded.mutex);
}

static VkResult
vk_queue_submit(struct vk_queue *queue,
                const VkSubmitInfo2KHR *info,
                struct vk_fence *fence)
{
   VkResult result;

   const uint32_t temp_sync_count = info->waitSemaphoreInfoCount;
   const uint32_t wait_count = info->waitSemaphoreInfoCount;
   const uint32_t command_buffer_count = info->commandBufferInfoCount;
   const uint32_t signal_count = info->signalSemaphoreInfoCount +
                                 (fence != NULL);

   VK_MULTIALLOC(ma);
   VK_MULTIALLOC_DECL(&ma, struct vk_queue_submit, submit, 1);
   VK_MULTIALLOC_DECL(&ma, struct vk_sync *, temp_syncs, temp_sync_count);
   VK_MULTIALLOC_DECL(&ma, struct vk_sync_wait, waits, wait_count);
   VK_MULTIALLOC_DECL(&ma, struct vk_timeline_point *, wait_points, wait_count);
   VK_MULTIALLOC_DECL(&ma, struct vk_command_buffer *, command_buffers,
                      command_buffer_count);
   VK_MULTIALLOC_DECL(&ma, struct vk_sync_signal, signals, signal_count);
   VK_MULTIALLOC_DECL(&ma, struct vk_timeline_point *, signal_points,
                      signal_count);

   if (!vk_multialloc_zalloc(&ma, &queue->base.device->alloc,
                             VK_SYSTEM_ALLOCATION_SCOPE_DEVICE))
      return vk_error(queue, VK_ERROR_OUT_OF_HOST_MEMORY);

   submit->temp_sync_count       = temp_sync_count;
   submit->wait_count            = wait_count;
   submit->command_buffer_count  = command_buffer_count;
   submit->signal_count          = signal_count;

   submit->temp_syncs      = temp_syncs;
   submit->waits           = waits;
   submit->wait_points     = wait_points;
   submit->command_buffers = command_buffers;
   submit->signals         = signals;
   submit->signal_points   = signal_points;

   for (uint32_t i = 0; i < info->waitSemaphoreInfoCount; i++) {
      VK_FROM_HANDLE(vk_semaphore, semaphore,
                     info->pWaitSemaphoreInfos[i].semaphore);

      waits[i] = (struct vk_sync_wait) {
         .sync = vk_semaphore_get_active_sync(semaphore),
         .stage_mask = info->pWaitSemaphoreInfos[i].stageMask,
         .wait_value = info->pWaitSemaphoreInfos[i].value,
      };
   }

   if (!vk_queue_has_submit_thread(queue)) {
      result = vk_sync_wait_all(queue->base.device,
                                wait_count, waits,
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
            assert(waits[i].sync == semaphore->temporary);
            temp_syncs[i] = semaphore->temporary;
            semaphore->temporary = NULL;
         } else {
            assert(waits[i].sync == &semaphore->permanent);

            result = vk_sync_create(queue->base.device,
                                    semaphore->permanent.type,
                                    0 /* initial value */,
                                    &temp_syncs[i]);
            if (unlikely(result != VK_SUCCESS))
               goto fail;

            result = vk_sync_move(queue->base.device, temp_syncs[i],
                                  &semaphore->permanent);
            if (unlikely(result != VK_SUCCESS))
               goto fail;

            waits[i].sync = temp_syncs[i];
         }
      }
   }

   for (uint32_t i = 0; i < info->commandBufferInfoCount; i++) {
      VK_FROM_HANDLE(vk_command_buffer, cmd_buffer,
                     info->pCommandBufferInfos[i].commandBuffer);
      assert(info->pCommandBufferInfos[i].deviceMask == 0);
      command_buffers[i] = cmd_buffer;
   }

   for (uint32_t i = 0; i < info->signalSemaphoreInfoCount; i++) {
      VK_FROM_HANDLE(vk_semaphore, semaphore,
                     info->pSignalSemaphoreInfos[i].semaphore);

      signals[i] = (struct vk_sync_signal) {
         .sync = vk_semaphore_get_active_sync(semaphore),
         .stage_mask = info->pSignalSemaphoreInfos[i].stageMask,
         .signal_value = info->pSignalSemaphoreInfos[i].value,
      };

      struct vk_timeline *timeline = vk_sync_as_timeline(signals[i].sync);
      if (timeline) {
         /* Allocate the point now so we don't have to later */
         result = vk_timeline_alloc_point(queue->base.device, timeline,
                                          &signal_points[i]);
         if (unlikely(result != VK_SUCCESS))
            goto fail;

         signals[i].sync = &signal_points[i]->sync;
      }
   }

   if (fence != NULL) {
      assert(signals[signal_count - 1].sync == NULL);
      signals[signal_count - 1] = (struct vk_sync_signal) {
         .sync = vk_fence_get_active_sync(fence),
         .stage_mask = ~(VkPipelineStageFlags2KHR)0,
      };
   }

   if (vk_queue_has_submit_thread(queue)) {
      mtx_lock(&queue->threaded.mutex);
      list_addtail(&submit->link, &queue->threaded.submits);
      cnd_signal(&queue->threaded.cond);
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

   if (vk_queue_is_lost(queue))
      return VK_ERROR_DEVICE_LOST;

   for (uint32_t i = 0; i < submitCount; i++) {
      VkResult result = vk_queue_submit(queue, &pSubmits[i],
                                        i == submitCount - 1 ? fence : NULL);
      if (unlikely(result != VK_SUCCESS))
         return result;
   }

   return VK_SUCCESS;
}
