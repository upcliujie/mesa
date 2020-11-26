/*
 * Copyright © 2020 Google, Inc.
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

#include <inttypes.h>

#include "pipe/p_context.h"
#include "pipe/p_state.h"

#include "util/list.h"
#include "util/ralloc.h"
#include "util/u_debug.h"
#include "util/u_inlines.h"

#include "u_fifo.h"
#include "u_trace.h"

#define __NEEDS_TRACE_PRIV
#include "u_trace_priv.h"

enum {
   timestamp_buf_size = 0x1000, // XXX PAGE_SIZE
   traces_per_chunk = timestamp_buf_size / sizeof(uint64_t),
};

/**
 * A "chunk" of trace-events and corresponding timestamp buffer.  As
 * trace events are emitted, additional trace chucks will be allocated
 * as needed.  When u_trace_flush() is called, they are transferred
 * from the u_trace to the u_trace_context queue.
 */
struct u_trace_chunk {
   struct list_head node;

   struct u_trace_context *utctx;

   /* The number of traces this chunk contains so far: */
   unsigned num_traces;

   /**
    * The trace event FIFO consists of pairs of pointers, a u_tracepoint
    * ptr followed by trace payload ptr.
    */
   struct util_fifo *trace_fifo;

   /* list of recorded 64b timestamps */
   struct pipe_resource *timestamps;

   /**
    * For trace payload, we sub-allocate from ralloc'd buffers which
    * hang off of the chunk's ralloc context, so they are automatically
    * free'd when the chunk is free'd
    */
   uint8_t *payload_buf, *payload_end;

   struct util_queue_fence fence;

   bool last;          /* this chunk is last in batch */
// TODO where can this be set?
   bool eof;           /* this chunk is last in frame */
};

DEBUG_GET_ONCE_BOOL_OPTION(trace, "GALLIUM_GPU_TRACE", false)

void
u_trace_context_init(struct u_trace_context *utctx,
      struct pipe_context *pctx,
      u_record_timestamp record_timestamp,
      u_translate_timestamp translate_timestamp)
{
   utctx->pctx = pctx;
   utctx->record_timestamp = record_timestamp;
   utctx->translate_timestamp = translate_timestamp;

   bool ret = util_queue_init(&utctx->queue, "traceq", 256, 1,
         UTIL_QUEUE_INIT_USE_MINIMUM_PRIORITY |
         UTIL_QUEUE_INIT_RESIZE_IF_FULL);
   assert(ret);

   utctx->last_time_ns = 0;
   utctx->first_time_ns = 0;

   utctx->enabled = ret && debug_get_option_trace();
}

void
u_trace_context_fini(struct u_trace_context *utctx)
{
   util_queue_finish(&utctx->queue);
   util_queue_destroy(&utctx->queue);
}

void
u_trace_init(struct u_trace *ut, struct u_trace_context *utctx)
{
   ut->utctx = utctx;
   list_inithead(&ut->trace_chunks);
}

void
u_trace_fini(struct u_trace *ut)
{
   /* Normally the list of trace-chunks would be empty, if they
    * have been flushed to the trace-context.
    */
   while (!list_is_empty(&ut->trace_chunks)) {
      struct u_trace_chunk *chunk = list_first_entry(&ut->trace_chunks,
            struct u_trace_chunk, node);
      ralloc_free(chunk);
   }
}

static void
free_chunk(void *ptr)
{
   struct u_trace_chunk *chunk = ptr;

   u_fifo_destroy(chunk->trace_fifo);

   pipe_resource_reference(&chunk->timestamps, NULL);

   list_del(&chunk->node);
}

static struct u_trace_chunk *
get_chunk(struct u_trace *ut)
{
   struct u_trace_chunk *chunk;

   /* do we currently have a non-full chunk to append msgs to? */
   if (!list_is_empty(&ut->trace_chunks)) {
           chunk = list_last_entry(&ut->trace_chunks,
                           struct u_trace_chunk, node);
           if (chunk->num_traces < traces_per_chunk)
                   return chunk;
           /* we need to expand to add another chunk to the batch, so
            * the current one is no longer the last one of the batch:
            */
           chunk->last = false;
   }

   /* .. if not, then create a new one: */
   chunk = rzalloc_size(NULL, sizeof(*chunk));
   ralloc_set_destructor(chunk, free_chunk);

   chunk->utctx = ut->utctx;

   chunk->trace_fifo = u_fifo_create(traces_per_chunk * 2);

   struct pipe_resource tmpl = {
         .target     = PIPE_BUFFER,
         .format     = PIPE_FORMAT_R8_UNORM,
         .bind       = PIPE_BIND_QUERY_BUFFER | PIPE_BIND_LINEAR,
         .width0     = timestamp_buf_size,
         .height0    = 1,
         .depth0     = 1,
         .array_size = 1,
   };

   struct pipe_screen *pscreen = ut->utctx->pctx->screen;
   chunk->timestamps = pscreen->resource_create(pscreen, &tmpl);

   chunk->last = true;

   list_addtail(&chunk->node, &ut->trace_chunks);

   return chunk;
}

