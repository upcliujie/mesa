/*
 * Copyright © 2019-2021 Collabora, Ltd.
 * Author: Antonio Caggiano <antonio.caggiano@collabora.com>
 * Author: Rohan Garg <rohan.garg@collabora.com>
 * Author: Robert Beckett <bob.beckett@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "pan_pps_driver.h"

#include <cstring>
#include <perfetto.h>
#include <xf86drm.h>

#include <pps/pps.h>
#include <pps/pps_algorithm.h>

#include "hwc_names.h"

namespace pps
{
constexpr const char *to_string(CounterBlock block)
{
   switch (block) {
   case CounterBlock::JOB_MANAGER:
      return "JOB_MANAGER";
   case CounterBlock::TILER:
      return "TILER";
   case CounterBlock::L2_MMU:
      return "L2_MMU";
   case CounterBlock::SHADER_CORE:
      return "SHADER_CORE";
   default:
      assert(false && "Invalid counter");
      return "INVALID_BLOCK";
   }
}

const std::string &PanfrostDriver::get_name()
{
   static std::string name = "panfrost";
   return name;
}

uint64_t PanfrostDriver::get_min_sampling_period_ns()
{
   return 1000000;
}

std::vector<CounterGroup> create_groups()
{
   std::vector<CounterGroup> ret = {};

   for (uint32_t id = 0; id < static_cast<uint32_t>(CounterBlock::MAX_VALUE); ++id) {
      CounterGroup group = {};
      group.id = id;
      group.name = to_string(static_cast<CounterBlock>(id));

      ret.emplace_back(std::move(group));
   }

   return ret;
}

/// @param gpu_id drm_panfrost_get_param struct that contains the GPU ID
/// @return A list of counter names for the drm device
std::vector<const char *> create_counter_names(const struct drm_panfrost_get_param *gpu_id)
{
   const char *const *base = nullptr;
   size_t size = 0;

   switch (gpu_id->value) {
#define SET_BASE_SIZE(array, base, size)                                                           \
   {                                                                                               \
      base = array;                                                                                \
      size = sizeof(array) / sizeof(array[0]);                                                     \
   }
   case 0x600:
      SET_BASE_SIZE(mali_userspace::hardware_counters_mali_t60x, base, size);
      break;
   case 0x620:
      SET_BASE_SIZE(mali_userspace::hardware_counters_mali_t62x, base, size);
      break;
   case 0x720:
      SET_BASE_SIZE(mali_userspace::hardware_counters_mali_t72x, base, size);
      break;
   case 0x750:
      SET_BASE_SIZE(mali_userspace::hardware_counters_mali_t76x, base, size);
      break;
   case 0x820:
      SET_BASE_SIZE(mali_userspace::hardware_counters_mali_t82x, base, size);
      break;
   case 0x830:
      SET_BASE_SIZE(mali_userspace::hardware_counters_mali_t83x, base, size);
      break;
   case 0x860:
      SET_BASE_SIZE(mali_userspace::hardware_counters_mali_t86x, base, size);
      break;
   case 0x880:
      SET_BASE_SIZE(mali_userspace::hardware_counters_mali_t88x, base, size);
      break;
   default:
      PERFETTO_FATAL("GPU ID not supported %llx", gpu_id->value);
   }

   return std::vector<const char *>(base, base + size);
}

/// @param name Name of the new counter
/// @param numerator Counter to use as numerator
/// @param denominator Counter to use as denominator
/// @return A derived counter which is the ratio of two counters
Counter create_ratio_counter(int32_t id,
   const char *name,
   const Counter &numerator,
   const Counter &denominator)
{
   Counter ret {id, name, numerator.group};
   ret.set_getter(
      [numerator, denominator](const Counter &c, const Driver &driver) -> Counter::Value {
         auto num_v = std::get<int64_t>(numerator.get_value(driver));
         auto den_v = std::get<int64_t>(denominator.get_value(driver));
         return ratio(num_v, den_v);
      });
   return ret;
}

std::vector<Counter>::const_iterator find_by_name(const std::vector<Counter> &counters,
   const char *name)
{
   return FIND_IF(counters, [name](const Counter &c) { return c.name == name; });
}

bool contains(const std::vector<Counter> &counters,
   const std::vector<Counter>::const_iterator &iterator)
{
   return iterator != std::end(counters);
}

void add_ratio_counter(const char *name,
   const char *num_name,
   const char *den_name,
   std::vector<Counter> &counters)
{
   auto num = find_by_name(counters, num_name);
   auto den = find_by_name(counters, den_name);

   if (contains(counters, num) && contains(counters, den)) {
      int32_t id = static_cast<int32_t>(counters.size());
      Counter ratio_counter = create_ratio_counter(id, name, *num, *den);
      ratio_counter.derived = true;
      counters.insert(std::next(num), std::move(ratio_counter));
   }
}

void add_tripipe_counters(std::vector<Counter> &counters)
{
   auto tripipe_active_it = find_by_name(counters, "TRIPIPE_ACTIVE");
   auto gpu_active_it = find_by_name(counters, "GPU_ACTIVE");

   if (contains(counters, tripipe_active_it) && contains(counters, gpu_active_it)) {
      Counter tripipe_active = *tripipe_active_it;
      Counter gpu_active = *gpu_active_it;

      int32_t id = static_cast<int32_t>(counters.size());
      Counter tripipe_usage {id, "TRIPIPE_USAGE", tripipe_active.group};
      tripipe_usage.set_getter(
         [tripipe_active, gpu_active](const Counter &c, const Driver &driver) -> Counter::Value {
            auto num_v = std::get<int64_t>(tripipe_active.get_value(driver));
            auto den_v = std::get<int64_t>(gpu_active.get_value(driver));
            return ratio(
               num_v / double(reinterpret_cast<const PanfrostDriver &>(driver).cores), den_v);
         });

      counters.insert(std::next(tripipe_active_it), std::move(tripipe_usage));
   }

   add_ratio_counter("ARITH_USAGE", "ARITH_WORDS", "TRIPIPE_ACTIVE", counters);
}

void add_load_store_counters(std::vector<Counter> &counters)
{
   add_ratio_counter("LS_USAGE", "LS_WORDS", "TRIPIPE_ACTIVE", counters);
   add_ratio_counter("LS_MICRO_USAGE", "LS_ISSUES", "TRIPIPE_ACTIVE", counters);
   add_ratio_counter("LS_CPI", "LS_WORDS", "LS_ISSUES", counters);
}

void add_load_store_cache_counters(std::vector<Counter> &counters)
{
   add_ratio_counter("LSC_READ_HITRATE", "LSC_READ_HITS", "LSC_READ_OP", counters);
   add_ratio_counter("LSC_WRITE_HITRATE", "LSC_WRITE_HITS", "LSC_WRITE_OP", counters);
   add_ratio_counter("LSC_ATOMIC_HITRATE", "LSC_ATOMIC_HITS", "LSC_ATOMIC_OP", counters);
}

void add_texture_counters(std::vector<Counter> &counters)
{
   add_ratio_counter("TEX_CPI", "TEX_WORDS", "TEX_ISSUES", counters);
}

void add_l2_counters(std::vector<Counter> &counters)
{
   add_ratio_counter("L2_READ_HITRATE", "L2_READ_HIT", "L2_READ_LOOKUP", counters);
   add_ratio_counter("L2_WRITE_HITRATE", "L2_WRITE_HIT", "L2_WRITE_LOOKUP", counters);
}

void add_l2_ext_read_counters(std::vector<Counter> &counters)
{
   auto read_beats_it = find_by_name(counters, "L2_EXT_READ_BEATS");
   // Do not add anything if l2 ext read beats counter is not enabled
   if (!contains(counters, read_beats_it))
      return;

   auto read_beats = *read_beats_it;

   // Add l2 ext read bytes counter
   int32_t id = static_cast<int32_t>(counters.size());
   auto read_bytes = Counter {id, "L2_EXT_READ_BYTES", read_beats.group};
   read_bytes.set_getter([read_beats](const Counter &c, const Driver &driver) {
      auto beats = std::get<int64_t>(read_beats.get_value(driver));
      auto panfrost_driver = static_cast<const PanfrostDriver *>(&driver);
      return beats * panfrost_driver->l2_axi_width;
   });

   counters.insert(std::next(read_beats_it), std::move(read_bytes));

   auto gpu_active_it = find_by_name(counters, "GPU_ACTIVE");
   // Do not add usage if gpu active counter is not enabled
   if (!contains(counters, gpu_active_it))
      return;

   auto gpu_active = *gpu_active_it;

   // Add l2 ext read usage counter
   id = static_cast<int32_t>(counters.size());
   auto read_usage = Counter {id, "L2_EXT_READ_USAGE", read_beats.group};
   read_usage.set_getter([read_beats, gpu_active](const Counter &c, const Driver &driver) {
      auto beats = std::get<int64_t>(read_beats.get_value(driver));
      auto gpu = std::get<int64_t>(gpu_active.get_value(driver));
      auto panfrost_driver = static_cast<const PanfrostDriver *>(&driver);
      return beats / double(gpu * panfrost_driver->l2_axi_port_count);
   });

   counters.insert(std::next(read_beats_it), std::move(read_usage));
}

void add_l2_ext_write_counters(std::vector<Counter> &counters)
{
   auto write_beats_it = find_by_name(counters, "L2_EXT_WRITE_BEATS");
   // Do not add anything if l2 ext write beats counter is not enabled
   if (!contains(counters, write_beats_it))
      return;

   auto write_beats = *write_beats_it;

   // Add l2 ext write bytes counter
   int32_t id = static_cast<int32_t>(counters.size());
   auto write_bytes = Counter {id, "L2_EXT_WRITE_BYTES", write_beats.group};
   write_bytes.set_getter([write_beats](const Counter &c, const Driver &driver) {
      auto beats = std::get<int64_t>(write_beats.get_value(driver));
      auto panfrost_driver = static_cast<const PanfrostDriver *>(&driver);
      return beats * panfrost_driver->l2_axi_width;
   });

   counters.insert(std::next(write_beats_it), std::move(write_bytes));

   auto gpu_active_it = find_by_name(counters, "GPU_ACTIVE");
   // Do not add write usage if gpu active counter is not enabled
   if (!contains(counters, gpu_active_it))
      return;

   auto gpu_active = *gpu_active_it;

   // Add l2 ext write usage counter
   id = static_cast<int32_t>(counters.size());
   auto write_usage = Counter {id, "L2_EXT_WRITE_USAGE", write_beats.group};
   write_usage.set_getter([write_beats, gpu_active](const Counter &c, const Driver &driver) {
      auto beats = std::get<int64_t>(write_beats.get_value(driver));
      auto gpu = std::get<int64_t>(gpu_active.get_value(driver));
      auto panfrost_driver = static_cast<const PanfrostDriver *>(&driver);
      return beats / double(gpu * panfrost_driver->l2_axi_port_count);
   });

   counters.insert(std::next(write_beats_it), std::move(write_usage));
}

void add_l2_ext_counters(std::vector<Counter> &counters)
{
   add_l2_ext_read_counters(counters);
   add_l2_ext_write_counters(counters);
}

void add_derived_counters(std::vector<Counter> &counters)
{
   if (auto it = find_by_name(counters, "JS0_TASKS"); contains(counters, it)) {
      auto js0_tasks = *it;
      int32_t id = static_cast<int32_t>(counters.size());
      auto pixel_count = Counter {id, "PIXEL_COUNT", js0_tasks.group};
      pixel_count.derived = true;
      pixel_count.set_getter([js0_tasks](const Counter &counter, const Driver &driver) {
         auto tasks = std::get<int64_t>(js0_tasks.get_value(driver));
         return tasks * reinterpret_cast<const PanfrostDriver &>(driver).tile_size;
      });
      counters.insert(std::next(it), std::move(pixel_count));
   }

   add_tripipe_counters(counters);
   add_load_store_counters(counters);
   add_load_store_cache_counters(counters);
   add_texture_counters(counters);
   add_l2_counters(counters);
   add_l2_ext_counters(counters);
}

/// @return The block id of a counter given its offset within the counter names
CounterBlock find_block(const uint32_t offset)
{
   int32_t block = offset / 64;

   switch (block) {
   case 0:
   case 1:
      break;
   case 2:
      block = 3;
      break;
   case 3:
      block = 2;
      break;
   default:
      assert(false && "Invalid counter block");
      return CounterBlock::UNDEFINED;
   }

   return static_cast<CounterBlock>(block);
}

std::pair<std::vector<CounterGroup>, std::vector<Counter>>
PanfrostDriver::create_available_counters(const std::vector<const char *> &counter_names)
{
   std::pair<std::vector<CounterGroup>, std::vector<Counter>> ret;
   auto &[groups, counters] = ret;

   groups = create_groups();

   // Map counters names to Counter struct
   int32_t id = 0;
   for (size_t offset = 0; offset < counter_names.size(); ++offset) {
      const char *name = counter_names[offset];

      // Skip empty counter names
      if (std::strcmp(name, "") == 0) {
         continue;
      }

      // Add this new counter to the right group
      auto group_id = static_cast<int32_t>(find_block(offset));
      auto &group = groups[group_id];
      group.counters.push_back(id);

      // Increment the id only with valid counter names
      auto counter = Counter {id++, name, group_id};
      counter.offset = static_cast<int32_t>(offset);
      counters.emplace_back(counter);
   }

   add_derived_counters(counters);

   return ret;
}

/// This is a default getter for Mali performance counters
Counter::Value Counter::default_getter(const Counter &counter, const Driver &dri)
{
   auto &panfrost = reinterpret_cast<const PanfrostDriver &>(dri);

   int32_t block_index = counter.group;
   uint32_t block_offset = counter.offset % panfrost.counters_per_block;

   int64_t value = panfrost.samples[block_index * panfrost.counters_per_block + block_offset];

   // Shader cores
   if (counter.group == static_cast<int32_t>(CounterBlock::SHADER_CORE)) {
      // Accumulate values from other cores
      for (size_t core = 1; core < panfrost.cores; ++core) {
         size_t pos = (block_index + core) * panfrost.counters_per_block + block_offset;
         value += panfrost.samples[pos];
      }
   }

   return value;
}

/// @return The number of cores of the GPU
uint32_t query_core_count(const int card_fd)
{
   if (card_fd <= 0) {
      PERFETTO_FATAL("Invalid GPU file descriptor");
      return 0;
   }

   struct drm_panfrost_get_param get_param = {
      0,
   };
   get_param.param = DRM_PANFROST_PARAM_SHADER_PRESENT;
   auto ret = drmIoctl(card_fd, DRM_IOCTL_PANFROST_GET_PARAM, &get_param);

   if (!check(ret, "Could not query GPU shader cores"))
      return 0;

   auto present = get_param.value;

   uint32_t cores = 0;

   while (present) {
      ++cores;
      present >>= 1;
   }

   return cores;
}

size_t PanfrostDriver::query_counters_count(const uint32_t cores)
{
   if (cores <= 0) {
      PERFETTO_FATAL("Invalid number of cores");
      // Guess a default size
      return PanfrostDriver::counters_per_block * 20;
   }

   // There are also blocks for job manager, tiler, and L2 / MMU
   auto blocks = cores + 3;

   return PanfrostDriver::counters_per_block * blocks;
}

uint32_t query_tile_size(const struct drm_panfrost_get_param *gpu_id)
{
   switch (gpu_id->value) {
   case 0x600:
   case 0x620:
   case 0x720:
      return 16 * 16;

   case 0x750:
   case 0x800:
   default:
      return 32 * 32;
   }
}

struct drm_panfrost_get_param query_gpu_id(int card_fd)
{
   if (card_fd <= 0) {
      PERFETTO_FATAL("Invalid GPU file descriptor");
      return {};
   }

   struct drm_panfrost_get_param gpu_id = {
      0,
   };
   gpu_id.param = DRM_PANFROST_PARAM_GPU_PROD_ID;
   auto ret = drmIoctl(card_fd, DRM_IOCTL_PANFROST_GET_PARAM, &gpu_id);

   if (!check(ret, "Could not query GPU ID")) {
      return {};
   }

   return gpu_id;
}

bool PanfrostDriver::init_perfcnt()
{
   gpu_id = query_gpu_id(drm_device.fd);
   cores = query_core_count(drm_device.fd);
   std::tie(groups, counters) = create_available_counters(create_counter_names(&gpu_id));
   size_t counters_count = query_counters_count(cores);
   samples.resize(counters_count);
   tile_size = query_tile_size(&gpu_id);

   return true;
}

void PanfrostDriver::enable_counter(const uint32_t counter_id)
{
   enabled_counters.push_back(counters[counter_id]);
}

void PanfrostDriver::enable_all_counters()
{
   enabled_counters.reserve(counters.size());
   for (auto &counter : counters) {
      enabled_counters.push_back(counter);
   }
}

void PanfrostDriver::enable_perfcnt(const uint64_t /* sampling_period_ns */)
{
   drm_panfrost_perfcnt_enable perfcnt = {};
   perfcnt.enable = 1;
   perfcnt.counterset = 0;

   auto res = drmIoctl(drm_device.fd, DRM_IOCTL_PANFROST_PERFCNT_ENABLE, &perfcnt);
   if (!check(res, "Cannot enable performance counters")) {
      if (res == -ENOSYS) {
         PERFETTO_FATAL("Please enable unstable ioctls with: modprobe panfrost unstable_ioctls=1");
      }
      PERFETTO_FATAL("Please verify graphics card");
   }
}

bool PanfrostDriver::dump_perfcnt()
{
   // Dump performance counters to buffer
   drm_panfrost_perfcnt_dump dump = {};
   dump.buf_ptr = reinterpret_cast<uintptr_t>(samples.data());

   last_dump_ts = perfetto::base::GetBootTimeNs().count();
   auto res = drmIoctl(drm_device.fd, DRM_IOCTL_PANFROST_PERFCNT_DUMP, &dump);
   if (!check(res, "Cannot dump")) {
      PERFETTO_ELOG("Skipping sample");
      return false;
   }

   return true;
}

uint64_t PanfrostDriver::next()
{
   auto ret = last_dump_ts;
   last_dump_ts = 0;
   return ret;
}

void PanfrostDriver::disable_perfcnt()
{
   drm_panfrost_perfcnt_enable perfcnt = {};
   perfcnt.enable = 0; // disable
   auto res = drmIoctl(drm_device.fd, DRM_IOCTL_PANFROST_PERFCNT_ENABLE, &perfcnt);
   check(res, "Cannot disable perfcnt");
}

} // namespace pps
