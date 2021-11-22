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

#include <stdio.h>
#include <stdarg.h>

#include "common/intel_gem.h"
#include "perf/intel_perf.h"

#include "util/hash_table.h"

#include "intel_driver_ds.h"
#include "intel_pps_priv.h"
#include "intel_tracepoints.h"

#ifdef HAVE_PERFETTO

#include "util/u_perfetto.h"

#include "intel_tracepoints_perfetto.h"

static const char *intel_queue_stage_names[INTEL_DS_QUEUE_STAGE_N_STAGES] = {
   "cmd-buffer",
   "compute",
   "render-pass",
   "stall",
   "blorp",
   "draw",
};

struct IntelRenderpassIncrementalState {
   bool was_cleared = true;
};

struct IntelRenderpassTraits : public perfetto::DefaultDataSourceTraits {
   using IncrementalStateType = IntelRenderpassIncrementalState;
};

class IntelRenderpassDataSource : public perfetto::DataSource<IntelRenderpassDataSource,
                                                            IntelRenderpassTraits> {
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
   }

   void OnStop(const StopArgs &) override
   {
      PERFETTO_LOG("Tracing stopped");

      // Undo any initialization done in OnStart.
      u_trace_perfetto_stop();
      // TODO we should perhaps block until queued traces are flushed?

      Trace([](IntelRenderpassDataSource::TraceContext ctx) {
         auto packet = ctx.NewTracePacket();
         packet->Finalize();
         ctx.Flush();
      });
   }
};

PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(IntelRenderpassDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(IntelRenderpassDataSource);

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
send_descriptors(IntelRenderpassDataSource::TraceContext &ctx,
                 struct intel_ds_device *device)
{
   PERFETTO_LOG("Sending renderstage descriptors");

   auto packet = ctx.NewTracePacket();

   packet->set_timestamp(perfetto::base::GetBootTimeNs().count());
   packet->set_timestamp_clock_id(perfetto::protos::pbzero::BUILTIN_CLOCK_BOOTTIME);

   auto event = packet->set_gpu_render_stage_event();
   event->set_gpu_id(device->gpu_id);

   auto spec = event->set_specifications();

   struct intel_ds_queue *queue;
   u_vector_foreach(queue, &device->queues) {
      auto desc = spec->add_hw_queue();
      desc->set_name(queue->name);
      // TODO
      // desc->set_description();
   }

   for (unsigned i = 0; i < INTEL_DS_QUEUE_STAGE_N_STAGES; i++) {
      auto desc = spec->add_stage();
      desc->set_name(intel_queue_stage_names[i]);
      // TODO
      // desc->set_description(stages[i].desc);
   }
}

typedef void (*trace_payload_as_extra_func)(perfetto::protos::pbzero::GpuRenderStageEvent *, const void*);

static void
begin_event(struct intel_ds_queue *queue, uint64_t ts_ns,
            enum intel_ds_queue_stage stage)
{
   queue->stage_start_ns[stage] = ts_ns;
}

static void
end_event(struct intel_ds_queue *queue, uint64_t ts_ns,
          enum intel_ds_queue_stage stage,
          uint32_t submission_id, const void* payload = nullptr,
          trace_payload_as_extra_func payload_as_extra = nullptr)
{
   struct intel_ds_device *device = queue->device;

   /* If we haven't managed to calibrate the alignment between GPU and CPU
    * timestamps yet, then skip this trace, otherwise perfetto won't know
    * what to do with it.
    */
   if (!device->sync_gpu_ts)
      return;

   /* Discard anything prior to the first GPU timestamp snapshot. */
   if (device->sync_gpu_ts > queue->stage_start_ns[stage])
      return;

   IntelRenderpassDataSource::Trace([=](IntelRenderpassDataSource::TraceContext tctx) {
      if (auto state = tctx.GetIncrementalState(); state->was_cleared) {
         send_descriptors(tctx, queue->device);
         state->was_cleared = false;
      }

      auto packet = tctx.NewTracePacket();

      packet->set_timestamp(queue->stage_start_ns[stage]);
      packet->set_timestamp_clock_id(queue->device->gpu_clock_id);

      assert(ts_ns >= queue->stage_start_ns[stage]);

      auto event = packet->set_gpu_render_stage_event();
      event->set_gpu_id(queue->device->gpu_id);
      event->set_hw_queue_id(queue->queue_id);
      event->set_stage_id(stage);
      event->set_context((uintptr_t)queue->device);
      event->set_event_id(0); // ???
      event->set_duration(ts_ns - queue->stage_start_ns[stage]);
      event->set_context((uintptr_t)queue->device);
      event->set_submission_id(submission_id);

      if (payload && payload_as_extra) {
         payload_as_extra(event, payload);
      }
   });
}

static void
custom_trace_payload_as_extra_end_stall(perfetto::protos::pbzero::GpuRenderStageEvent *event,
                                        const struct trace_end_stall *payload)
{
   char buf[256];

   {
      auto data = event->add_extra_data();
      data->set_name("stall_reason");

      snprintf(buf, sizeof(buf), "%s%s%s%s%s%s%s%s%s%s%s%s%s%s : %s",
              (payload->flags & INTEL_DS_DEPTH_CACHE_FLUSH_BIT) ? "+depth_flush" : "",
              (payload->flags & INTEL_DS_DATA_CACHE_FLUSH_BIT) ? "+dc_flush" : "",
              (payload->flags & INTEL_DS_HDC_PIPELINE_FLUSH_BIT) ? "+hdc_flush" : "",
              (payload->flags & INTEL_DS_RENDER_TARGET_CACHE_FLUSH_BIT) ? "+rt_flush" : "",
              (payload->flags & INTEL_DS_TILE_CACHE_FLUSH_BIT) ? "+tile_flush" : "",
              (payload->flags & INTEL_DS_STATE_CACHE_INVALIDATE_BIT) ? "+state_inv" : "",
              (payload->flags & INTEL_DS_CONST_CACHE_INVALIDATE_BIT) ? "+const_inv" : "",
              (payload->flags & INTEL_DS_VF_CACHE_INVALIDATE_BIT) ? "+vf_inv" : "",
              (payload->flags & INTEL_DS_TEXTURE_CACHE_INVALIDATE_BIT) ? "+tex_inv" : "",
              (payload->flags & INTEL_DS_INST_CACHE_INVALIDATE_BIT) ? "+inst_inv" : "",
              (payload->flags & INTEL_DS_STALL_AT_SCOREBOARD_BIT) ? "+pb_stall" : "",
              (payload->flags & INTEL_DS_DEPTH_STALL_BIT) ? "+depth_stall" : "",
              (payload->flags & INTEL_DS_HDC_PIPELINE_FLUSH_BIT) ? "+hdc_flush" : "",
              (payload->flags & INTEL_DS_CS_STALL_BIT) ? "+cs_stall" : "",
              payload->reason ? payload->reason : "unknown");

      assert(strlen(buf) > 0);

      data->set_value(buf);
   }
}

static void
sync_timestamp(struct intel_ds_device *device)
{
   uint64_t cpu_ts = perfetto::base::GetBootTimeNs().count();
   uint64_t gpu_ts = intel_perf_scale_gpu_timestamp(&device->info,
                                                    intel_read_gpu_timestamp(device->fd));

   if (cpu_ts < device->next_clock_sync_ns)
      return;

   device->sync_gpu_ts = gpu_ts;
   device->next_clock_sync_ns = cpu_ts + 1000000000ull;

   IntelRenderpassDataSource::Trace([=](IntelRenderpassDataSource::TraceContext tctx) {
      if (auto state = tctx.GetIncrementalState(); state->was_cleared) {
         send_descriptors(tctx, device);
         state->was_cleared = false;
      }

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

         clock->set_clock_id(device->gpu_clock_id);
         clock->set_timestamp(gpu_ts);
      }
   });
}

