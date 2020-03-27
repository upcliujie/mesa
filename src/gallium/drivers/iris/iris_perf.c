/*
 * Copyright © 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "iris_perf.h"
#include "iris_context.h"

#include "perf/gen_perf_query.h"

static void *
iris_oa_bo_alloc(struct iris_context *ice, const char *name, uint64_t size)
{
   struct iris_screen *screen = (struct iris_screen *) ice->ctx.screen;
   struct iris_bo *bo = iris_bo_alloc(screen->bufmgr, name, size, IRIS_MEMZONE_OTHER);

   bo->skip_implicit_flush = true;

   return bo;
}

static void
iris_perf_emit_stall_at_pixel_scoreboard(struct iris_context *ice,
                                         uint32_t gem_ctx_idx)
{
   iris_emit_end_of_pipe_sync(&ice->batches[gem_ctx_idx],
                              "OA metrics",
                              PIPE_CONTROL_STALL_AT_SCOREBOARD);
}

static void
iris_perf_emit_mi_report_perf_count(struct iris_context *ice,
                                    uint32_t gem_ctx_idx,
                                    void *bo,
                                    uint32_t offset_in_bytes,
                                    uint32_t report_id)
{
   struct iris_batch *batch = &ice->batches[gem_ctx_idx];
   ice->vtbl.emit_mi_report_perf_count(batch, bo, offset_in_bytes, report_id);
}

static bool
iris_perf_batch_references(struct iris_context *ice, uint32_t gem_ctx_idx, struct iris_bo *bo)
{
   return iris_batch_references(&ice->batches[gem_ctx_idx], bo);
}

static void
iris_perf_batchbuffer_flush(struct iris_context *ice, uint32_t gem_ctx_idx, const char *file, int line)
{
   _iris_batch_flush(&ice->batches[gem_ctx_idx], __FILE__, __LINE__);
}

static void
iris_perf_store_register_mem(void *ctx, uint32_t gem_ctx_idx, void *bo,
                             uint32_t reg, uint32_t reg_size,
                             uint32_t offset)
{
   struct iris_context *ice = ctx;
   struct iris_batch *batch = &ice->batches[gem_ctx_idx];
   if (reg_size == 8) {
      ice->vtbl.store_register_mem64(batch, reg, bo, offset, false);
   } else {
      assert(reg_size == 4);
      ice->vtbl.store_register_mem32(batch, reg, bo, offset, false);
   }
}

typedef void *(*bo_alloc_t)(void *, const char *, uint64_t);
typedef void (*bo_unreference_t)(void *);
typedef void *(*bo_map_t)(void *, void *, unsigned flags);
typedef void (*bo_unmap_t)(void *);
typedef void (*emit_mi_report_t)(void *, uint32_t, void *, uint32_t, uint32_t);
typedef void (*emit_mi_flush_t)(void *, uint32_t);
typedef void (*store_register_mem_t)(void *ctx, uint32_t, void *bo,
                                     uint32_t reg, uint32_t reg_size,
                                     uint32_t offset);
typedef bool (*batch_references_t)(void *ctx, uint32_t, void *bo);
typedef void (*batch_flush_t)(void *ctx, uint32_t, const char *, int);
typedef void (*bo_wait_rendering_t)(void *bo);
typedef bool (*bo_busy_t)(void *bo);

void
iris_perf_init_vtbl(struct gen_perf_context_vtable *vtable)
{
   vtable->bo_alloc = (bo_alloc_t) iris_oa_bo_alloc;
   vtable->bo_unreference = (bo_unreference_t)iris_bo_unreference;
   vtable->bo_map = (bo_map_t)iris_bo_map;
   vtable->bo_unmap = (bo_unmap_t)iris_bo_unmap;
   vtable->emit_stall_at_pixel_scoreboard =
      (emit_mi_flush_t)iris_perf_emit_stall_at_pixel_scoreboard;

   vtable->emit_mi_report_perf_count =
      (emit_mi_report_t)iris_perf_emit_mi_report_perf_count;
   vtable->batchbuffer_flush =
      (batch_flush_t) iris_perf_batchbuffer_flush;
   vtable->store_register_mem =
      (store_register_mem_t) iris_perf_store_register_mem;
   vtable->batch_references = (batch_references_t)iris_perf_batch_references;
   vtable->bo_wait_rendering =
      (bo_wait_rendering_t)iris_bo_wait_rendering;
   vtable->bo_busy = (bo_busy_t)iris_bo_busy;
}
