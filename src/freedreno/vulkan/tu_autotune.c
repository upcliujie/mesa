/*
 * Copyright © 2021 Igalia S.L.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <vulkan/vulkan_core.h>

#include "tu_autotune.h"
#include "tu_private.h"
#include "tu_cs.h"

/* In Vulkan application may fill command buffer from many threads
 * and expect no locking to occur. We do introduce the possibility of
 * locking on renderpass end, however assuming that application
 * doesn't have a huge amount of slightly different renderpasses,
 * there would be minimal to none contention.
 *
 * Other assumptions are:
 * - Application doesn't create one-time-submit command buffers to
 *   hold them indefinitely without submission.
 * - Application does submit command buffers soon after their creation.
 *
 * Breaking the above may lead to some decrease in performance or
 * autotuner turning itself off.
 */

#define TU_AUTOTUNE_DEBUG_LOG 0
#define TU_AUTOTUNE_LOG_AT_FINISH 0

#define MAX_HISTORY_RESULTS 5
#define MAX_HISTORY_LIFETIME 128

/**
 * Tracks results for a given renderpass key
 *
 * ralloc parent is fd_autotune::ht
 */
struct tu_renderpass_history {
   uint64_t key;

   /* We would delete old history entries */
   uint32_t last_fence;

   /* We cannot delete history entry with
    * unsubmitted results.
    */
   uint32_t unsubmitted_results;

   /**
    * List of recent fd_renderpass_result's
    */
   struct list_head results;
   uint32_t num_results;

   uint32_t avg_samples;
};

struct tu_autotune_cs {
   struct list_head node;
   struct tu_cs cs;
   uint32_t fence;
};

#define APPEND_TO_HASH(state, field) \
   XXH64_update(state, &field, sizeof(field));

static uint64_t
hash_renderpass_instance(const struct tu_render_pass *pass,
                         const struct tu_framebuffer *framebuffer) {
   XXH64_state_t hash_state;
   XXH64_reset(&hash_state, 0);

   APPEND_TO_HASH(&hash_state, framebuffer->width);
   APPEND_TO_HASH(&hash_state, framebuffer->height);
   APPEND_TO_HASH(&hash_state, framebuffer->layers);
   APPEND_TO_HASH(&hash_state, framebuffer->attachment_count);
   for (unsigned i = 0; i < framebuffer->attachment_count; i++) {
      APPEND_TO_HASH(&hash_state, framebuffer->attachments[i].attachment->extent);
      APPEND_TO_HASH(&hash_state, framebuffer->attachments[i].attachment->image->vk_format);
      APPEND_TO_HASH(&hash_state, framebuffer->attachments[i].attachment->image->layer_count);
      APPEND_TO_HASH(&hash_state, framebuffer->attachments[i].attachment->image->level_count);
   }

   APPEND_TO_HASH(&hash_state, pass->attachment_count);
   XXH64_update(&hash_state, pass->attachments, pass->attachment_count * sizeof(pass->attachments[0]));
   APPEND_TO_HASH(&hash_state, pass->subpass_count);
   for (unsigned i = 0; i < pass->subpass_count; i++) {
      APPEND_TO_HASH(&hash_state, pass->subpasses[i].samples);
      APPEND_TO_HASH(&hash_state, pass->subpasses[i].input_count);
      APPEND_TO_HASH(&hash_state, pass->subpasses[i].color_count);
      APPEND_TO_HASH(&hash_state, pass->subpasses[i].resolve_count);
   }

   return XXH64_digest(&hash_state);
}

static void
history_destructor(void *h)
{
   struct tu_renderpass_history *history = h;

   list_for_each_entry_safe(struct tu_renderpass_result, result,
                            &history->results, node) {
      ralloc_free(result);
   }
}

