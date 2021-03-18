/*
 * Copyright © 2019-2021 Collabora, Ltd.
 * Author: Antonio Caggiano <antonio.caggiano@collabora.com>
 * Author: Robert Beckett <bob.beckett@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "pps.h"
#include "pps_driver.h"

namespace pps
{
struct GpuIncrementalState {
   bool was_cleared = true;
};

struct GpuDataSourceTraits : public perfetto::DefaultDataSourceTraits {
   using IncrementalStateType = GpuIncrementalState;
};

class Driver;

class GpuDataSource : public perfetto::DataSource<GpuDataSource, GpuDataSourceTraits>
{
   public:
   void OnSetup(const SetupArgs &args) override;
   void OnStart(const StartArgs &args) override;
   void OnStop(const StopArgs &args) override;

   /// @brief Perfetto trace callback
   static void trace_callback(TraceContext ctx);
   static void register_data_source();

   void trace(TraceContext &ctx);

   private:
   State state = State::Stop;

   /// Time between trace callbacks
   std::chrono::nanoseconds time_to_sleep = std::chrono::nanoseconds(1000000);

   /// Used to check whether the datasource is quick enough
   std::chrono::nanoseconds time_to_trace;

   std::vector<std::unique_ptr<Driver>> drivers;

   /// Timestamp of packet sent with counter descriptors
   uint64_t descriptor_timestamp = 0;
};

} // namespace pps
