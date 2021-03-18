/*
 * Copyright © 2020 Collabora, Ltd.
 * Author: Antonio Caggiano <antonio.caggiano@collabora.com>
 * Author: Robert Beckett <bob.beckett@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "pps_counter.h"
#include "pps_device.h"

namespace pps
{
/// @brief Abstract Driver class
class Driver
{
   public:
   /// @return A list of supported DRM device names
   const std::vector<std::string> &supported_device_names();

   /// @return A new driver supporting a specific DRM device, nullptr if not supported
   static std::unique_ptr<Driver> create(DrmDevice &&drm_device);

   Driver() = default;
   virtual ~Driver() = default;

   // Forbid copy
   Driver(const Driver &) = delete;
   Driver &operator=(const Driver &) = delete;

   /// @return The minimum sampling period for the current device
   virtual uint64_t get_min_sampling_period_ns() = 0;

   /// @brief Enable a counter by its ID
   virtual void enable_counter(uint32_t counter_id) = 0;

   virtual void enable_all_counters() = 0;

   /// @brief Initialize performance counters data such as groups and counters
   /// @return Whether it was successful or not
   virtual bool init_perfcnt() = 0;

   /// @brief Enables performance counters, meaning that from now on they can be sampled
   virtual void enable_perfcnt(uint64_t sampling_period_ns) = 0;

   /// @brief Disables performance counters on the device
   virtual void disable_perfcnt() = 0;

   /// @brief Asking the GPU to dump performance counters could have different meanings
   /// depending on the concrete driver. Some could just ask the GPU to dump counters to a
   /// user space buffer, while some others will need to read data from a stream which was
   /// written asynchronously.
   /// @return Whether it was able to dump, false otherwise
   virtual bool dump_perfcnt() = 0;

   /// @brief After dumping performance counters, with this function you can iterate
   /// through the samples collected.
   /// @return The CPU timestamp associated to current sample, or 0 if there are no more samples
   virtual uint64_t next() = 0;

   DrmDevice drm_device;

   /// List of counter groups
   std::vector<CounterGroup> groups;

   /// List of counters exposed by the GPU
   std::vector<Counter> counters;

   /// List of counters that are actually enabled
   std::vector<Counter> enabled_counters;

   /// Memory where to dump performance counters
   std::vector<uint32_t> samples;

   protected:
   // Prevent object slicing by allowing move only from subclasses
   Driver(Driver &&) = default;
   Driver &operator=(Driver &&) = default;
};

} // namespace pps
