/*
 * Copyright © 2019-2020 Collabora, Ltd.
 * Author: Antonio Caggiano <antonio.caggiano@collabora.com>
 * Author: Robert Beckett <bob.beckett@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdlib>

#include "pps_datasource.h"

int main(int argc, const char **argv)
{
   using namespace pps;

   // Connects to the system tracing service
   perfetto::TracingInitArgs args;
   args.backends = perfetto::kSystemBackend;
   perfetto::Tracing::Initialize(args);

   GpuDataSource::register_data_source();

   while (true) {
      GpuDataSource::Trace(GpuDataSource::trace_callback);
   }

   return EXIT_SUCCESS;
}
