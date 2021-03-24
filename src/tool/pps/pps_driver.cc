/*
 * Copyright © 2019-2020 Collabora, Ltd.
 * Author: Antonio Caggiano <antonio.caggiano@collabora.com>
 * Author: Rohan Garg <rohan.garg@collabora.com>
 * Author: Robert Beckett <bob.beckett@collabora.com>
 * Author: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "pps_driver.h"

#include <iterator>
#include <sstream>

#ifdef PPS_PANFROST
#include "panfrost/ds/pan_pps_driver.h"
#endif // PPS_PANFROST

#ifdef PPS_INTEL
#include "intel/ds/intel_pps_driver.h"
#endif // PPS_INTEL

#include "pps.h"
#include "pps_algorithm.h"

namespace pps
{
const std::vector<std::string> &Driver::supported_device_names()
{
   static std::vector<std::string> supported_device_names = {
#ifdef PPS_PANFROST
      PanfrostDriver::get_name(),
#endif // PPS_PANFROST
#ifdef PPS_INTEL
      IntelDriver::get_name(),
#endif // PPS_INTEL
   };
   return supported_device_names;
}

std::unique_ptr<Driver> Driver::create(DrmDevice &&drm_device)
{
   std::unique_ptr<Driver> driver;

#ifdef PPS_PANFROST
   if (drm_device.name == PanfrostDriver::get_name()) {
      driver = std::make_unique<PanfrostDriver>();
   }
#endif // PPS_PANFROST

#ifdef PPS_INTEL
   if (drm_device.name == IntelDriver::get_name()) {
      driver = std::make_unique<IntelDriver>();
   }
#endif // PPS_INTEL

   if (!driver) {
      PERFETTO_ELOG("Failed to find a driver for DRM device %s", drm_device.name.c_str());
      return nullptr;
   }

   driver->drm_device = std::move(drm_device);
   return driver;
}

std::string Driver::default_driver_name()
{
   auto supported_devices = Driver::supported_device_names();
   auto devices = DrmDevice::create_all();
   for (auto &device : devices) {
      if (CONTAINS(supported_devices, device.name)) {
         PPS_LOG_IMPORTANT("Driver selected: %s", device.name.c_str());
         return device.name;
      }
   }
   PPS_LOG_FATAL("Failed to find any driver");
}

std::string Driver::find_driver_name(const char *requested)
{
   auto supported_devices = Driver::supported_device_names();
   auto devices = DrmDevice::create_all();
   for (auto &device : devices) {
      if (device.name == requested) {
         PPS_LOG_IMPORTANT("Driver selected: %s", device.name.c_str());
         return device.name;
      }
   }

   std::ostringstream drivers_os;
   std::copy(supported_devices.begin(),
      supported_devices.end() - 1,
      std::ostream_iterator<std::string>(drivers_os, ", "));
   drivers_os << supported_devices.back();

   PPS_LOG_ERROR(
      "Device '%s' not found (supported drivers: %s)", requested, drivers_os.str().c_str());

   return default_driver_name();
}

} // namespace pps
