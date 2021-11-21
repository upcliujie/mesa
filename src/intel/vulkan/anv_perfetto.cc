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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <perfetto.h>

#include "anv_private.h"

#include "perf/intel_perf.h"

#include "util/u_perfetto.h"
#include "util/hash_table.h"

#include "anv_tracepoints.h"
#include "anv_tracepoints_perfetto.h"

static uint32_t gpu_clock_id;
static uint64_t next_clock_sync_ns; /* cpu time of next clk sync */

/**
 * The timestamp at the point where we first emitted the clock_sync..
 * this  will be a *later* timestamp that the first GPU traces (since
 * we capture the first clock_sync from the CPU *after* the first GPU
 * tracepoints happen).  To avoid confusing perfetto we need to drop
 * the GPU traces with timestamps before this.
 */
static uint64_t sync_gpu_ts;

struct AnvRenderpassIncrementalState {
   bool was_cleared = true;
};

struct AnvRenderpassTraits : public perfetto::DefaultDataSourceTraits {
   using IncrementalStateType = AnvRenderpassIncrementalState;
};

class AnvRenderpassDataSource : public perfetto::DataSource<AnvRenderpassDataSource,
                                                            AnvRenderpassTraits> {
public:
   void OnSetup(const SetupArgs &) override
   {
      // Use this callback to apply any custom configuration to your data source
      // based on the TraceConfig in SetupArgs.
   }

   void OnStart(const StartArgs &) override
   {
      // This notification can be used to initialize the GPU driver, enable
      // counters, etc. StartArgs will contains the DataSourceDescriptor,
      // which can be extended.
      u_trace_perfetto_start();
      PERFETTO_LOG("Tracing started");

      /* Note: clock_id's below 128 are reserved.. for custom clock sources,
       * using the hash of a namespaced string is the recommended approach.
       * See: https://perfetto.dev/docs/concepts/clock-sync
       */
      gpu_clock_id =
         _mesa_hash_string("org.freedesktop.mesa.intel") | 0x80000000;
   }

   void OnStop(const StopArgs &) override
   {
      PERFETTO_LOG("Tracing stopped");

      sync_gpu_ts = 0;

      // Undo any initialization done in OnStart.
      u_trace_perfetto_stop();
      // TODO we should perhaps block until queued traces are flushed?

      Trace([](AnvRenderpassDataSource::TraceContext ctx) {
         auto packet = ctx.NewTracePacket();
         packet->Finalize();
         ctx.Flush();
      });
   }
};

PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(AnvRenderpassDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(AnvRenderpassDataSource);

using perfetto::protos::pbzero::InternedGpuRenderStageSpecification_RenderStageCategory;

enum InternedGpuRenderStageSpecification_RenderStageCategory
i915_engine_class_to_category(enum drm_i915_gem_engine_class engine_class)
{
   switch (engine_class) {
   case I915_ENGINE_CLASS_RENDER:
      return InternedGpuRenderStageSpecification_RenderStageCategory::
         InternedGpuRenderStageSpecification_RenderStageCategory_GRAPHICS;
   default:
      return InternedGpuRenderStageSpecification_RenderStageCategory::InternedGpuRenderStageSpecification_RenderStageCategory_OTHER;
   }
}

static void
send_descriptors(AnvRenderpassDataSource::TraceContext &ctx,
                 struct anv_device *device)
{
   PERFETTO_LOG("Sending renderstage descriptors");

   auto packet = ctx.NewTracePacket();

   packet->set_timestamp(perfetto::base::GetBootTimeNs().count());
   packet->set_timestamp_clock_id(perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME);

   auto event = packet->set_gpu_render_stage_event();
   event->set_gpu_id(device->physical->local_minor - 128);

   auto spec = event->set_specifications();

   for (unsigned i = 0; i < device->queue_count; i++) {
      struct anv_queue *queue = &device->queues[i];

      auto desc = spec->add_hw_queue();
      desc->set_name(queue->name);
      // TODO
      // desc->set_description();
   }

   for (unsigned i = 0; i < ANV_QUEUE_STAGE_N_STAGES; i++) {
      auto desc = spec->add_stage();
      desc->set_name(anv_queue_stage_names[i]);
      // TODO
      // desc->set_description(stages[i].desc);
   }
}

typedef void (*trace_payload_as_extra_func)(perfetto::protos::pbzero::GpuRenderStageEvent *, const void*);

static void
begin_event(struct anv_queue *queue, uint64_t ts_ns, enum anv_queue_stage stage)
{
   queue->stage_start_ns[stage] = ts_ns;
}

static void
end_event(struct anv_queue *queue, uint64_t ts_ns,
          enum anv_queue_stage stage,
          uint32_t submission_id, const void* payload = nullptr,
          trace_payload_as_extra_func payload_as_extra = nullptr)
{
   // struct tu_perfetto_state *p = tu_device_get_perfetto_state(dev);

   /* If we haven't managed to calibrate the alignment between GPU and CPU
    * timestamps yet, then skip this trace, otherwise perfetto won't know
    * what to do with it.
    */
   if (!sync_gpu_ts)
      return;

   AnvRenderpassDataSource::Trace([=](AnvRenderpassDataSource::TraceContext tctx) {
      if (auto state = tctx.GetIncrementalState(); state->was_cleared) {
         send_descriptors(tctx, queue->device);
         state->was_cleared = false;
      }

      auto packet = tctx.NewTracePacket();

      packet->set_timestamp(queue->stage_start_ns[stage]);
      packet->set_timestamp_clock_id(gpu_clock_id);

      assert(ts_ns >= queue->stage_start_ns[stage]);

      auto event = packet->set_gpu_render_stage_event();
      event->set_gpu_id(queue->device->physical->local_minor - 128);
      event->set_hw_queue_id(queue - queue->device->queues);
      event->set_stage_id(stage);
      event->set_context((uintptr_t)queue->device);
      event->set_event_id(ts_ns); // ???
      event->set_duration(ts_ns - queue->stage_start_ns[stage]);
      event->set_context((uintptr_t)queue->device);
      event->set_submission_id(submission_id);

      if (payload && payload_as_extra) {
         payload_as_extra(event, payload);
      }
   });
}

static void
event(struct anv_queue *queue, uint64_t ts_ns,
      uint32_t submission_id, const void* payload = nullptr,
      trace_payload_as_extra_func payload_as_extra = nullptr)
{
   // struct tu_perfetto_state *p = tu_device_get_perfetto_state(dev);

   /* If we haven't managed to calibrate the alignment between GPU and CPU
    * timestamps yet, then skip this trace, otherwise perfetto won't know
    * what to do with it.
    */
   if (!sync_gpu_ts)
      return;

   AnvRenderpassDataSource::Trace([=](AnvRenderpassDataSource::TraceContext tctx) {
      if (auto state = tctx.GetIncrementalState(); state->was_cleared) {
         send_descriptors(tctx, queue->device);
         state->was_cleared = false;
      }

      auto packet = tctx.NewTracePacket();

      packet->set_timestamp(ts_ns);
      packet->set_timestamp_clock_id(gpu_clock_id);

      auto event = packet->set_gpu_render_stage_event();
      event->set_gpu_id(queue->device->physical->local_minor - 128);
      event->set_hw_queue_id(queue - queue->device->queues);
      event->set_context((uintptr_t)queue->device);
      event->set_stage_id(ANV_QUEUE_STAGE_OTHER);
      event->set_event_id(ts_ns); // ???
      event->set_duration(// ts_ns - p->start_ts[stage]
         100);
      event->set_submission_id(submission_id);

      if (payload && payload_as_extra) {
         payload_as_extra(event, payload);
      }
   });
}

#ifdef __cplusplus
extern "C" {
#endif

void
anv_perfetto_init(void)
{
   util_perfetto_init();

   perfetto::DataSourceDescriptor dsd;
   dsd.set_name("gpu.renderstages.intel");
   AnvRenderpassDataSource::Register(dsd);
}

static void
sync_timestamp(struct anv_device *device)
{
   uint64_t cpu_ts = perfetto::base::GetBootTimeNs().count();
   uint64_t gpu_ts = intel_perf_scale_gpu_timestamp(&device->info,
                                                    intel_read_gpu_timestamp(device->fd));

   if (cpu_ts < next_clock_sync_ns)
      return;

   AnvRenderpassDataSource::Trace([=](AnvRenderpassDataSource::TraceContext tctx) {
      auto packet = tctx.NewTracePacket();

      PERFETTO_LOG("sending clocks");

      packet->set_timestamp(cpu_ts);

      auto event = packet->set_clock_snapshot();

      {
         auto clock = event->add_clocks();

         clock->set_clock_id(perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME);
         clock->set_timestamp(cpu_ts);
      }

      {
         auto clock = event->add_clocks();

         clock->set_clock_id(gpu_clock_id);
         clock->set_timestamp(gpu_ts);
      }

      sync_gpu_ts = gpu_ts;
      next_clock_sync_ns = cpu_ts + 1000000000ull;
   });
}

uint64_t
anv_perfetto_begin_submit(struct anv_queue *queue)
{
   return perfetto::base::GetBootTimeNs().count();
}

void
anv_perfetto_end_submit(struct anv_queue *queue,
                        uint32_t submission_id,
                        uint64_t start_ts)
{
   uint64_t end_ts = perfetto::base::GetBootTimeNs().count();

   sync_timestamp(queue->device);

   AnvRenderpassDataSource::Trace([=](AnvRenderpassDataSource::TraceContext tctx) {
      auto packet = tctx.NewTracePacket();

      packet->set_timestamp(start_ts);

      auto event = packet->set_vulkan_api_event();
      auto submit = event->set_vk_queue_submit();

      // submit->set_pid(os_get_pid());
      // submit->set_tid(os_get_tid());
      submit->set_duration_ns(end_ts - start_ts);
      submit->set_vk_queue((uintptr_t) queue);
      submit->set_submission_id(submission_id);
   });
}

/*
 * Trace callbacks, called from u_trace once the timestamps from GPU have been
 * collected.
 */

#define CREATE_EVENT_CALLBACK(event_name)                               \
   void                                                                 \
   intel_##event_name(struct anv_device *device,                        \
                      uint64_t ts_ns,                                   \
                      const void *flush_data,                           \
                      const struct trace_##event_name *payload)         \
   {                                                                    \
      return;                                                           \
      const struct anv_utrace_flush_copy *flush =                       \
         (const struct anv_utrace_flush_copy *) flush_data;             \
      event(flush->queue, ts_ns, flush->submission_id, payload,         \
            (trace_payload_as_extra_func)                               \
            &trace_payload_as_extra_##event_name);                      \
   }

#define CREATE_DUAL_EVENT_CALLBACK(event_name, stage)                   \
   void                                                                 \
   intel_begin_##event_name(struct anv_device *device,                  \
                            uint64_t ts_ns,                             \
                            const void *flush_data,                     \
                            const struct trace_begin_##event_name *payload) \
   {                                                                    \
      const struct anv_utrace_flush_copy *flush =                       \
         (const struct anv_utrace_flush_copy *) flush_data;             \
      begin_event(flush->queue, ts_ns, stage);                          \
   }                                                                    \
                                                                        \
   void                                                                 \
   intel_end_##event_name(struct anv_device *device,                    \
                          uint64_t ts_ns,                               \
                          const void *flush_data,                       \
                          const struct trace_end_##event_name *payload) \
   {                                                                    \
      const struct anv_utrace_flush_copy *flush =                       \
         (const struct anv_utrace_flush_copy *) flush_data;             \
      end_event(flush->queue, ts_ns, stage, flush->submission_id,       \
                payload,                                                \
                (trace_payload_as_extra_func)                           \
                &trace_payload_as_extra_end_##event_name);              \
   }                                                                    \


CREATE_DUAL_EVENT_CALLBACK(cmd_buffer, ANV_QUEUE_STAGE_CMD_BUFFER)
CREATE_DUAL_EVENT_CALLBACK(render_pass, ANV_QUEUE_STAGE_RENDER_PASS)
CREATE_DUAL_EVENT_CALLBACK(blorp, ANV_QUEUE_STAGE_BLORP)
CREATE_DUAL_EVENT_CALLBACK(draw, ANV_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indexed, ANV_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indexed_multi, ANV_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indexed_indirect, ANV_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_multi, ANV_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indirect, ANV_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indirect_count, ANV_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indirect_byte_count, ANV_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indexed_indirect_count, ANV_QUEUE_STAGE_DRAW)

CREATE_DUAL_EVENT_CALLBACK(compute, ANV_QUEUE_STAGE_COMPUTE)
CREATE_EVENT_CALLBACK(stall)

#ifdef __cplusplus
}
#endif
