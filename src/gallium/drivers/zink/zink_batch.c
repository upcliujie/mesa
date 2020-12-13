#include "zink_batch.h"

#include "zink_context.h"
#include "zink_fence.h"
#include "zink_framebuffer.h"
#include "zink_query.h"
#include "zink_program.h"
#include "zink_render_pass.h"
#include "zink_resource.h"
#include "zink_screen.h"

#include "util/u_cpu_detect.h"
#include "util/hash_table.h"
#include "util/u_debug.h"
#include "util/set.h"


void
debug_describe_zink_batch_state(char *buf, const struct zink_batch_state *ptr)
{
   sprintf(buf, "zink_batch_state");
}

static void
batch_usage_unset(struct zink_batch_usage *u, uint32_t batch_id)
{
   p_atomic_cmpxchg(&u->usage, batch_id, 0);
}

void
zink_reset_batch_state(struct zink_context *ctx, struct zink_batch_state *bs)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);

   assert(bs->fence.completed | !bs->fence.submitted);
   zink_render_pass_reference(screen, &bs->rp, NULL);
   zink_framebuffer_reference(screen, &bs->fb, NULL);
   zink_fence_clear_resources(screen, &bs->fence);

   set_foreach(bs->active_queries, entry) {
      struct zink_query *query = (void*)entry->key;
      zink_prune_query(screen, query);
      _mesa_set_remove(bs->active_queries, entry);
   }

   set_foreach(bs->samplers, entry) {
      struct zink_sampler *sampler = (struct zink_sampler*)entry->key;
      batch_usage_unset(&sampler->batch_uses, bs->fence.batch_id);
      zink_sampler_reference(ctx, &sampler, NULL);
      _mesa_set_remove(bs->samplers, entry);
   }

   set_foreach(bs->surfaces, entry) {
      struct zink_surface *surf = (struct zink_surface *)entry->key;
      batch_usage_unset(&surf->batch_uses, bs->fence.batch_id);
      pipe_surface_reference((struct pipe_surface**)&surf, NULL);
      _mesa_set_remove(bs->surfaces, entry);
   }
   set_foreach(bs->bufferviews, entry) {
      struct zink_buffer_view *buffer_view = (struct zink_buffer_view *)entry->key;
      batch_usage_unset(&buffer_view->batch_uses, bs->fence.batch_id);
      zink_buffer_view_reference(ctx, &buffer_view, NULL);
      _mesa_set_remove(bs->bufferviews, entry);
   }

   set_foreach(bs->desc_sets, entry) {
      struct zink_descriptor_set *zds = (void*)entry->key;
      batch_usage_unset(&zds->batch_uses, bs->fence.batch_id);
      /* reset descriptor pools when no bs is using this program to avoid
       * having some inactive program hogging a billion descriptors
       */
      pipe_reference(&zds->reference, NULL);
      zink_descriptor_set_recycle(zds);
      _mesa_set_remove(bs->desc_sets, entry);
   }

   set_foreach(bs->programs, entry) {
      struct zink_program *pg = (struct zink_program*)entry->key;
      if (pg->is_compute) {
         struct zink_compute_program *comp = (struct zink_compute_program*)pg;
         bool in_use = comp == ctx->curr_compute;
         if (zink_compute_program_reference(screen, &comp, NULL) && in_use)
            ctx->curr_compute = NULL;
      } else {
         struct zink_gfx_program *prog = (struct zink_gfx_program*)pg;
         bool in_use = prog == ctx->curr_program;
         if (zink_gfx_program_reference(screen, &prog, NULL) && in_use)
            ctx->curr_program = NULL;
      }
      _mesa_set_remove(bs->programs, entry);
   }

   bs->descs_used = 0;
   ctx->resource_size -= bs->resource_size;
   bs->resource_size = 0;
   /* only reset submitted here so that tc fence desync can pick up the 'completed' flag
    * before the state is reused
    */
   bs->fence.submitted = false;
   bs->fence.batch_id = 0;
}

void
zink_clear_batch_state(struct zink_context *ctx, struct zink_batch_state *bs)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   bs->fence.completed = true;
   hash_table_foreach(bs->framebuffer_cache, entry) {
      struct zink_framebuffer* fb = (struct zink_framebuffer*)entry->data;
      zink_framebuffer_reference(screen, &fb, NULL);
      _mesa_hash_table_remove(bs->framebuffer_cache, entry);
   }
   zink_reset_batch_state(ctx, bs);
}

