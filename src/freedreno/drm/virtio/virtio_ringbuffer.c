/*
 * Copyright © 2022 Google, Inc.
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

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>

#include "util/hash_table.h"
#include "util/os_file.h"
#include "util/slab.h"

#include "drm/freedreno_ringbuffer.h"
#include "virtio_priv.h"

#define INIT_SIZE 0x1000

#define SUBALLOC_SIZE (32 * 1024)

/* In the pipe->flush() path, we don't have a util_queue_fence we can wait on,
 * instead use a condition-variable.  Note that pipe->flush() is not expected
 * to be a common/hot path.
 */
static pthread_cond_t  flush_cnd = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t flush_mtx = PTHREAD_MUTEX_INITIALIZER;


struct virtio_submit {
   struct fd_submit base;

   DECLARE_ARRAY(struct fd_bo *, bos);

   /* maps fd_bo to idx in bos table: */
   struct hash_table *bo_table;

   struct slab_child_pool ring_pool;

   /* Allow for sub-allocation of stateobj ring buffers (ie. sharing
    * the same underlying bo)..
    *
    * We also rely on previous stateobj having been fully constructed
    * so we can reclaim extra space at it's end.
    */
   struct fd_ringbuffer *suballoc_ring;

   /* Flush args, potentially attached to the last submit in the list
    * of submits to merge:
    */
   int in_fence_fd;
   struct fd_submit_fence *out_fence;

   /* State for enqueued submits:
    */
   struct list_head submit_list;   /* includes this submit as last element */

   /* Used in case out_fence==NULL: */
   struct util_queue_fence fence;
};
FD_DEFINE_CAST(fd_submit, virtio_submit);

/* for FD_RINGBUFFER_GROWABLE rb's, tracks the 'finalized' cmdstream buffers
 * and sizes.  Ie. a finalized buffer can have no more commands appended to
 * it.
 */
struct virtio_cmd {
   struct fd_bo *ring_bo;
   unsigned size;
};

struct virtio_ringbuffer {
   struct fd_ringbuffer base;

   /* for FD_RINGBUFFER_STREAMING rb's which are sub-allocated */
   unsigned offset;

   union {
      /* for _FD_RINGBUFFER_OBJECT case, the array of BOs referenced from
       * this one
       */
      struct {
         struct fd_pipe *pipe;
         DECLARE_ARRAY(struct fd_bo *, reloc_bos);
      };
      /* for other cases: */
      struct {
         struct fd_submit *submit;
         DECLARE_ARRAY(struct virtio_cmd, cmds);
      };
   } u;

   struct fd_bo *ring_bo;
};
FD_DEFINE_CAST(fd_ringbuffer, virtio_ringbuffer);

static void finalize_current_cmd(struct fd_ringbuffer *ring);
static struct fd_ringbuffer *
virtio_ringbuffer_init(struct virtio_ringbuffer *virtio_ring, uint32_t size,
                       enum fd_ringbuffer_flags flags);

/* add (if needed) bo to submit and return index: */
static uint32_t
virtio_submit_append_bo(struct virtio_submit *submit, struct fd_bo *bo)
{
   struct virtio_bo *virtio_bo = to_virtio_bo(bo);
   uint32_t idx;

   /* NOTE: it is legal to use the same bo on different threads for
    * different submits.  But it is not legal to use the same submit
    * from different threads.
    */
   idx = READ_ONCE(virtio_bo->idx);

   if (unlikely((idx >= submit->nr_bos) || (submit->bos[idx] != bo))) {
      uint32_t hash = _mesa_hash_pointer(bo);
      struct hash_entry *entry;

      entry = _mesa_hash_table_search_pre_hashed(submit->bo_table, hash, bo);
      if (entry) {
         /* found */
         idx = (uint32_t)(uintptr_t)entry->data;
      } else {
         idx = APPEND(submit, bos, fd_bo_ref(bo));

         _mesa_hash_table_insert_pre_hashed(submit->bo_table, hash, bo,
                                            (void *)(uintptr_t)idx);
      }
      virtio_bo->idx = idx;
   }

   return idx;
}

