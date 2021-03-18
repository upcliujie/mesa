/*
 * Copyright © 2020-2021 Collabora, Ltd.
 * Author: Antonio Caggiano <antonio.caggiano@collabora.com>
 * Author: Rohan Garg <rohan.garg@collabora.com>
 * Author: Robert Beckett <bob.beckett@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <pps/pps_driver.h>

#include <drm/panfrost_drm.h>

namespace pps
{
enum class CounterBlock {
   UNDEFINED = -1,

   JOB_MANAGER = 0,
   TILER = 1,
   L2_MMU = 2,
   SHADER_CORE = 3,

   MAX_VALUE
};

/// @return A string representation for the counter block
constexpr const char *to_string(CounterBlock block);

/// @brief Panfrost implementation of DRM device
class PanfrostDriver : public Driver
{
   public:
   /// @param A list of mali counter names
   /// @return A pair with two lists: counter groups and available counters
   static std::pair<std::vector<CounterGroup>, std::vector<Counter>> create_available_counters(
      const std::vector<const char *> &counter_names);

   /// @return The number of performance counters
   static size_t query_counters_count(const uint32_t cores);

   static constexpr uint32_t counters_per_block = 64;
   struct drm_panfrost_get_param gpu_id = {
      0,
   };

   /// @todo Some implementations use 8 bytes interfaces instead of 16
   static constexpr uint32_t l2_axi_width = 16;

   /// @todo RK3399 has a Mali T860 MP4 (quad-core)
   static constexpr uint32_t l2_axi_port_count = 4;

   static const std::string &get_name();

   uint64_t get_min_sampling_period_ns() override;
   bool init_perfcnt() override;
   void enable_counter(uint32_t counter_id) override;
   void enable_all_counters() override;
   void enable_perfcnt(uint64_t sampling_period_ns) override;
   void disable_perfcnt() override;
   bool dump_perfcnt() override;
   uint64_t next() override;

   /// Number of cores
   uint32_t cores = 0;

   uint32_t tile_size = 0;

   uint64_t last_dump_ts = 0;
};

inline PanfrostDriver &to_panfrost(Driver &dri)
{
   return reinterpret_cast<PanfrostDriver &>(dri);
}

inline const PanfrostDriver &to_panfrost(const Driver &dri)
{
   return reinterpret_cast<const PanfrostDriver &>(dri);
}

} // namespace pps