void
zink_batch_reset_all(struct zink_context *ctx)
{
   hash_table_foreach(&ctx->batch_states, entry) {
      struct zink_batch_state *bs = entry->data;
      bs->fence.completed = true;
      zink_reset_batch_state(ctx, bs);
      _mesa_hash_table_remove(&ctx->batch_states, entry);
      util_dynarray_append(&ctx->free_batch_states, struct zink_batch_state *, bs);
   }
}

void
zink_batch_state_destroy(struct zink_screen *screen, struct zink_batch_state *bs)
{
   if (!bs)
      return;

   util_queue_fence_destroy(&bs->flush_completed);

   if (bs->fence.fence)
      vkDestroyFence(screen->dev, bs->fence.fence, NULL);

   if (bs->cmdbuf)
      vkFreeCommandBuffers(screen->dev, bs->cmdpool, 1, &bs->cmdbuf);
   if (bs->cmdpool)
      vkDestroyCommandPool(screen->dev, bs->cmdpool, NULL);

   _mesa_hash_table_destroy(bs->framebuffer_cache, NULL);
   _mesa_set_destroy(bs->fence.resources, NULL);
   _mesa_set_destroy(bs->samplers, NULL);
   _mesa_set_destroy(bs->surfaces, NULL);
   _mesa_set_destroy(bs->bufferviews, NULL);
   _mesa_set_destroy(bs->programs, NULL);
   _mesa_set_destroy(bs->desc_sets, NULL);
   _mesa_set_destroy(bs->active_queries, NULL);
   simple_mtx_destroy(&bs->fence.resource_mtx);
   ralloc_free(bs);
}

static uint32_t
hash_framebuffer_state(const void *key)
{
   struct zink_framebuffer_state* s = (struct zink_framebuffer_state*)key;
   return _mesa_hash_data(key, offsetof(struct zink_framebuffer_state, attachments) + sizeof(s->attachments[0]) * s->num_attachments);
}

static bool
equals_framebuffer_state(const void *a, const void *b)
{
   struct zink_framebuffer_state *s = (struct zink_framebuffer_state*)a;
   return memcmp(a, b, offsetof(struct zink_framebuffer_state, attachments) + sizeof(s->attachments[0]) * s->num_attachments) == 0;
}

static struct zink_batch_state *
create_batch_state(struct zink_context *ctx)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   struct zink_batch_state *bs = rzalloc(NULL, struct zink_batch_state);
   VkCommandPoolCreateInfo cpci = {};
   cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   cpci.queueFamilyIndex = screen->gfx_queue;
   cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   if (vkCreateCommandPool(screen->dev, &cpci, NULL, &bs->cmdpool) != VK_SUCCESS)
      goto fail;

   VkCommandBufferAllocateInfo cbai = {};
   cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   cbai.commandPool = bs->cmdpool;
   cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   cbai.commandBufferCount = 1;

   if (vkAllocateCommandBuffers(screen->dev, &cbai, &bs->cmdbuf) != VK_SUCCESS)
      goto fail;

#define SET_CREATE_OR_FAIL(ptr) \
   ptr = _mesa_pointer_set_create(bs); \
   if (!ptr) \
      goto fail

   pipe_reference_init(&bs->reference, 1);
   SET_CREATE_OR_FAIL(bs->fence.resources);
   SET_CREATE_OR_FAIL(bs->samplers);
   SET_CREATE_OR_FAIL(bs->surfaces);
   SET_CREATE_OR_FAIL(bs->bufferviews);
   SET_CREATE_OR_FAIL(bs->programs);
   SET_CREATE_OR_FAIL(bs->desc_sets);
   SET_CREATE_OR_FAIL(bs->active_queries);

   bs->framebuffer_cache = _mesa_hash_table_create(bs, hash_framebuffer_state, equals_framebuffer_state);
   if (!bs->framebuffer_cache)
      goto fail;

   VkFenceCreateInfo fci = {};
   fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

   if (vkCreateFence(screen->dev, &fci, NULL, &bs->fence.fence) != VK_SUCCESS)
      goto fail;

   bs->queue = ctx->batch.queue;
   simple_mtx_init(&bs->fence.resource_mtx, mtx_plain);
   util_queue_fence_init(&bs->flush_completed);

   return bs;
fail:
   zink_batch_state_destroy(screen, bs);
   return NULL;
}

static bool
find_unused_state(struct hash_entry *entry)
{
   struct zink_fence *fence = entry->data;
   /* we can't reset these from fence_finish because threads */
   bool completed = p_atomic_read(&fence->completed);
   bool submitted = p_atomic_read(&fence->submitted);
   return submitted && completed;
}