static void
virtio_submit_suballoc_ring_bo(struct fd_submit *submit,
                            struct virtio_ringbuffer *virtio_ring, uint32_t size)
{
   struct virtio_submit *virtio_submit = to_virtio_submit(submit);
   unsigned suballoc_offset = 0;
   struct fd_bo *suballoc_bo = NULL;

   if (virtio_submit->suballoc_ring) {
      struct virtio_ringbuffer *suballoc_ring =
         to_virtio_ringbuffer(virtio_submit->suballoc_ring);

      suballoc_bo = suballoc_ring->ring_bo;
      suballoc_offset =
         fd_ringbuffer_size(virtio_submit->suballoc_ring) + suballoc_ring->offset;

      suballoc_offset = align(suballoc_offset, 0x10);

      if ((size + suballoc_offset) > suballoc_bo->size) {
         suballoc_bo = NULL;
      }
   }

   if (!suballoc_bo) {
      // TODO possibly larger size for streaming bo?
      virtio_ring->ring_bo = fd_bo_new_ring(submit->pipe->dev, SUBALLOC_SIZE);
      virtio_ring->offset = 0;
   } else {
      virtio_ring->ring_bo = fd_bo_ref(suballoc_bo);
      virtio_ring->offset = suballoc_offset;
   }

   struct fd_ringbuffer *old_suballoc_ring = virtio_submit->suballoc_ring;

   virtio_submit->suballoc_ring = fd_ringbuffer_ref(&virtio_ring->base);

   if (old_suballoc_ring)
      fd_ringbuffer_del(old_suballoc_ring);
}

static struct fd_ringbuffer *
virtio_submit_new_ringbuffer(struct fd_submit *submit, uint32_t size,
                             enum fd_ringbuffer_flags flags)
{
   struct virtio_submit *virtio_submit = to_virtio_submit(submit);
   struct virtio_ringbuffer *virtio_ring;

   virtio_ring = slab_alloc(&virtio_submit->ring_pool);

   virtio_ring->u.submit = submit;

   /* NOTE: needs to be before _suballoc_ring_bo() since it could
    * increment the refcnt of the current ring
    */
   virtio_ring->base.refcnt = 1;

   if (flags & FD_RINGBUFFER_STREAMING) {
      virtio_submit_suballoc_ring_bo(submit, virtio_ring, size);
   } else {
      if (flags & FD_RINGBUFFER_GROWABLE)
         size = INIT_SIZE;

      virtio_ring->offset = 0;
      virtio_ring->ring_bo = fd_bo_new_ring(submit->pipe->dev, size);
   }

   if (!virtio_ringbuffer_init(virtio_ring, size, flags))
      return NULL;

   return &virtio_ring->base;
}

/**
 * Prepare submit for flush, always done synchronously.
 *
 * 1) Finalize primary ringbuffer, at this point no more cmdstream may
 *    be written into it, since from the PoV of the upper level driver
 *    the submit is flushed, even if deferred
 * 2) Add cmdstream bos to bos table
 * 3) Update bo fences
 */
static bool
virtio_submit_flush_prep(struct fd_submit *submit, int in_fence_fd,
                         struct fd_submit_fence *out_fence)
{
   struct virtio_submit *virtio_submit = to_virtio_submit(submit);
   bool has_shared = false;

   finalize_current_cmd(submit->primary);

   struct virtio_ringbuffer *primary =
      to_virtio_ringbuffer(submit->primary);

   for (unsigned i = 0; i < primary->u.nr_cmds; i++)
      virtio_submit_append_bo(virtio_submit, primary->u.cmds[i].ring_bo);

   simple_mtx_lock(&table_lock);
   for (unsigned i = 0; i < virtio_submit->nr_bos; i++) {
      fd_bo_add_fence(virtio_submit->bos[i], submit->pipe, submit->fence);
      has_shared |= virtio_submit->bos[i]->shared;
   }
   simple_mtx_unlock(&table_lock);

