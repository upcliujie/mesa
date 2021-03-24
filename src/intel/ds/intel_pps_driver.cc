/*
 * Copyright © 2020-2021 Collabora, Ltd.
 * Author: Antonio Caggiano <antonio.caggiano@collabora.com>
 * Author: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "intel_pps_driver.h"

#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <i915_drm.h>
#include <perf_data_reader.h>

#include <pps/pps.h>
#include <pps/pps_algorithm.h>

namespace pps
{
const std::string &IntelDriver::get_name()
{
   static std::string name = "i915";
   return name;
}

uint64_t IntelDriver::get_min_sampling_period_ns()
{
   return 500000;
}

IntelDriver::IntelDriver(IntelDriver &&other)
   : Driver {std::move(other)}
   , sampling_period_ns {other.sampling_period_ns}
   , timestamp_frequency {other.timestamp_frequency}
   , perf {other.perf}
   , accu {other.accu}
   , last_cpu_timestamp {other.last_cpu_timestamp}
   , correlations {std::move(other.correlations)}
   , metric_buffer {std::move(other.metric_buffer)}
   , total_bytes_read {other.total_bytes_read}
   , records {std::move(other.records)}
   , metric_set {other.metric_set}
   , metric_fd {other.metric_fd}
{
   other.perf = nullptr;
   other.metric_set = nullptr;
   other.metric_fd = -1;
}

IntelDriver &IntelDriver::operator=(IntelDriver &&other)
{
   if (this == &other) {
      return *this;
   }

   Driver::operator=(std::move(other));
   std::swap(sampling_period_ns, other.sampling_period_ns);
   std::swap(timestamp_frequency, other.timestamp_frequency);
   std::swap(perf, other.perf);
   std::swap(accu, other.accu);
   std::swap(last_cpu_timestamp, other.last_cpu_timestamp);
   std::swap(correlations, other.correlations);
   std::swap(metric_buffer, other.metric_buffer);
   std::swap(total_bytes_read, other.total_bytes_read);
   std::swap(records, other.records);
   std::swap(metric_set, other.metric_set);
   std::swap(metric_fd, other.metric_fd);

   return *this;
}

IntelDriver::~IntelDriver()
{
   if (metric_fd >= 0) {
      close(metric_fd);
   }

   if (perf) {
      intel_perf_free(perf);
   }
}

static intel_perf_metric_set *query_metric_set_by_name(const intel_perf &perf,
   const std::string &metric_set)
{
   intel_perf_metric_set *it = nullptr;
   intel_perf_metric_set *ret = nullptr;

   igt_list_for_each_entry(it, &perf.metric_sets, link)
   {
      if (it->symbol_name == metric_set) {
         ret = it;
         break;
      }
   }

   return ret;
}

void IntelDriver::enable_counter(uint32_t counter_id)
{
   auto &counter = counters[counter_id];
   auto &group = groups[counter.group];
   if (metric_set != nullptr) {
      if (metric_set->symbol_name != group.name) {
         PPS_LOG_ERROR(
            "Unable to enable metrics from different sets: %u "
            "belongs to %s but %s is currently in use.",
            counter_id,
            metric_set->symbol_name,
            group.name.c_str());
         return;
      }
   }

   enabled_counters.emplace_back(counter);
   if (metric_set == nullptr) {
      metric_set = query_metric_set_by_name(*perf, group.name);
   }
}

void IntelDriver::enable_all_counters()
{
   // We can only enable one metric set at a time so at least enable one.
   for (auto &group : groups) {
      if (group.name == "RenderBasic") {
         for (uint32_t counter_id : group.counters) {
            auto &counter = counters[counter_id];
            enabled_counters.emplace_back(counter);
         }

         metric_set = query_metric_set_by_name(*perf, group.name);
         break;
      }
   }
}

static int perf_ioctl(int fd, unsigned long request, void *arg)
{
   int ret;

   do {
      ret = ioctl(fd, request, arg);
   } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

   return ret;
}

static uint64_t timespec_diff(timespec *begin, timespec *end)
{
   return 1000000000ull * (end->tv_sec - begin->tv_sec) + end->tv_nsec - begin->tv_nsec;
}

/// @brief This function tries to correlate CPU time with GPU time
std::optional<intel_perf_record_timestamp_correlation>
IntelDriver::query_correlation_timestamps() const
{
   intel_perf_record_timestamp_correlation corr = {};

   clock_t correlation_clock_id = CLOCK_BOOTTIME;

   drm_i915_reg_read reg_read = {};
   const uint64_t render_ring_timestamp = 0x2358;
   reg_read.offset = render_ring_timestamp | I915_REG_READ_8B_WA;

   constexpr size_t attempt_count = 3;
   struct {
      timespec cpu_ts_begin;
      timespec cpu_ts_end;
      uint64_t gpu_ts;
   } attempts[attempt_count] = {};

   uint32_t best = 0;

   // Gather 3 correlations
   for (uint32_t i = 0; i < attempt_count; i++) {
      clock_gettime(correlation_clock_id, &attempts[i].cpu_ts_begin);
      if (perf_ioctl(drm_device.fd, DRM_IOCTL_I915_REG_READ, &reg_read) < 0) {
         return std::nullopt;
      }
      clock_gettime(correlation_clock_id, &attempts[i].cpu_ts_end);

      attempts[i].gpu_ts = reg_read.val;
   }

   // Now select the best
   for (uint32_t i = 1; i < attempt_count; i++) {
      if (timespec_diff(&attempts[i].cpu_ts_begin, &attempts[i].cpu_ts_end) <
         timespec_diff(&attempts[best].cpu_ts_begin, &attempts[best].cpu_ts_end)) {
         best = i;
      }
   }

   corr.cpu_timestamp =
      (attempts[best].cpu_ts_begin.tv_sec * 1000000000ull + attempts[best].cpu_ts_begin.tv_nsec) +
      timespec_diff(&attempts[best].cpu_ts_begin, &attempts[best].cpu_ts_end) / 2;
   corr.gpu_timestamp = attempts[best].gpu_ts;

   return corr;
}

static uint64_t query_timestamp_frequency(const DrmDevice &drm_device)
{
   int timestamp_frequency;

   drm_i915_getparam_t gp = {};
   gp.param = I915_PARAM_CS_TIMESTAMP_FREQUENCY;
   gp.value = &timestamp_frequency;
   if (perf_ioctl(drm_device.fd, DRM_IOCTL_I915_GETPARAM, &gp) == 0) {
      return timestamp_frequency;
   }

   PPS_LOG_ERROR("Unable to query timestamp frequency from i915, guessing values...");
   return 12000000;
}

void IntelDriver::get_new_correlation()
{
   // Rotate left correlations by one position so to make space at the end
   std::rotate(correlations.begin(), correlations.begin() + 1, correlations.end());

   // Then we overwrite the last correlation with a new one
   if (auto corr = query_correlation_timestamps()) {
      correlations.back() = *corr;
   } else {
      PPS_LOG_FATAL("Failed to get correlation timestamps");
   }
}

bool IntelDriver::init_perfcnt()
{
   // Initialize intel Perf
   assert(!perf && "Perf data for i915 should not be valid at this point");
   perf = intel_perf_for_fd(drm_device.fd);
   if (!perf) {
      PPS_LOG_ERROR("Failed to find perf data for i915");
      return false;
   }
   intel_perf_load_perf_configs(perf, drm_device.fd);

   // Find groups and counters
   intel_perf_metric_set *metric_set = nullptr;
   igt_list_for_each_entry(metric_set, &perf->metric_sets, link)
   {
      // Create group
      CounterGroup group = {};
      group.id = groups.size();
      group.name = metric_set->symbol_name;

      for (int i = 0; i < metric_set->n_counters; ++i) {
         intel_perf_logical_counter &counter = metric_set->counters[i];

         // Create counter
         Counter counter_desc = {};
         counter_desc.id = counters.size();
         counter_desc.name = counter.symbol_name;
         counter_desc.group = group.id;
         counter_desc.getter = [counter, metric_set](
                                  const Counter &c, const Driver &dri) -> Counter::Value {
            auto &intel = reinterpret_cast<const IntelDriver &>(dri);
            switch (counter.storage) {
            case INTEL_PERF_LOGICAL_COUNTER_STORAGE_UINT64:
            case INTEL_PERF_LOGICAL_COUNTER_STORAGE_UINT32:
            case INTEL_PERF_LOGICAL_COUNTER_STORAGE_BOOL32:
               return (int64_t)counter.read_uint64(
                  intel.perf, metric_set, const_cast<uint64_t *>(intel.accu.deltas));
               break;
            case INTEL_PERF_LOGICAL_COUNTER_STORAGE_DOUBLE:
            case INTEL_PERF_LOGICAL_COUNTER_STORAGE_FLOAT:
               return counter.read_float(
                  intel.perf, metric_set, const_cast<uint64_t *>(intel.accu.deltas));
               break;
            }

            return {};
         };

         // Add counter id to the group
         group.counters.emplace_back(counter_desc.id);

         // Store counter
         counters.emplace_back(std::move(counter_desc));
      }

      // Store group
      groups.emplace_back(std::move(group));
   }

   assert(groups.size() && "Failed to query groups");
   assert(counters.size() && "Failed to query counters");

   timestamp_frequency = query_timestamp_frequency(drm_device);

   return true;
}

int IntelDriver::perf_open(const intel_perf_metric_set &metric_set,
   const uint64_t sampling_period_ns)
{
   assert(timestamp_frequency > 0 && "Invalid timestamp frequency");

   uint64_t properties[DRM_I915_PERF_PROP_MAX * 2];
   uint32_t p = 0;

   properties[p++] = DRM_I915_PERF_PROP_SAMPLE_OA;
   properties[p++] = true;

   properties[p++] = DRM_I915_PERF_PROP_OA_METRICS_SET;
   properties[p++] = metric_set.perf_oa_metrics_set;

   properties[p++] = DRM_I915_PERF_PROP_OA_FORMAT;
   properties[p++] = metric_set.perf_oa_format;

   // The period_exponent gives a sampling period as follows:
   // sample_period = timestamp_period * 2^(period_exponent + 1)
   // where timestamp_period is 80ns for Haswell+
   auto oa_exponent = (uint32_t)log2(sampling_period_ns * timestamp_frequency / 1000000000ull) - 1;
   properties[p++] = DRM_I915_PERF_PROP_OA_EXPONENT;
   properties[p++] = oa_exponent;

   struct drm_i915_perf_open_param param = {};
   param.flags = 0;
   param.flags |= I915_PERF_FLAG_FD_CLOEXEC | I915_PERF_FLAG_FD_NONBLOCK;
   param.properties_ptr = (uintptr_t)properties;
   param.num_properties = p / 2;

   auto stream_fd = perf_ioctl(drm_device.fd, DRM_IOCTL_I915_PERF_OPEN, &param);
   return stream_fd;
}

void IntelDriver::enable_perfcnt(const uint64_t sampling_period_ns)
{
   this->sampling_period_ns = sampling_period_ns;

   // Fill correlations with an initial one
   if (auto corr = query_correlation_timestamps()) {
      correlations.fill(*corr);
   } else {
      PPS_LOG_FATAL("Failed to get correlation timestamps");
   }

   assert(metric_set && "Metric set not found during initialization");
   assert(metric_fd < 0 && "Metric set FD should not be valid at this point");

   // Open metric set stream
   metric_fd = perf_open(*metric_set, sampling_period_ns);
   if (metric_fd < 0) {
      PPS_LOG_ERROR("Failed to open perf: not enough permissions for system-wide analysis?");
   }
}

/// @brief Transforms the GPU timestop into a CPU timestamp equivalent
uint64_t IntelDriver::correlate_gpu_timestamp(const uint32_t gpu_ts)
{
   auto &corr_a = correlations[0];
   auto &corr_b = correlations[correlations.size() - 1];

   // A correlation timestamp has 36 bits, so get the first 32 to make it work with gpu_ts
   uint64_t mask = 0xffffffff;
   uint32_t corr_a_gpu_ts = corr_a.gpu_timestamp & mask;
   uint32_t corr_b_gpu_ts = corr_b.gpu_timestamp & mask;

   // Make sure it is within the interval [a,b)
   assert(gpu_ts >= corr_a_gpu_ts && "GPU TS < Corr a");
   assert(gpu_ts < corr_b_gpu_ts && "GPU TS >= Corr b");

   uint32_t gpu_delta = gpu_ts - corr_a_gpu_ts;
   // Factor to convert gpu time to cpu time
   double gpu_to_cpu = (corr_b.cpu_timestamp - corr_a.cpu_timestamp) /
      double(corr_b.gpu_timestamp - corr_a.gpu_timestamp);
   uint64_t cpu_delta = gpu_delta * gpu_to_cpu;
   return corr_a.cpu_timestamp + cpu_delta;
}

void IntelDriver::disable_perfcnt()
{
   if (metric_fd < 0) {
      PPS_LOG_ERROR("Performance counters were not enabled");
      return;
   }

   close(metric_fd);
   metric_fd = -1;
}

struct Report {
   uint32_t version;
   uint32_t timestamp;
   uint32_t id;
};

/// @brief Some perf record durations can be really short
/// @return True if the duration is at least close to the sampling period
static bool close_enough(uint64_t duration, uint64_t sampling_period)
{
   return duration > sampling_period - 100000;
}

/// @brief Transforms the raw data received in from the driver into records
std::vector<PerfRecord> IntelDriver::parse_perf_records(const std::vector<uint8_t> &data,
   const size_t byte_count)
{
   std::vector<PerfRecord> records;
   records.reserve(128);

   PerfRecord record;
   record.reserve(512);

   const uint8_t *iter = data.data();
   const uint8_t *end = iter + byte_count;

   uint64_t prev_cpu_timestamp = last_cpu_timestamp;

   while (iter < end) {
      // Iterate a record at a time
      auto header = reinterpret_cast<const drm_i915_perf_record_header *>(iter);

      if (header->type == DRM_I915_PERF_RECORD_SAMPLE) {
         // Report is next to the header
         auto report = reinterpret_cast<const Report *>(header + 1);
         auto cpu_timestamp = correlate_gpu_timestamp(report->timestamp);
         auto duration = cpu_timestamp - prev_cpu_timestamp;

         // Skip perf-records that are too short by checking
         // the distance between last report and this one
         if (close_enough(duration, sampling_period_ns)) {
            prev_cpu_timestamp = cpu_timestamp;

            // Add the new record to the list
            record.resize(header->size); // Possibly 264?
            memcpy(record.data(), iter, header->size);
            records.emplace_back(record);
         }
      }

      // Go to the next record
      iter += header->size;
   }

   return records;
}

/// @brief Read all the available data from the metric set currently in use
void IntelDriver::read_data_from_metric_set()
{
   assert(metric_buffer.size() >= 1024 && "Metric buffer should have space for reading");

   int bytes_read = 0;
   while ((bytes_read = read(metric_fd,
              metric_buffer.data() + total_bytes_read,
              metric_buffer.size() - total_bytes_read)) > 0 ||
      errno == EINTR) {
      total_bytes_read += std::max(0, bytes_read);

      // Increase size of the buffer for the next read
      if (metric_buffer.size() / 2 < total_bytes_read) {
         metric_buffer.resize(metric_buffer.size() * 2);
      }
   }

   assert(total_bytes_read < metric_buffer.size() && "Buffer not big enough");
}

bool IntelDriver::dump_perfcnt()
{
   pollfd pfd = {metric_fd, POLLIN, 0};
   if (poll(&pfd, 1, 0) < 0) {
      PPS_LOG_ERROR("Error while polling metric fd");
      return false;
   }

   if (!(pfd.revents & POLLIN)) {
      // Metric fd is not ready to read yet
      return false;
   }

   read_data_from_metric_set();

   get_new_correlation();

   auto new_records = parse_perf_records(metric_buffer, total_bytes_read);
   if (new_records.empty()) {
      // No new records from the GPU yet
      return false;
   } else {
      // Records are parsed correctly, so we can reset the
      // number of bytes read so far from the metric set
      total_bytes_read = 0;
   }

   APPEND(records, new_records);

   if (records.size() < 2) {
      // Not enough records to accumulate
      return false;
   }

   return true;
}

/// @brief Adds accumulation src to dst
static void add(const intel_perf_accumulator &src, intel_perf_accumulator &dest)
{
   const size_t delta_count = sizeof(dest.deltas) / sizeof(dest.deltas[0]);
   for (size_t i = 0; i < delta_count; ++i) {
      dest.deltas[i] += src.deltas[i];
   }
}

uint32_t IntelDriver::gpu_next()
{
   if (records.size() < 2) {
      // Not enough records to accumulate
      return 0;
   }

   // Get first and second
   auto record_a = reinterpret_cast<const drm_i915_perf_record_header *>(records[0].data());
   auto record_b = reinterpret_cast<const drm_i915_perf_record_header *>(records[1].data());

   intel_perf_accumulator temp_accumulator;
   intel_perf_accumulate_reports(&temp_accumulator, metric_set->perf_oa_format, record_a, record_b);
   add(temp_accumulator, accu);

   // Get last timestamp
   auto report_b = reinterpret_cast<const Report *>(record_b + 1);
   auto gpu_timestamp = report_b->timestamp;

   // Consume first record
   records.erase(std::begin(records), std::begin(records) + 1);

   return gpu_timestamp;
}

uint64_t IntelDriver::cpu_next()
{
   if (auto gpu_timestamp = gpu_next()) {
      auto cpu_timestamp = correlate_gpu_timestamp(gpu_timestamp);

      last_cpu_timestamp = cpu_timestamp;
      return cpu_timestamp;
   }

   return 0;
}

uint64_t IntelDriver::next()
{
   // Reset accumulation
   accu = {};
   return cpu_next();
}

} // namespace pps
