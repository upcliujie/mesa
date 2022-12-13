/*
 * Copyright © 2022 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

extern crate vulkan_h;
extern crate vulkan_runtime;

mod boxed;

use vulkan_h::VkResult;

pub type Result<T> = std::result::Result<T, VkResult>;