   virtio_submit->out_fence   = out_fence;
   virtio_submit->in_fence_fd = (in_fence_fd == -1) ?
         -1 : os_dupfd_cloexec(in_fence_fd);

   return has_shared;
}

static int
flush_submit_list(struct list_head *submit_list)
{
   struct virtio_submit *virtio_submit = to_virtio_submit(last_submit(submit_list));
   struct virtio_pipe *virtio_pipe = to_virtio_pipe(virtio_submit->base.pipe);
   struct fd_device *dev = virtio_pipe->base.dev;

   unsigned nr_cmds = 0;

   /* Determine the number of extra cmds's from deferred submits that
    * we will be merging in:
    */
   foreach_submit (submit, submit_list) {
      assert(submit->pipe == &virtio_pipe->base);
      nr_cmds += to_virtio_ringbuffer(submit->primary)->u.nr_cmds;
   }

   /* TODO we can get rid of the extra copy into the req by just
    * assuming the max amount that nr->bos will grow is by the
    * nr_cmds, and just over-allocate a bit.
    */

   struct drm_msm_gem_submit_cmd cmds[nr_cmds];

   unsigned cmd_idx = 0;

   /* Build up the table of cmds, and for all but the last submit in the
    * list, merge their bo tables into the last submit.
    */
   foreach_submit_safe (submit, submit_list) {
      struct virtio_ringbuffer *deferred_primary =
         to_virtio_ringbuffer(submit->primary);

      for (unsigned i = 0; i < deferred_primary->u.nr_cmds; i++) {
         cmds[cmd_idx].type = MSM_SUBMIT_CMD_BUF;
         cmds[cmd_idx].submit_idx =
               virtio_submit_append_bo(virtio_submit, deferred_primary->u.cmds[i].ring_bo);
         cmds[cmd_idx].submit_offset = deferred_primary->offset;
         cmds[cmd_idx].size = deferred_primary->u.cmds[i].size;
         cmds[cmd_idx].pad = 0;
         cmds[cmd_idx].nr_relocs = 0;

         cmd_idx++;
      }

      /* We are merging all the submits in the list into the last submit,
       * so the remainder of the loop body doesn't apply to the last submit
       */
      if (submit == last_submit(submit_list)) {
         DEBUG_MSG("merged %u submits", cmd_idx);
         break;
      }

      struct virtio_submit *virtio_deferred_submit = to_virtio_submit(submit);
      for (unsigned i = 0; i < virtio_deferred_submit->nr_bos; i++) {
         /* Note: if bo is used in both the current submit and the deferred
          * submit being merged, we expect to hit the fast-path as we add it
          * to the current submit:
          */
         virtio_submit_append_bo(virtio_submit, virtio_deferred_submit->bos[i]);
      }

      /* Now that the cmds/bos have been transfered over to the current submit,
       * we can remove the deferred submit from the list and drop it's reference
       */
      list_del(&submit->node);
      fd_submit_del(submit);
   }

   /* Needs to be after get_cmd() as that could create bos/cmds table:
    *
    * NOTE allocate on-stack in the common case, but with an upper-
    * bound to limit on-stack allocation to 4k:
    */
   const unsigned bo_limit = sizeof(struct drm_msm_gem_submit_bo) / 4096;
   bool bos_on_stack = virtio_submit->nr_bos < bo_limit;
   struct drm_msm_gem_submit_bo
      _submit_bos[bos_on_stack ? virtio_submit->nr_bos : 0];
   struct drm_msm_gem_submit_bo *submit_bos;
   if (bos_on_stack) {
      submit_bos = _submit_bos;
   } else {
      submit_bos = malloc(virtio_submit->nr_bos * sizeof(submit_bos[0]));
   }

   for (unsigned i = 0; i < virtio_submit->nr_bos; i++) {
      submit_bos[i].flags = virtio_submit->bos[i]->reloc_flags;
      submit_bos[i].handle = to_virtio_bo(virtio_submit->bos[i])->host_handle;
      submit_bos[i].presumed = 0;
   }

   if (virtio_pipe->next_submit_fence <= 0)
      virtio_pipe->next_submit_fence = 1;

   uint32_t kfence = virtio_pipe->next_submit_fence++;

   /* TODO avoid extra memcpy, and populate bo's and cmds directly
    * into the req msg
    */
   unsigned bos_len = virtio_submit->nr_bos * sizeof(struct drm_msm_gem_submit_bo);
   unsigned cmd_len = nr_cmds * sizeof(struct drm_msm_gem_submit_cmd);
   unsigned req_len = sizeof(struct msm_ccmd_gem_submit_req) + bos_len + cmd_len;
   struct msm_ccmd_gem_submit_req *req = malloc(req_len);

   req->hdr      = MSM_CCMD(GEM_SUBMIT, req_len);
   req->flags    = virtio_pipe->pipe;
   req->queue_id = virtio_pipe->queue_id;
   req->nr_bos   = virtio_submit->nr_bos;
   req->nr_cmds  = nr_cmds;
   req->fence    = kfence;

   memcpy(req->payload, submit_bos, bos_len);
   memcpy(req->payload + bos_len, cmds, cmd_len);

   struct fd_submit_fence *out_fence = virtio_submit->out_fence;
   int *out_fence_fd = NULL;

   if (out_fence) {
      out_fence->fence.kfence = kfence;
      out_fence->fence.ufence = virtio_submit->base.fence;
      /* Even if gallium driver hasn't requested a fence-fd, request one.
       * This way, if we have to block waiting for the fence, we can do
       * it in the guest, rather than in the single-threaded host.
       */
      out_fence->use_fence_fd = true;
      out_fence_fd = &out_fence->fence_fd;
   }

   if (virtio_submit->in_fence_fd != -1) {
      virtio_pipe->no_implicit_sync = true;
   }

   if (virtio_pipe->no_implicit_sync) {
      req->flags |= MSM_SUBMIT_NO_IMPLICIT;
   }

   virtio_execbuf_fenced(dev, &req->hdr, virtio_submit->in_fence_fd, out_fence_fd);

   if (!bos_on_stack)
      free(submit_bos);

   pthread_mutex_lock(&flush_mtx);
   assert(fd_fence_before(virtio_pipe->last_submit_fence, virtio_submit->base.fence));
   virtio_pipe->last_submit_fence = virtio_submit->base.fence;
   pthread_cond_broadcast(&flush_cnd);
   pthread_mutex_unlock(&flush_mtx);

   if (virtio_submit->in_fence_fd != -1)
      close(virtio_submit->in_fence_fd);

   return 0;
}