static struct tu_renderpass_history *
get_history(struct tu_autotune *at, uint64_t rp_key)
{
   struct tu_renderpass_history *history;

   u_rwlock_rdlock(&at->ht_lock);
   struct hash_entry *entry =
      _mesa_hash_table_search(at->ht, &rp_key);
   if (entry) {
      history = (struct tu_renderpass_history *) entry->data;
      p_atomic_inc(&history->unsubmitted_results);
   }
   u_rwlock_rdunlock(&at->ht_lock);

   if (entry) {
      return history;
   }

   /* The assumption is that we almost always find the entry,
    * so the insert with locks is a rare event.
    */

   history = rzalloc_size(NULL, sizeof(*history));
   ralloc_set_destructor(history, history_destructor);
   history->key = rp_key;
   list_inithead(&history->results);

   u_rwlock_wrlock(&at->ht_lock);
   /* We have to search again in case an entry sneaked between the locks */
   entry = _mesa_hash_table_search(at->ht, &rp_key);
   if (!entry) {
      _mesa_hash_table_insert(at->ht, &history->key, history);
   } else {
      ralloc_free(history);
      history = (struct tu_renderpass_history *) entry->data;
   }
   p_atomic_inc(&history->unsubmitted_results);
   u_rwlock_wrunlock(&at->ht_lock);

   return history;
}

static void
result_destructor(void *r)
{
   struct tu_renderpass_result *result = r;

   /* Just in case we manage to somehow still be on the pending_results list: */
   list_del(&result->node);
}

static struct tu_renderpass_result *
get_history_result(struct tu_autotune *at,
                     struct tu_renderpass_history *history)
{
   struct tu_renderpass_result *result = rzalloc_size(NULL, sizeof(*result));

   result->idx = p_atomic_inc_return(&at->idx_counter);
   result->history = history;

   ralloc_set_destructor(result, result_destructor);

   return result;
}

static void
history_add_result(struct tu_renderpass_history *history,
                      struct tu_renderpass_result *result)
{
   list_delinit(&result->node);
   list_add(&result->node, &history->results);

   if (history->num_results < MAX_HISTORY_RESULTS) {
      history->num_results++;
   } else {
      /* Once above the limit, start popping old results off the
       * tail of the list:
       */
      struct tu_renderpass_result *old_result =
         list_last_entry(&history->results, struct tu_renderpass_result, node);
      list_delinit(&old_result->node);
      ralloc_free(old_result);
   }

   /* Do calculations here to avoid locking history in tu_autotune_use_bypass */
   uint32_t total_samples = 0;
   list_for_each_entry(struct tu_renderpass_result, result,
                       &history->results, node) {
      total_samples += result->samples_passed;
   }

   float avg_samples = (float)total_samples / (float)history->num_results;
   p_atomic_set(&history->avg_samples, (uint32_t)avg_samples);
}

static void
process_results(struct tu_autotune *at)
{
   uint32_t current_fence = at->results->fence;

   uint32_t min_idx = ~0;
   uint32_t max_idx = 0;

   list_for_each_entry_safe(struct tu_renderpass_result, result,
                            &at->pending_results, node) {
      if (result->fence > current_fence)
         break;

      struct tu_renderpass_history *history = result->history;

      min_idx = MIN2(min_idx, result->idx);
      max_idx = MAX2(max_idx, result->idx);
      uint32_t idx = result->idx % ARRAY_SIZE(at->results->result);

      result->samples_passed = at->results->result[idx].samples_end -
                               at->results->result[idx].samples_start;

      history_add_result(history, result);
   }

   list_for_each_entry_safe(struct tu_autotune_cs, at_cs,
                            &at->pending_cs, node) {
      if (at_cs->fence > current_fence)
         break;

      list_del(&at_cs->node);
      tu_cs_finish(&at_cs->cs);
      free(at_cs);
   }

   if (max_idx - min_idx > TU_AUTOTUNE_MAX_RESULTS) {
      /* If results start to trample each other it's better to bail out */
      at->enabled = false;
      mesa_logw("disabling sysmem vs gmem autotuner because results "
                "are trampling each other");
   }
}

static bool
fallback_use_bypass(const struct tu_render_pass *pass,
                    const struct tu_framebuffer *framebuffer,
                    const struct tu_cmd_buffer *cmd_buffer)
{
   if ((cmd_buffer->state.drawcall_count > 5))
      return false;

   for (unsigned i = 0; i < pass->subpass_count; i++) {
      if (pass->subpasses[i].samples != VK_SAMPLE_COUNT_1_BIT)
         return false;
   }

   return true;
}

