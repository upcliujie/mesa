/*
 * Copyright © 2020-2021 Collabora, Ltd.
 * Author: Antonio Caggiano <antonio.caggiano@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <i915-perf/perf.h>
#include <i915-perf/perf_data.h>

#include <pps/pps_driver.h>

namespace pps
{
/// @brief Variable length sequence of bytes generated
/// by Intel Obstervation Architecture (OA)
using PerfRecord = std::vector<uint8_t>;

/// @brief Driver class for Intel graphics devices
class IntelDriver : public Driver
{
   public:
   static const std::string &get_name();

   IntelDriver() = default;

   IntelDriver(IntelDriver &&);
   IntelDriver &operator=(IntelDriver &&);

   ~IntelDriver() override;

   std::optional<intel_perf_record_timestamp_correlation> query_correlation_timestamps() const;
   void get_new_correlation();

   /// @brief OA reports only have the lower 32 bits of the timestamp
   /// register, while correlation data has the whole 36 bits.
   /// @param gpu_ts a 32 bit OA report GPU timestamp
   /// @return The CPU timestamp relative to the argument
   uint64_t correlate_gpu_timestamp(uint32_t gpu_ts);

   int perf_open(const intel_perf_metric_set &metric_set, uint64_t sampling_period_ns);

   uint64_t get_min_sampling_period_ns() override;
   bool init_perfcnt() override;
   void enable_counter(uint32_t counter_id) override;
   void enable_all_counters() override;
   void enable_perfcnt(uint64_t sampling_period_ns) override;
   void disable_perfcnt() override;
   bool dump_perfcnt() override;
   uint64_t next() override;

   /// @brief Requests the next perf sample
   /// @return The sample GPU timestamp
   uint32_t gpu_next();

   /// @brief Requests the next perf sample accumulating those which
   /// which duration is shorter than the requested sampling period
   /// @return The sample CPU timestamp
   uint64_t cpu_next();

   /// @param data Buffer of bytes to parse
   /// @param byte_count Number of bytes to parse
   /// @return A list of perf records parsed from raw data passed as input
   std::vector<PerfRecord> parse_perf_records(const std::vector<uint8_t> &data, size_t byte_count);

   /// @brief Reads data from the GPU metric set
   void read_data_from_metric_set();

   /// Sampling period in nanoseconds requested by the datasource
   uint64_t sampling_period_ns = 0;

   uint64_t timestamp_frequency = 0;
   intel_perf *perf = nullptr;
   intel_perf_accumulator accu = {};

   /// Keep track of the timestamp of the last sample generated
   uint64_t last_cpu_timestamp = 0;

   /// This is used to correlate CPU and GPU timestamps
   std::array<intel_perf_record_timestamp_correlation, 64> correlations;

   /// Data buffer used to store data read from the metric set
   std::vector<uint8_t> metric_buffer = std::vector<uint8_t>(1024, 0);
   /// Number of bytes read so far still un-parsed.
   /// Reset once bytes from the metric buffer are parsed to perf records
   size_t total_bytes_read = 0;

   /// List of OA perf records read so far
   std::vector<PerfRecord> records;

   intel_perf_metric_set *metric_set = nullptr;
   /// Stream file descriptor for a configured metric set
   int metric_fd = -1;
};

} // namespace pps
