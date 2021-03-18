# Copyright © 2020 Collabora, Ltd.
# Author: Antonio Caggiano <antonio.caggiano@collabora.com>
# Author: Robert Beckett <bob.beckett@collabora.com>
#
# SPDX-License-Identifier: MIT

pps_panfrost_sources = [
  'pan_pps_driver.cc'
]

dep_drm = dependency('libdrm')

pps_panfrost_lib = static_library(
  'pps-panfrost',
  sources: pps_panfrost_sources,
  include_directories: [inc_tool, inc_src],
  dependencies: [dep_drm, dep_perfetto],
  cpp_args: '-std=c++17'
)

compile_args_pps_panfrost = ['-DPPS_PANFROST']

pps_panfrost_dep = declare_dependency(
  link_with: pps_panfrost_lib,
  include_directories: [inc_tool, inc_src],
  compile_args: compile_args_pps_panfrost
)

pps_datasources += pps_panfrost_dep
with_datasources += 'panfrost'