static struct tu_cs *
create_fence_cs(struct tu_device *dev, struct tu_autotune *at)
{
   struct tu_autotune_cs *at_cs = calloc(1, sizeof(struct tu_autotune_cs));
   at_cs->fence = at->fence_counter;

   tu_cs_init(&at_cs->cs, dev, TU_CS_MODE_GROW, 32);
   tu_cs_begin(&at_cs->cs);

   tu_cs_emit_pkt7(&at_cs->cs, CP_EVENT_WRITE, 4);
   tu_cs_emit(&at_cs->cs, CP_EVENT_WRITE_0_EVENT(CACHE_FLUSH_TS));
   tu_cs_emit_qw(&at_cs->cs, results_ptr(at, fence));
   tu_cs_emit(&at_cs->cs, at->fence_counter);

   tu_cs_end(&at_cs->cs);

   list_addtail(&at_cs->node, &at->pending_cs);

   return &at_cs->cs;
}

struct tu_cs *
tu_autotune_on_submit(struct tu_device *dev,
                      struct tu_autotune *at,
                      VkCommandBuffer *cmd_buffers,
                      uint32_t cmd_buffer_count)
{
   /* We are single-threaded here */

   process_results(at);

   /* pre-increment so zero isn't valid fence */
   uint32_t new_fence = ++at->fence_counter;

   for (uint32_t i = 0; i < cmd_buffer_count; i++) {
      TU_FROM_HANDLE(tu_cmd_buffer, cmdbuf, cmd_buffers[i]);
      list_for_each_entry(struct tu_renderpass_result, result,
                          &cmdbuf->renderpass_autotune_results, node) {
         result->fence = new_fence;
         result->history->last_fence = new_fence;
         p_atomic_dec(&result->history->unsubmitted_results);
      }

      if (!list_is_empty(&cmdbuf->renderpass_autotune_results)) {
         list_splicetail(&cmdbuf->renderpass_autotune_results,
                         &at->pending_results);
      }
   }

   /* Cleanup old entries from history table. The assumption
    * here is that application doesn't hold many old unsubmitted
    * command buffers, otherwise this table may grow big.
    */
   hash_table_foreach(at->ht, entry) {
      struct tu_renderpass_history *history = entry->data;
      if (history->last_fence == 0 || history->unsubmitted_results > 0 ||
          (new_fence - history->last_fence) <= MAX_HISTORY_LIFETIME)
         continue;

      bool free = false;
      u_rwlock_wrlock(&at->ht_lock);
      if (likely(history->unsubmitted_results == 0)) {
         _mesa_hash_table_remove_key(at->ht, &history->key);
         free = true;
      }
      u_rwlock_wrunlock(&at->ht_lock);

      if (free)
         ralloc_free(history);
   }

   return create_fence_cs(dev, at);
}

static bool
renderpass_key_equals(const void *_a, const void *_b)
{
   return *(uint64_t *)_a == *(uint64_t *)_b;
}

static uint32_t
renderpass_key_hash(const void *_a)
{
   return *((uint64_t *) _a) & 0xffffffff;
}

void
tu_autotune_init(struct tu_autotune *at, struct tu_device *dev)
{
   at->enabled = true;
   at->ht = _mesa_hash_table_create(NULL,
                                    renderpass_key_hash,
                                    renderpass_key_equals);
   u_rwlock_init(&at->ht_lock);

   at->results_bo = malloc(sizeof(struct tu_bo));
   tu_bo_init_new(dev, at->results_bo, sizeof(struct tu_autotune_results),
                  TU_BO_ALLOC_NO_FLAGS);
   tu_bo_map(dev, at->results_bo);

   at->results = at->results_bo->map;

   list_inithead(&at->pending_results);
   list_inithead(&at->pending_cs);
}