static void
virtio_submit_flush_execute(void *job, void *gdata, int thread_index)
{
   struct fd_submit *submit = job;
   struct virtio_submit *virtio_submit = to_virtio_submit(submit);

   flush_submit_list(&virtio_submit->submit_list);

   DEBUG_MSG("finish: %u", submit->fence);
}

static void
virtio_submit_flush_cleanup(void *job, void *gdata, int thread_index)
{
   struct fd_submit *submit = job;
   fd_submit_del(submit);
}

static int
enqueue_submit_list(struct list_head *submit_list)
{
   struct fd_submit *submit = last_submit(submit_list);
   struct virtio_submit *virtio_submit = to_virtio_submit(submit);
   struct virtio_device *virtio_dev = to_virtio_device(submit->pipe->dev);

   list_replace(submit_list, &virtio_submit->submit_list);
   list_inithead(submit_list);

   struct util_queue_fence *fence;
   if (virtio_submit->out_fence) {
      fence = &virtio_submit->out_fence->ready;
   } else {
      util_queue_fence_init(&virtio_submit->fence);
      fence = &virtio_submit->fence;
   }

   DEBUG_MSG("enqueue: %u", submit->fence);

   util_queue_add_job(&virtio_dev->submit_queue,
                      submit, fence,
                      virtio_submit_flush_execute,
                      virtio_submit_flush_cleanup,
                      0);

   return 0;
}

