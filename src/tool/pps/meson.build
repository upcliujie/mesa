# Copyright © 2020-2021 Collabora, Ltd.
# Author: Antonio Caggiano <antonio.caggiano@collabora.com>
#
# SPDX-License-Identifier: MIT

pps_sources = [
  'pps.cc',
  'pps_device.cc',
  'pps_driver.cc',
  'pps_counter.cc',
]

include_pps = include_directories('../')

dep_drm = dependency('libdrm')
gpu_deps = [dep_drm, dep_perfetto]

lib_pps = static_library(
  'pps',
  sources: pps_sources,
  dependencies: gpu_deps,
  cpp_args: '-std=c++17'
)

dep_pps = declare_dependency(
  link_with: lib_pps,
  include_directories: include_pps
)

producer_sources = [
  'pps_datasource.cc',
  'pps_producer.cc'
]

executable(
  'pps-producer',
  sources: producer_sources,
  include_directories: include_pps,
  dependencies: [dep_pps, dep_perfetto],
  cpp_args: '-std=c++17',
  install: true
)