void
tu_autotune_fini(struct tu_autotune *at, struct tu_device *dev)
{
#if TU_AUTOTUNE_LOG_AT_FINISH != 0
   while (!list_is_empty(&at->pending_results)) {
      process_results(at);
   }

   hash_table_foreach(at->ht, entry) {
      struct tu_renderpass_history *history = entry->data;

      printf("%016"PRIx64" \tavg_passed=%u results=%u\n",
            history->key, history->avg_samples, history->num_results);
   }
#endif

   hash_table_foreach(at->ht, entry) {
      struct tu_renderpass_history *history = entry->data;
      ralloc_free(history);
   }

   list_for_each_entry_safe(struct tu_autotune_cs, at_cs,
                            &at->pending_cs, node) {
      tu_cs_finish(&at_cs->cs);
      free(at_cs);
   }

   _mesa_hash_table_destroy(at->ht, NULL);
   u_rwlock_destroy(&at->ht_lock);
   tu_bo_finish(dev, at->results_bo);
   free(at->results_bo);
}

bool
tu_autotune_submit_requires_fence(const struct VkSubmitInfo *submit_info)
{
   for (uint32_t i = 0; i < submit_info->commandBufferCount; i++) {
      TU_FROM_HANDLE(tu_cmd_buffer, cmdbuf, submit_info->pCommandBuffers[i]);
      if (!list_is_empty(&cmdbuf->renderpass_autotune_results))
         return true;
   }

   return false;
}

void
tu_autotune_free_results(struct list_head *results)
{
   list_for_each_entry_safe(struct tu_renderpass_result, result,
                            results, node) {
      p_atomic_dec(&result->history->unsubmitted_results);
      ralloc_free(result);
   }
}

bool
tu_autotune_use_bypass(struct tu_autotune *at,
                       struct tu_cmd_buffer *cmd_buffer,
                       struct tu_renderpass_result **autotune_result)
{
   const struct tu_render_pass *pass = cmd_buffer->state.pass;
   const struct tu_framebuffer *framebuffer = cmd_buffer->state.framebuffer;

   if (!at->enabled)
      return fallback_use_bypass(pass, framebuffer, cmd_buffer);

   uint64_t renderpass_key = hash_renderpass_instance(pass, framebuffer);

   /* We use 64bit hash as a key since we don't fear rare hash collision,
    * the worst that would happen is sysmem being selected when it should
    * have not, and with 64bit it would be extremely rare.
    *
    * Q: Why not make the key from framebuffer + renderpass pointers?
    * A: At least DXVK creates new framebuffers each frame while keeping
    *    renderpasses the same. Also we want to support replaying a single
    *    frame in a loop for testing.
    */
   struct tu_renderpass_history *history = get_history(at, renderpass_key);
   if (!history) {
      return fallback_use_bypass(pass, framebuffer, cmd_buffer);
   }

   *autotune_result = get_history_result(at, history);

   if (history->num_results > 0) {
      uint32_t avg_samples = p_atomic_read(&history->avg_samples);

      /* TODO we should account for load/stores/clears/resolves especially
       * with low drawcall count and ~fb_size samples passed, in D3D11 games
       * we are seeing many renderpasses like:
       *  - color attachment load
       *  - single fullscreen draw
       *  - color attachment store
       */

      /* Low sample count could mean there was only a clear.. or there was
       * a clear plus draws that touch no or few samples
       */
      if (avg_samples < 500) {
#if TU_AUTOTUNE_DEBUG_LOG != 0
         mesa_logi("%016"PRIx64":%u\t avg_samples=%u selecting sysmem\n",
            renderpass_key, cmd_buffer->state.drawcall_count, avg_samples);
#endif
         return true;
      }

      /* Cost-per-sample is an estimate for the average number of reads+
       * writes for a given passed sample.
       */
      float sample_cost = cmd_buffer->state.total_drawcalls_cost;
      sample_cost /= cmd_buffer->state.drawcall_count;

      float total_draw_cost = (avg_samples * sample_cost) / cmd_buffer->state.drawcall_count;

      bool select_sysmem = total_draw_cost < 6000.0;

#if TU_AUTOTUNE_DEBUG_LOG != 0
      mesa_logi("%016"PRIx64":%u\t avg_samples=%u, "
          "sample_cost=%f, total_draw_cost=%f selecting %s\n",
          renderpass_key, cmd_buffer->state.drawcall_count, avg_samples,
          sample_cost, total_draw_cost, select_sysmem ? "sysmem" : "gmem");
#endif

      return select_sysmem;
   }

   return fallback_use_bypass(pass, framebuffer, cmd_buffer);
}