static bool
should_defer(struct fd_submit *submit)
{
   struct virtio_submit *virtio_submit = to_virtio_submit(submit);

   /* if too many bo's, it may not be worth the CPU cost of submit merging: */
   if (virtio_submit->nr_bos > 30)
      return false;

   /* On the kernel side, with 32K ringbuffer, we have an upper limit of 2k
    * cmds before we exceed the size of the ringbuffer, which results in
    * deadlock writing into the RB (ie. kernel doesn't finish writing into
    * the RB so it doesn't kick the GPU to start consuming from the RB)
    */
   if (submit->pipe->dev->deferred_cmds > 128)
      return false;

   return true;
}

static int
virtio_submit_flush(struct fd_submit *submit, int in_fence_fd,
                    struct fd_submit_fence *out_fence)
{
   struct fd_device *dev = submit->pipe->dev;
   struct virtio_pipe *virtio_pipe = to_virtio_pipe(submit->pipe);

   /* Acquire lock before flush_prep() because it is possible to race between
    * this and pipe->flush():
    */
   simple_mtx_lock(&dev->submit_lock);

   /* If there are deferred submits from another fd_pipe, flush them now,
    * since we can't merge submits from different submitqueue's (ie. they
    * could have different priority, etc)
    */
   if (!list_is_empty(&dev->deferred_submits) &&
       (last_submit(&dev->deferred_submits)->pipe != submit->pipe)) {
      struct list_head submit_list;

      list_replace(&dev->deferred_submits, &submit_list);
      list_inithead(&dev->deferred_submits);
      dev->deferred_cmds = 0;

      enqueue_submit_list(&submit_list);
   }

   list_addtail(&fd_submit_ref(submit)->node, &dev->deferred_submits);

   bool has_shared = virtio_submit_flush_prep(submit, in_fence_fd, out_fence);

   assert(fd_fence_before(virtio_pipe->last_enqueue_fence, submit->fence));
   virtio_pipe->last_enqueue_fence = submit->fence;

   /* If we don't need an out-fence, we can defer the submit.
    *
    * TODO we could defer submits with in-fence as well.. if we took our own
    * reference to the fd, and merged all the in-fence-fd's when we flush the
    * deferred submits
    */
   if ((in_fence_fd == -1) && !out_fence && !has_shared && should_defer(submit)) {
      DEBUG_MSG("defer: %u", submit->fence);
      dev->deferred_cmds += fd_ringbuffer_cmd_count(submit->primary);
      assert(dev->deferred_cmds == fd_dev_count_deferred_cmds(dev));
      simple_mtx_unlock(&dev->submit_lock);

      return 0;
   }

   struct list_head submit_list;

   list_replace(&dev->deferred_submits, &submit_list);
   list_inithead(&dev->deferred_submits);
   dev->deferred_cmds = 0;

   simple_mtx_unlock(&dev->submit_lock);

   return enqueue_submit_list(&submit_list);
}