static struct zink_batch_state *
get_batch_state(struct zink_context *ctx, struct zink_batch *batch)
{
   struct zink_batch_state *bs = NULL;

   if (util_dynarray_num_elements(&ctx->free_batch_states, struct zink_batch_state*))
      bs = util_dynarray_pop(&ctx->free_batch_states, struct zink_batch_state*);
   if (!bs) {
      struct hash_entry *he = _mesa_hash_table_random_entry(&ctx->batch_states, find_unused_state);
      if (he) { //there may not be any entries available
         bs = he->data;
         _mesa_hash_table_remove(&ctx->batch_states, he);
      }
   }
   if (bs)
      zink_reset_batch_state(ctx, bs);
   else {
      if (!batch->state) {
         /* this is batch init, so create a few more states for later use */
         for (int i = 0; i < 3; i++) {
            struct zink_batch_state *state = create_batch_state(ctx);
            util_dynarray_append(&ctx->free_batch_states, struct zink_batch_state *, state);
         }
      }
      bs = create_batch_state(ctx);
   }
   return bs;
}

void
zink_reset_batch(struct zink_context *ctx, struct zink_batch *batch)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   bool fresh = !batch->state;

   batch->state = get_batch_state(ctx, batch);
   assert(batch->state);

   if (!fresh) {
      if (vkResetCommandPool(screen->dev, batch->state->cmdpool, 0) != VK_SUCCESS)
         fprintf(stderr, "vkResetCommandPool failed\n");
   }
   batch->has_work = false;
}

void
zink_start_batch(struct zink_context *ctx, struct zink_batch *batch)
{
   zink_reset_batch(ctx, batch);

   VkCommandBufferBeginInfo cbbi = {};
   cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   if (vkBeginCommandBuffer(batch->state->cmdbuf, &cbbi) != VK_SUCCESS)
      debug_printf("vkBeginCommandBuffer failed\n");

   batch->state->fence.batch_id = ctx->curr_batch;
   batch->state->fence.completed = false;
   if (ctx->last_fence) {
      struct zink_batch_state *last_state = zink_batch_state(ctx->last_fence);
      batch->last_batch_id = last_state->fence.batch_id;
   } else {
      /* TODO: move to wsi */
      if (util_cpu_caps.nr_cpus > 1 && debug_get_bool_option("GALLIUM_THREAD", util_cpu_caps.nr_cpus > 1))
         util_queue_init(&batch->flush_queue, "zfq", 8, 1, 0);
   }
   if (!ctx->queries_disabled)
      zink_resume_queries(ctx, batch);
}

static void
submit_queue(void *data, int thread_index)
{
   struct zink_batch_state *bs = data;
   VkSubmitInfo si = {};
   si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   si.waitSemaphoreCount = 0;
   si.pWaitSemaphores = NULL;
   si.signalSemaphoreCount = 0;
   si.pSignalSemaphores = NULL;
   si.pWaitDstStageMask = NULL;
   si.commandBufferCount = 1;
   si.pCommandBuffers = &bs->cmdbuf;

   if (vkQueueSubmit(bs->queue, 1, &si, bs->fence.fence) != VK_SUCCESS) {
      debug_printf("ZINK: vkQueueSubmit() failed\n");
      bs->is_device_lost = true;
   }
   p_atomic_set(&bs->fence.submitted, true);
}

void
zink_end_batch(struct zink_context *ctx, struct zink_batch *batch)
{
   if (!ctx->queries_disabled)
      zink_suspend_queries(ctx, batch);

   if (vkEndCommandBuffer(batch->state->cmdbuf) != VK_SUCCESS) {
      debug_printf("vkEndCommandBuffer failed\n");
      return;
   }

   vkResetFences(zink_screen(ctx->base.screen)->dev, 1, &batch->state->fence.fence);

   ctx->last_fence = &batch->state->fence;
   _mesa_hash_table_insert_pre_hashed(&ctx->batch_states, batch->state->fence.batch_id, (void*)(uintptr_t)batch->state->fence.batch_id, batch->state);
   ctx->resource_size += batch->state->resource_size;


   if (util_queue_is_initialized(&batch->flush_queue))
      util_queue_add_job(&batch->flush_queue, batch->state, &batch->state->flush_completed,
                         submit_queue, NULL, 0);
   else
      submit_queue(batch->state, 0);
}

