/*
 * Copyright © 2021 Google, Inc.
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

#include "c11/threads.h"

#include "u_perfetto.h"


class MesaLogDataSource : public perfetto::DataSource<MesaLogDataSource> {
public:
   void OnSetup(const SetupArgs&) override {
      // Use this callback to apply any custom configuration to your data source
      // based on the TraceConfig in SetupArgs.
   }

   void OnStart(const StartArgs&) override {
      // This notification can be used to initialize the GPU driver, enable
      // counters, etc. StartArgs will contains the DataSourceDescriptor,
      // which can be extended.
   }

   void OnStop(const StopArgs&) override {
   }
};

PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(MesaLogDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(MesaLogDataSource);

static void
util_perfetto_init_once(void)
{
   // Connects to the system tracing service
   perfetto::TracingInitArgs args;
   args.backends = perfetto::kSystemBackend;
   perfetto::Tracing::Initialize(args);

   perfetto::DataSourceDescriptor dsd;
   dsd.set_name("gpu.log");
   MesaLogDataSource::Register(dsd);
}

static once_flag perfetto_once_flag = ONCE_FLAG_INIT;

void
util_perfetto_init(void)
{
   call_once(&perfetto_once_flag, util_perfetto_init_once);
}

void
util_perfetto_log(enum mesa_log_level level, const char *tag, const char *format, ...)
{
   va_list va;

   va_start(va, format);
   util_perfetto_log_v(level, tag, format, va);
   va_end(va);
}

static inline perfetto::protos::pbzero::GpuLog_Severity
level_to_perfetto(enum mesa_log_level level)
{
   switch (level) {
   case MESA_LOG_ERROR: return perfetto::protos::pbzero::GpuLog::LOG_SEVERITY_ERROR;
   case MESA_LOG_WARN: return perfetto::protos::pbzero::GpuLog::LOG_SEVERITY_WARNING;
   case MESA_LOG_INFO: return perfetto::protos::pbzero::GpuLog::LOG_SEVERITY_INFO;
   case MESA_LOG_DEBUG: return perfetto::protos::pbzero::GpuLog::LOG_SEVERITY_DEBUG;
   }

   unreachable("bad mesa_log_level");
}

void
util_perfetto_log_v(enum mesa_log_level level, const char *tag, const char *format,
                    va_list va)
{
   MesaLogDataSource::Trace([=](MesaLogDataSource::TraceContext tctx) {
      auto packet = tctx.NewTracePacket();

      packet->set_timestamp(perfetto::base::GetBootTimeNs().count());

      auto event = packet->set_gpu_log();
      event->set_severity(level_to_perfetto(level));
      event->set_tag(tag);

      char *msg = NULL;
      if (vasprintf(&msg, format, va) >= 0) {
         event->set_log_message(msg);
         free(msg);
      }
   });
}