#endif /* HAVE_PERFETTO */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_PERFETTO

/*
 * Trace callbacks, called from u_trace once the timestamps from GPU have been
 * collected.
 */

#define CREATE_EVENT_CALLBACK(event_name)                               \
   void                                                                 \
   intel_##event_name(struct intel_ds_device *device,                   \
                      uint64_t ts_ns,                                   \
                      const void *flush_data,                           \
                      const struct trace_##event_name *payload)         \
   {                                                                    \
      return;                                                           \
      const struct intel_ds_flush_data *flush =                         \
         (const struct intel_ds_flush_data *) flush_data;               \
      event(flush->queue, ts_ns, flush->submission_id, payload,         \
            (trace_payload_as_extra_func)                               \
            &trace_payload_as_extra_##event_name);                      \
   }

#define CREATE_DUAL_EVENT_CALLBACK(event_name, stage)                   \
   void                                                                 \
   intel_begin_##event_name(struct intel_ds_device *device,             \
                            uint64_t ts_ns,                             \
                            const void *flush_data,                     \
                            const struct trace_begin_##event_name *payload) \
   {                                                                    \
      const struct intel_ds_flush_data *flush =                         \
         (const struct intel_ds_flush_data *) flush_data;               \
      begin_event(flush->queue, ts_ns, stage);                          \
   }                                                                    \
                                                                        \
   void                                                                 \
   intel_end_##event_name(struct intel_ds_device *device,               \
                          uint64_t ts_ns,                               \
                          const void *flush_data,                       \
                          const struct trace_end_##event_name *payload) \
   {                                                                    \
      const struct intel_ds_flush_data *flush =                         \
         (const struct intel_ds_flush_data *) flush_data;               \
      end_event(flush->queue, ts_ns, stage, flush->submission_id,       \
                payload,                                                \
                (trace_payload_as_extra_func)                           \
                &trace_payload_as_extra_end_##event_name);              \
   }                                                                    \


CREATE_DUAL_EVENT_CALLBACK(batch, INTEL_DS_QUEUE_STAGE_CMD_BUFFER)
CREATE_DUAL_EVENT_CALLBACK(cmd_buffer, INTEL_DS_QUEUE_STAGE_CMD_BUFFER)
CREATE_DUAL_EVENT_CALLBACK(render_pass, INTEL_DS_QUEUE_STAGE_RENDER_PASS)
CREATE_DUAL_EVENT_CALLBACK(blorp, INTEL_DS_QUEUE_STAGE_BLORP)
CREATE_DUAL_EVENT_CALLBACK(draw, INTEL_DS_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indexed, INTEL_DS_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indexed_multi, INTEL_DS_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indexed_indirect, INTEL_DS_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_multi, INTEL_DS_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indirect, INTEL_DS_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indirect_count, INTEL_DS_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indirect_byte_count, INTEL_DS_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(draw_indexed_indirect_count, INTEL_DS_QUEUE_STAGE_DRAW)
CREATE_DUAL_EVENT_CALLBACK(compute, INTEL_DS_QUEUE_STAGE_COMPUTE)

void
intel_begin_stall(struct intel_ds_device *device,
                  uint64_t ts_ns,
                  const void *flush_data,
                  const struct trace_begin_stall *payload)
{
   const struct intel_ds_flush_data *flush =
      (const struct intel_ds_flush_data *) flush_data;
   begin_event(flush->queue, ts_ns, INTEL_DS_QUEUE_STAGE_STALL);
}

void
intel_end_stall(struct intel_ds_device *device,
                uint64_t ts_ns,
                const void *flush_data,
                const struct trace_end_stall *payload)
{
   const struct intel_ds_flush_data *flush =
      (const struct intel_ds_flush_data *) flush_data;
   end_event(flush->queue, ts_ns, INTEL_DS_QUEUE_STAGE_STALL, flush->submission_id,
             payload,
             (trace_payload_as_extra_func)custom_trace_payload_as_extra_end_stall);
}

uint64_t
intel_ds_begin_submit(struct intel_ds_queue *queue)
{
   return perfetto::base::GetBootTimeNs().count();
}

void
intel_ds_end_submit(struct intel_ds_queue *queue,
                    uint64_t start_ts)
{
   if (!u_trace_context_actively_tracing(&queue->device->trace_context)) {
      /* Force a clock sync at the next enable. */
      queue->device->sync_gpu_ts = 0;
      queue->device->next_clock_sync_ns = 0;
      return;
   }

   uint64_t end_ts = perfetto::base::GetBootTimeNs().count();
   uint32_t submission_id = queue->submission_id++;

   sync_timestamp(queue->device);

   IntelRenderpassDataSource::Trace([=](IntelRenderpassDataSource::TraceContext tctx) {
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

#endif /* HAVE_PERFETTO */

static void
intel_driver_ds_init_once(void)
{
#ifdef HAVE_PERFETTO
   util_perfetto_init();
   perfetto::DataSourceDescriptor dsd;
   dsd.set_name("gpu.renderstages.intel");
   IntelRenderpassDataSource::Register(dsd);
#endif
}

static once_flag intel_driver_ds_once_flag = ONCE_FLAG_INIT;

void
intel_driver_ds_init(void)
{
   call_once(&intel_driver_ds_once_flag,
             intel_driver_ds_init_once);
}

void
intel_ds_device_init(struct intel_ds_device *device,
                     struct intel_device_info *devinfo,
                     int drm_fd,
                     uint32_t gpu_id)
{
   memset(device, 0, sizeof(*device));

   device->gpu_id = gpu_id;
   device->gpu_clock_id = intel_pps_clock_id(gpu_id);
   device->fd = drm_fd;
   device->info = *devinfo;
   u_vector_init(&device->queues, 4, sizeof(struct intel_ds_queue));
}

void
intel_ds_device_fini(struct intel_ds_device *device)
{
   u_trace_context_fini(&device->trace_context);
   u_vector_finish(&device->queues);
}

struct intel_ds_queue *
intel_ds_device_add_queue(struct intel_ds_device *device,
                          const char *fmt_name,
                          ...)
{
   struct intel_ds_queue *queue =
      (struct intel_ds_queue *) u_vector_add(&device->queues);
   va_list ap;

   memset(queue, 0, sizeof(*queue));

   queue->device = device;
   queue->queue_id = u_vector_length(&device->queues) - 1;

   va_start(ap, fmt_name);
   vsnprintf(queue->name, sizeof(queue->name), fmt_name, ap);
   va_end(ap);

   return queue;
}

void intel_ds_flush_data_init(struct intel_ds_flush_data *data,
                              struct intel_ds_queue *queue,
                              uint64_t submission_id)
{
   memset(data, 0, sizeof(*data));

   data->queue = queue;
   data->submission_id = submission_id;

   u_trace_init(&data->trace, &queue->device->trace_context);
}

void intel_ds_flush_data_fini(struct intel_ds_flush_data *data)
{
   u_trace_fini(&data->trace);
}

#ifdef __cplusplus
}
#endif