void
virtio_pipe_flush(struct fd_pipe *pipe, uint32_t fence)
{
   struct virtio_pipe *virtio_pipe = to_virtio_pipe(pipe);
   struct fd_device *dev = pipe->dev;
   struct list_head submit_list;

   DEBUG_MSG("flush: %u", fence);

   list_inithead(&submit_list);

   simple_mtx_lock(&dev->submit_lock);

   assert(!fd_fence_after(fence, virtio_pipe->last_enqueue_fence));

   foreach_submit_safe (deferred_submit, &dev->deferred_submits) {
      /* We should never have submits from multiple pipes in the deferred
       * list.  If we did, we couldn't compare their fence to our fence,
       * since each fd_pipe is an independent timeline.
       */
      if (deferred_submit->pipe != pipe)
         break;

      if (fd_fence_after(deferred_submit->fence, fence))
         break;

      list_del(&deferred_submit->node);
      list_addtail(&deferred_submit->node, &submit_list);
      dev->deferred_cmds -= fd_ringbuffer_cmd_count(deferred_submit->primary);
   }

   assert(dev->deferred_cmds == fd_dev_count_deferred_cmds(dev));

   simple_mtx_unlock(&dev->submit_lock);

   if (list_is_empty(&submit_list))
      goto flush_sync;

   enqueue_submit_list(&submit_list);

flush_sync:
   /* Once we are sure that we've enqueued at least up to the requested
    * submit, we need to be sure that submitq has caught up and flushed
    * them to the kernel
    */
   pthread_mutex_lock(&flush_mtx);
   while (fd_fence_before(virtio_pipe->last_submit_fence, fence)) {
      pthread_cond_wait(&flush_cnd, &flush_mtx);
   }
   pthread_mutex_unlock(&flush_mtx);
}

static void
virtio_submit_destroy(struct fd_submit *submit)
{
   struct virtio_submit *virtio_submit = to_virtio_submit(submit);

   if (virtio_submit->suballoc_ring)
      fd_ringbuffer_del(virtio_submit->suballoc_ring);

   _mesa_hash_table_destroy(virtio_submit->bo_table, NULL);

   // TODO it would be nice to have a way to debug_assert() if all
   // rb's haven't been free'd back to the slab, because that is
   // an indication that we are leaking bo's
   slab_destroy_child(&virtio_submit->ring_pool);

   for (unsigned i = 0; i < virtio_submit->nr_bos; i++)
      fd_bo_del(virtio_submit->bos[i]);

   free(virtio_submit->bos);
   free(virtio_submit);
}

static const struct fd_submit_funcs submit_funcs = {
   .new_ringbuffer = virtio_submit_new_ringbuffer,
   .flush = virtio_submit_flush,
   .destroy = virtio_submit_destroy,
};

struct fd_submit *
virtio_submit_new(struct fd_pipe *pipe)
{
   struct virtio_submit *virtio_submit = calloc(1, sizeof(*virtio_submit));
   struct fd_submit *submit;

   virtio_submit->bo_table = _mesa_hash_table_create(NULL, _mesa_hash_pointer,
                                                  _mesa_key_pointer_equal);

   slab_create_child(&virtio_submit->ring_pool, &to_virtio_pipe(pipe)->ring_pool);

   submit = &virtio_submit->base;
   submit->funcs = &submit_funcs;

   return submit;
}

void
virtio_pipe_ringpool_init(struct virtio_pipe *virtio_pipe)
{
   // TODO tune size:
   slab_create_parent(&virtio_pipe->ring_pool, sizeof(struct virtio_ringbuffer),
                      16);
}

void
virtio_pipe_ringpool_fini(struct virtio_pipe *virtio_pipe)
{
   if (virtio_pipe->ring_pool.num_elements)
      slab_destroy_parent(&virtio_pipe->ring_pool);
}

static void
finalize_current_cmd(struct fd_ringbuffer *ring)
{
   debug_assert(!(ring->flags & _FD_RINGBUFFER_OBJECT));

   struct virtio_ringbuffer *virtio_ring = to_virtio_ringbuffer(ring);
   APPEND(&virtio_ring->u, cmds,
          (struct virtio_cmd){
             .ring_bo = fd_bo_ref(virtio_ring->ring_bo),
             .size = offset_bytes(ring->cur, ring->start),
          });
}

static void
virtio_ringbuffer_grow(struct fd_ringbuffer *ring, uint32_t size)
{
   struct virtio_ringbuffer *virtio_ring = to_virtio_ringbuffer(ring);
   struct fd_pipe *pipe = virtio_ring->u.submit->pipe;

   debug_assert(ring->flags & FD_RINGBUFFER_GROWABLE);

   finalize_current_cmd(ring);

   fd_bo_del(virtio_ring->ring_bo);
   virtio_ring->ring_bo = fd_bo_new_ring(pipe->dev, size);

   ring->start = fd_bo_map(virtio_ring->ring_bo);
   ring->end = &(ring->start[size / 4]);
   ring->cur = ring->start;
   ring->size = size;
}

