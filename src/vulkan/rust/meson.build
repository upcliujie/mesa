# Copyright © 2022 Collabora, Ltd.
# SPDX-License-Identifier: MIT

add_languages('rust', required: true)
rust = import('unstable-rust')

assert(
  meson.version().version_compare('>= 1.0.0'),
  'Meson 1.0 is required for Rust Vulkan bindings'
)

_vulkan_h_rs = rust.bindgen(
  input : ['vulkan_h_bindgen.h'],
  output : 'vulkan_h.rs',
  include_directories : [inc_include],
  c_args : [
    pre_args,
  ],
  args : [
    '--allowlist-type', 'PFN_vk.*',
    '--allowlist-type', 'Vk.*',
    '--allowlist-var', 'VK_.*',
    '--no-prepend-enum-name',
    '--raw-line', '#![allow(non_camel_case_types)]',
    '--raw-line', '#![allow(non_snake_case)]',
    '--raw-line', '#![allow(non_upper_case_globals)]',
    '--size_t-is-usize',
  ],
)

libvulkan_h_rs = static_library(
  'vulkan_h',
  _vulkan_h_rs,
  gnu_symbol_visibility : 'hidden',
  rust_crate_type : 'rlib',
)