/**
 * Append a trace event, returning pointer to buffer of tp->payload_sz
 * to be filled in with trace payload.  Called by generated tracepoint
 * functions.
 */
void *
u_trace_append(struct u_trace *ut, const struct u_tracepoint *tp)
{
   struct u_trace_chunk *chunk = get_chunk(ut);

   assert(tp->payload_sz == ALIGN_NPOT(tp->payload_sz, 8));

   if (unlikely((chunk->payload_buf + tp->payload_sz) > chunk->payload_end)) {
      const unsigned payload_chunk_sz = 0x400;  /* TODO arbitrary size? */

      assert(tp->payload_sz < payload_chunk_sz);

      chunk->payload_buf = ralloc_size(chunk, payload_chunk_sz);
      chunk->payload_end = chunk->payload_buf + payload_chunk_sz;
   }

   /* sub-allocate storage for trace payload: */
   void *payload = chunk->payload_buf;
   chunk->payload_buf += tp->payload_sz;

   /* record a timestamp for the trace: */
   ut->utctx->record_timestamp(ut, chunk->timestamps,
         chunk->num_traces * sizeof(uint64_t));

   chunk->num_traces++;

   u_fifo_add(chunk->trace_fifo, (void *)tp);
   u_fifo_add(chunk->trace_fifo, payload);

   return payload;
}

static void
u_trace_chunk_process(void *job, int thread_index)
{
   struct u_trace_chunk *chunk = job;
   struct u_trace_context *utctx = chunk->utctx;
   const uint64_t *timestamps;
   FILE *out = stdout;

   /* Map the timestamp buffer, this should stall until the GPU has
    * finished writing it's timestamps, at which point we are ready
    * to dump the traces:
    *
    * TODO we can't pctx->transfer_map() from a different thread..
    * creating an entire private pctx is a bit heavy.  Maybe just
    * add another driver callback to read-back timestamps?  I
    * suppose that would make it easier to extract this out into
    * something that could be shared with vk drivers.. OTOH re-
    * using transfer_map() is probably convenient for drivers that
    * need to move the timestamp buffer out of VRAM?
    */
   struct pipe_context *pctx = chunk->utctx->pctx;
   struct pipe_transfer *xfer = NULL;
   struct pipe_box box;

   u_box_1d(0, timestamp_buf_size, &box);

   timestamps = pctx->transfer_map(pctx, chunk->timestamps, 0,
         PIPE_MAP_READ, &box, &xfer);

   /* For first chunk of batch, accumulated times will be zerod: */
   if (!utctx->last_time_ns) {
      fprintf(out, "+----- TS -----+ +----- NS -----+ +-- Δ --+  +----- MSG -----\n");
   }

   while (chunk->num_traces > 0) {
      const struct u_tracepoint *tp;
      const void *payload;

      u_fifo_pop(chunk->trace_fifo, (void **)&tp);
      u_fifo_pop(chunk->trace_fifo, (void **)&payload);

      uint64_t ts = *timestamps++;
      uint64_t ns;
      int32_t delta;

      if (!utctx->first_time_ns)
         utctx->first_time_ns = ns;

      if (ts != U_TRACE_NO_TIMESTAMP) {
         ns = utctx->translate_timestamp(utctx, ts);

         delta = utctx->last_time_ns ? ns - utctx->last_time_ns : 0;
         utctx->last_time_ns = ns;
      } else {
         /* we skipped recording the timestamp, so it should be
          * the same as last msg:
          */
         ns = utctx->last_time_ns;
         delta = 0;
      }

      fprintf(out, "%016"PRIu64" %016"PRIu64" %+9d: %s: ",
                    ts, ns, delta, tp->name);
      if (tp->print) {
         tp->print(out, payload);
      } else {
         fprintf(out, "\n");
      }

      chunk->num_traces--;
   }

   pctx->transfer_unmap(pctx, xfer);

   if (chunk->last) {
      uint64_t elapsed = utctx->last_time_ns - utctx->first_time_ns;
      fprintf(out, "ELAPSED: %"PRIu64" ns\n", elapsed);

      utctx->last_time_ns = 0;
      utctx->first_time_ns = 0;
   }

   if (chunk->eof)
      fprintf(out, "END OF FRAME %u\n", utctx->frame_nr++);
}

static void
u_trace_chunk_cleanup(void *job, int thread_index)
{
   ralloc_free(job);
}

void
u_trace_flush(struct u_trace *ut)
{
   while (!list_is_empty(&ut->trace_chunks)) {
      struct u_trace_chunk *chunk = list_first_entry(&ut->trace_chunks,
            struct u_trace_chunk, node);

      /* remove from list before enqueuing, because chunk is freed
       * once it is processed by the queue:
       */
      list_delinit(&chunk->node);

      util_queue_add_job(&ut->utctx->queue, chunk, &chunk->fence,
            u_trace_chunk_process, u_trace_chunk_cleanup,
            timestamp_buf_size);
   }
}