static inline bool
virtio_ringbuffer_references_bo(struct fd_ringbuffer *ring, struct fd_bo *bo)
{
   struct virtio_ringbuffer *virtio_ring = to_virtio_ringbuffer(ring);

   for (int i = 0; i < virtio_ring->u.nr_reloc_bos; i++) {
      if (virtio_ring->u.reloc_bos[i] == bo)
         return true;
   }
   return false;
}

#define PTRSZ 64
#include "virtio_ringbuffer.h"
#undef PTRSZ
#define PTRSZ 32
#include "virtio_ringbuffer.h"
#undef PTRSZ

static uint32_t
virtio_ringbuffer_cmd_count(struct fd_ringbuffer *ring)
{
   if (ring->flags & FD_RINGBUFFER_GROWABLE)
      return to_virtio_ringbuffer(ring)->u.nr_cmds + 1;
   return 1;
}

static bool
virtio_ringbuffer_check_size(struct fd_ringbuffer *ring)
{
   assert(!(ring->flags & _FD_RINGBUFFER_OBJECT));
   struct virtio_ringbuffer *virtio_ring = to_virtio_ringbuffer(ring);
   struct fd_submit *submit = virtio_ring->u.submit;

   if (to_virtio_submit(submit)->nr_bos > MAX_ARRAY_SIZE/2) {
      return false;
   }

   return true;
}

static void
virtio_ringbuffer_destroy(struct fd_ringbuffer *ring)
{
   struct virtio_ringbuffer *virtio_ring = to_virtio_ringbuffer(ring);

   fd_bo_del(virtio_ring->ring_bo);

   if (ring->flags & _FD_RINGBUFFER_OBJECT) {
      for (unsigned i = 0; i < virtio_ring->u.nr_reloc_bos; i++) {
         fd_bo_del(virtio_ring->u.reloc_bos[i]);
      }
      free(virtio_ring->u.reloc_bos);

      free(virtio_ring);
   } else {
      struct fd_submit *submit = virtio_ring->u.submit;

      for (unsigned i = 0; i < virtio_ring->u.nr_cmds; i++) {
         fd_bo_del(virtio_ring->u.cmds[i].ring_bo);
      }
      free(virtio_ring->u.cmds);

      slab_free(&to_virtio_submit(submit)->ring_pool, virtio_ring);
   }
}

static const struct fd_ringbuffer_funcs ring_funcs_nonobj_32 = {
   .grow = virtio_ringbuffer_grow,
   .emit_reloc = virtio_ringbuffer_emit_reloc_nonobj_32,
   .emit_reloc_ring = virtio_ringbuffer_emit_reloc_ring_32,
   .cmd_count = virtio_ringbuffer_cmd_count,
   .check_size = virtio_ringbuffer_check_size,
   .destroy = virtio_ringbuffer_destroy,
};

static const struct fd_ringbuffer_funcs ring_funcs_obj_32 = {
   .grow = virtio_ringbuffer_grow,
   .emit_reloc = virtio_ringbuffer_emit_reloc_obj_32,
   .emit_reloc_ring = virtio_ringbuffer_emit_reloc_ring_32,
   .cmd_count = virtio_ringbuffer_cmd_count,
   .destroy = virtio_ringbuffer_destroy,
};

static const struct fd_ringbuffer_funcs ring_funcs_nonobj_64 = {
   .grow = virtio_ringbuffer_grow,
   .emit_reloc = virtio_ringbuffer_emit_reloc_nonobj_64,
   .emit_reloc_ring = virtio_ringbuffer_emit_reloc_ring_64,
   .cmd_count = virtio_ringbuffer_cmd_count,
   .check_size = virtio_ringbuffer_check_size,
   .destroy = virtio_ringbuffer_destroy,
};