void
zink_batch_reference_resource_rw(struct zink_batch *batch, struct zink_resource *res, bool write)
{
   /* u_transfer_helper unrefs the stencil buffer when the depth buffer is unrefed,
    * so we add an extra ref here to the stencil buffer to compensate
    */
   struct zink_resource *stencil;

   zink_get_depth_stencil_resources((struct pipe_resource*)res, NULL, &stencil);

   /* if the resource already has usage of any sort set for this batch, we can skip hashing */
   if (!zink_batch_usage_matches(&res->obj->reads, batch->state->fence.batch_id) &&
       !zink_batch_usage_matches(&res->obj->writes, batch->state->fence.batch_id)) {
      bool found = false;
      _mesa_set_search_and_add(batch->state->fence.resources, res->obj, &found);
      if (!found) {
         pipe_reference(NULL, &res->obj->reference);
         if (!batch->last_batch_id || !zink_batch_usage_matches(&res->obj->reads, batch->last_batch_id))
            /* only add resource usage if it's "new" usage, though this only checks the most recent usage
             * and not all pending usages
             */
            batch->state->resource_size += res->obj->size;
         if (stencil) {
            pipe_reference(NULL, &stencil->base.b.reference);
            if (!batch->last_batch_id || !zink_batch_usage_matches(&stencil->obj->reads, batch->last_batch_id))
               batch->state->resource_size += stencil->obj->size;
         }
      }
       }
   if (write) {
      if (stencil)
         zink_batch_usage_set(&stencil->obj->writes, batch->state->fence.batch_id);
      zink_batch_usage_set(&res->obj->writes, batch->state->fence.batch_id);
   } else {
      if (stencil)
         zink_batch_usage_set(&stencil->obj->reads, batch->state->fence.batch_id);
      zink_batch_usage_set(&res->obj->reads, batch->state->fence.batch_id);
   }

   batch->has_work = true;
}

static bool
ptr_add_usage(struct zink_batch *batch, struct set *s, void *ptr, struct zink_batch_usage *u)
{
   bool found = false;
   if (zink_batch_usage_matches(u, batch->state->fence.batch_id))
      return false;
   _mesa_set_search_and_add(s, ptr, &found);
   assert(!found);
   zink_batch_usage_set(u, batch->state->fence.batch_id);
   return true;
}

void
zink_batch_reference_sampler_view(struct zink_batch *batch,
                                  struct zink_sampler_view *sv)
{
   if (sv->base.target == PIPE_BUFFER) {
      if (!ptr_add_usage(batch, batch->state->bufferviews, sv->buffer_view, &sv->buffer_view->batch_uses))
         return;
      pipe_reference(NULL, &sv->buffer_view->reference);
   } else {
      if (!ptr_add_usage(batch, batch->state->surfaces, sv->image_view, &sv->image_view->batch_uses))
         return;
      pipe_reference(NULL, &sv->image_view->base.reference);
   }
   batch->has_work = true;
}

void
zink_batch_reference_sampler(struct zink_batch *batch,
                             struct zink_sampler *sampler)
{
   if (!ptr_add_usage(batch, batch->state->samplers, sampler, &sampler->batch_uses))
      return;
   pipe_reference(NULL, &sampler->reference);
   batch->has_work = true;
}

void
zink_batch_reference_program(struct zink_batch *batch,
                             struct zink_program *pg)
{
   bool found = false;
   _mesa_set_search_and_add(batch->state->programs, pg, &found);
   if (!found)
      pipe_reference(NULL, &pg->reference);
   batch->has_work = true;
}

bool
zink_batch_add_desc_set(struct zink_batch *batch, struct zink_descriptor_set *zds)
{
   if (!ptr_add_usage(batch, batch->state->desc_sets, zds, &zds->batch_uses))
      return false;
   pipe_reference(NULL, &zds->reference);
   return true;
}

void
zink_batch_reference_image_view(struct zink_batch *batch,
                                struct zink_image_view *image_view)
{
   if (image_view->base.resource->target == PIPE_BUFFER) {
      if (!ptr_add_usage(batch, batch->state->bufferviews, image_view->buffer_view, &image_view->buffer_view->batch_uses))
         return;
      pipe_reference(NULL, &image_view->buffer_view->reference);
   } else {
      if (!ptr_add_usage(batch, batch->state->surfaces, image_view->surface, &image_view->surface->batch_uses))
         return;
      pipe_reference(NULL, &image_view->surface->base.reference);
   }
   batch->has_work = true;
}

void
zink_batch_usage_set(struct zink_batch_usage *u, uint32_t batch_id)
{
   p_atomic_set(&u->usage, batch_id);
}

bool
zink_batch_usage_matches(struct zink_batch_usage *u, uint32_t batch_id)
{
   uint32_t usage = p_atomic_read(&u->usage);
   return usage == batch_id;
}

bool
zink_batch_usage_exists(struct zink_batch_usage *u)
{
   uint32_t usage = p_atomic_read(&u->usage);
   return !!usage;
}
