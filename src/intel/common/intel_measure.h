/*
 * Copyright © 2020 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef INTEL_MEASURE_H
#define INTEL_MEASURE_H

#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "util/list.h"
#include "util/u_dynarray.h"
#include "perf/intel_perf.h"
#include "perf/intel_perf_query.h"

enum intel_measure_snapshot_type {
   INTEL_SNAPSHOT_UNDEFINED,
   INTEL_SNAPSHOT_BLIT,
   INTEL_SNAPSHOT_CCS_AMBIGUATE,
   INTEL_SNAPSHOT_CCS_COLOR_CLEAR,
   INTEL_SNAPSHOT_CCS_PARTIAL_RESOLVE,
   INTEL_SNAPSHOT_CCS_RESOLVE,
   INTEL_SNAPSHOT_COMPUTE,
   INTEL_SNAPSHOT_COPY,
   INTEL_SNAPSHOT_DRAW,
   INTEL_SNAPSHOT_HIZ_AMBIGUATE,
   INTEL_SNAPSHOT_HIZ_CLEAR,
   INTEL_SNAPSHOT_HIZ_RESOLVE,
   INTEL_SNAPSHOT_MCS_AMBIGUATE,
   INTEL_SNAPSHOT_MCS_COLOR_CLEAR,
   INTEL_SNAPSHOT_MCS_PARTIAL_RESOLVE,
   INTEL_SNAPSHOT_SLOW_COLOR_CLEAR,
   INTEL_SNAPSHOT_SLOW_DEPTH_CLEAR,
   INTEL_SNAPSHOT_SECONDARY_BATCH,
   INTEL_SNAPSHOT_END,
};

enum intel_measure_events {
   INTEL_MEASURE_DRAW       = (1 << 0),
   INTEL_MEASURE_RENDERPASS = (1 << 1),
   INTEL_MEASURE_SHADER     = (1 << 2),
   INTEL_MEASURE_BATCH      = (1 << 3),
   INTEL_MEASURE_FRAME      = (1 << 4),
};

struct intel_measure_config {

   /* Stderr, or optionally set with INTEL_MEASURE=file={path{ */
   FILE                      *file;

   /* Events that will be measured.  Set only one flag, with
    * INTEL_MEASURE=[draw,rt,shader,batch,frame] */
   enum intel_measure_events  flags;

   /* Optionally set with INTEL_MEASURE=start={num} */
   unsigned                   start_frame;

   /* Optionally calculated with INTEL_MEASURE=count={num} based on
    * start_frame
    */
   unsigned                   end_frame;

   /* Number of events to combine per line of output. Optionally set with
    * INTEL_MEASURE=interval={num}
    */
   unsigned                   event_interval;

   /* Max snapshots per batch.  Set with
    * INTEL_MEASURE=batch_size={num}. Additional snapshots will be dropped.
    */
   unsigned                   batch_size;

   /* Max number of batch measurements that can be buffered, for combining
    * snapshots into frame or interval data.
    */
   unsigned                   buffer_size;

   /* Fifo which will be read to enable measurements at run-time.  Set with
    * INTEL_MEASURE=control={path}.  `echo {num} > {path}` will collect num
    * frames of measurements, beginning with the next frame boundary.
    */
   int                        control_fh;

   /* true when snapshots are currently being collected */
   bool                       enabled;

   /* Measure CPU timing, not GPU timing */
   bool                       cpu_measure;

   /** Which metric set are we recording?  or 0 (XXX: or -1?) for none */
   unsigned oa_metric_set;

   /** A string version of the oa set */
   const char * oa_metric_name;
};

struct intel_measure_batch;

struct intel_measure_snapshot {
   enum intel_measure_snapshot_type type;
   unsigned count, event_count;
   const char* event_name;
   uint32_t renderpass;
   uintptr_t vs, tcs, tes, gs, fs, cs, ms, ts;
   /* for vulkan secondary command buffers */
   struct intel_measure_batch *secondary;

   /** A perf query object for oa measurements */
   struct intel_perf_query_object * perf_query;
};

struct intel_measure_buffered_result {
   struct intel_measure_snapshot snapshot;
   uint64_t start_ts, end_ts, idle_duration;
   unsigned frame, batch_count, event_index, primary_renderpass;
   unsigned *oa_result_data;
};

struct intel_measure_ringbuffer {
   unsigned head, tail;
   struct intel_measure_buffered_result results[0];
};

/* This function will be called when enqueued snapshots have been processed */
typedef void (*intel_measure_release_batch_cb)(struct intel_measure_batch *base);

/**
 * manager for OA Result storage
*/
struct intel_measure_oa_result_manager {
   pthread_mutex_t mutex;

   /** The size of the slots in the pool */
   unsigned slot_size;

   /** A pool of storage objects */
   struct util_dynarray * pool;
};

void intel_measure_oa_result_manager_init(struct intel_measure_oa_result_manager * mgr, const unsigned size);
void * intel_measure_oa_result_manager_take(struct intel_measure_oa_result_manager * mgr);
void intel_measure_oa_result_manager_return(struct intel_measure_oa_result_manager * mgr, void * data);

struct intel_measure_device {
   struct intel_measure_config *config;
   unsigned frame;
   unsigned render_pass_count;
   intel_measure_release_batch_cb release_batch;

   /* Holds the list of (iris/anv)_measure_batch snapshots that have been
    * submitted for rendering, but have not completed.
    */
   pthread_mutex_t mutex;
   struct list_head queued_snapshots;

   /* Holds completed snapshots that may need to be combined before being
    * written out
    */
   struct intel_measure_ringbuffer *ringbuffer;

   /**
    * A list of free results for use with OA measure gathering
    */
   struct intel_measure_oa_result_manager * oa_results;

};

struct intel_measure_batch {
   struct list_head link;
   unsigned index;
   unsigned frame, batch_count, event_count;
   uint32_t renderpass, primary_renderpass;
   uint64_t *timestamps;
   struct intel_measure_snapshot snapshots[0];
};

void intel_measure_init(struct intel_measure_device *device);
const char * intel_measure_snapshot_string(enum intel_measure_snapshot_type type);
bool intel_measure_state_changed(const struct intel_measure_batch *batch,
                                 uintptr_t vs, uintptr_t tcs, uintptr_t tes,
                                 uintptr_t gs, uintptr_t fs, uintptr_t cs,
                                 uintptr_t ms, uintptr_t ts);
void intel_measure_frame_transition(unsigned frame);

bool intel_measure_ready(struct intel_measure_batch *batch);

struct intel_device_info;
void intel_measure_print_cpu_result(unsigned int frame,
                                    unsigned int batch_count,
                                    unsigned int event_index,
                                    unsigned int event_count,
                                    unsigned int count,
                                    const char* event_name);
void intel_measure_gather(struct intel_measure_device *device,
                          const struct intel_device_info *info,
                          struct intel_perf_context *perf);

/** Initialize a perf context for use by intel_measure */
void intel_measure_perf_init_ctx(struct intel_perf_config * perf_cfg,
                                 void * mem_ctx,
                                 void * bufmgr,
                                 const struct intel_device_info * devinfo,
                                 uint32_t hw_ctx_id,
                                 int drm_fd,
                                 struct intel_perf_context * perf_ctx,
                                 struct intel_measure_config * config);
#endif /* INTEL_MEASURE_H */
