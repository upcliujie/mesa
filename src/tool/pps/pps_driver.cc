/*
 * Copyright © 2019-2020 Collabora, Ltd.
 * Author: Antonio Caggiano <antonio.caggiano@collabora.com>
 * Author: Rohan Garg <rohan.garg@collabora.com>
 * Author: Robert Beckett <bob.beckett@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "pps_driver.h"

#include "pps.h"

namespace pps
{
const std::vector<std::string> &Driver::supported_device_names()
{
   static std::vector<std::string> supported_device_names = {};
   return supported_device_names;
}

std::unique_ptr<Driver> Driver::create(DrmDevice &&drm_device)
{
   std::unique_ptr<Driver> driver;

   if (!driver) {
      PERFETTO_ELOG("Failed to find a driver for DRM device %s", drm_device.name.c_str());
      return nullptr;
   }

   driver->drm_device = std::move(drm_device);
   return driver;
}

} // namespace pps