static const struct fd_ringbuffer_funcs ring_funcs_obj_64 = {
   .grow = virtio_ringbuffer_grow,
   .emit_reloc = virtio_ringbuffer_emit_reloc_obj_64,
   .emit_reloc_ring = virtio_ringbuffer_emit_reloc_ring_64,
   .cmd_count = virtio_ringbuffer_cmd_count,
   .destroy = virtio_ringbuffer_destroy,
};

static inline struct fd_ringbuffer *
virtio_ringbuffer_init(struct virtio_ringbuffer *virtio_ring, uint32_t size,
                       enum fd_ringbuffer_flags flags)
{
   struct fd_ringbuffer *ring = &virtio_ring->base;

   /* We don't do any translation from internal FD_RELOC flags to MSM flags. */
   STATIC_ASSERT(FD_RELOC_READ == MSM_SUBMIT_BO_READ);
   STATIC_ASSERT(FD_RELOC_WRITE == MSM_SUBMIT_BO_WRITE);
   STATIC_ASSERT(FD_RELOC_DUMP == MSM_SUBMIT_BO_DUMP);

   debug_assert(virtio_ring->ring_bo);

   uint8_t *base = fd_bo_map(virtio_ring->ring_bo);
   ring->start = (void *)(base + virtio_ring->offset);
   ring->end = &(ring->start[size / 4]);
   ring->cur = ring->start;

   ring->size = size;
   ring->flags = flags;

   if (flags & _FD_RINGBUFFER_OBJECT) {
      if (fd_dev_64b(&virtio_ring->u.pipe->dev_id)) {
         ring->funcs = &ring_funcs_obj_64;
      } else {
         ring->funcs = &ring_funcs_obj_32;
      }
   } else {
      if (fd_dev_64b(&virtio_ring->u.submit->pipe->dev_id)) {
         ring->funcs = &ring_funcs_nonobj_64;
      } else {
         ring->funcs = &ring_funcs_nonobj_32;
      }
   }

   // TODO initializing these could probably be conditional on flags
   // since unneed for FD_RINGBUFFER_STAGING case..
   virtio_ring->u.cmds = NULL;
   virtio_ring->u.nr_cmds = virtio_ring->u.max_cmds = 0;

   virtio_ring->u.reloc_bos = NULL;
   virtio_ring->u.nr_reloc_bos = virtio_ring->u.max_reloc_bos = 0;

   return ring;
}

struct fd_ringbuffer *
virtio_ringbuffer_new_object(struct fd_pipe *pipe, uint32_t size)
{
   struct fd_device *dev = pipe->dev;
   struct virtio_ringbuffer *virtio_ring = malloc(sizeof(*virtio_ring));

   /* Lock access to the virtio_pipe->suballoc_* since ringbuffer object allocation
    * can happen both on the frontend (most CSOs) and the driver thread (a6xx
    * cached tex state, for example)
    */
   simple_mtx_lock(&dev->suballoc_lock);

   /* Maximum known alignment requirement is a6xx's TEX_CONST at 16 dwords */
   virtio_ring->offset = align(dev->suballoc_offset, 64);
   if (!dev->suballoc_bo ||
       virtio_ring->offset + size > fd_bo_size(dev->suballoc_bo)) {
      if (dev->suballoc_bo)
         fd_bo_del(dev->suballoc_bo);
      dev->suballoc_bo =
         fd_bo_new_ring(dev, MAX2(SUBALLOC_SIZE, align(size, 4096)));
      virtio_ring->offset = 0;
   }

   virtio_ring->u.pipe = pipe;
   virtio_ring->ring_bo = fd_bo_ref(dev->suballoc_bo);
   virtio_ring->base.refcnt = 1;

   dev->suballoc_offset = virtio_ring->offset + size;

   simple_mtx_unlock(&dev->suballoc_lock);

   return virtio_ringbuffer_init(virtio_ring, size, _FD_RINGBUFFER_OBJECT);
}
