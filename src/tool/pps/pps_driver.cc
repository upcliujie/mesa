/*
 * Copyright © 2019-2020 Collabora, Ltd.
 * Author: Antonio Caggiano <antonio.caggiano@collabora.com>
 * Author: Rohan Garg <rohan.garg@collabora.com>
 * Author: Robert Beckett <bob.beckett@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "pps_driver.h"

#ifdef PPS_PANFROST
#include "panfrost/ds/pan_pps_driver.h"
#endif // PPS_PANFROST
#include "pps.h"

namespace pps
{
const std::vector<std::string> &Driver::supported_device_names()
{
   static std::vector<std::string> supported_device_names = {
#ifdef PPS_PANFROST
      PanfrostDriver::get_name()
#endif // PPS_PANFROST
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

   if (!driver) {
      PERFETTO_ELOG("Failed to find a driver for DRM device %s", drm_device.name.c_str());
      return nullptr;
   }

   driver->drm_device = std::move(drm_device);
   return driver;
}

} // namespace pps
