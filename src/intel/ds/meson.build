# Copyright © 2020-2021 Collabora, Ltd.
# Author: Antonio Caggiano <antonio.caggiano@collabora.com>
#
# SPDX-License-Identifier: MIT

pps_intel_sources = [
  'intel_pps_driver.cc',
]

dep_drm = dependency('libdrm')
dep_i915perf = dependency('i915-perf', required: false)

if dep_i915perf.found()
  pps_intel_lib = static_library(
    'pps-intel',
    sources: pps_intel_sources,
    include_directories: [inc_tool],
    dependencies: [dep_perfetto, dep_drm, dep_i915perf],
    cpp_args: '-std=c++17'
  )

  compile_args_pps_intel = ['-DPPS_INTEL']

  pps_intel_dep = declare_dependency(
    link_with: pps_intel_lib,
    include_directories: inc_tool,
    compile_args: compile_args_pps_intel,
  )

  pps_datasources += pps_intel_dep
  with_datasources += 'intel'
elif with_datasources.contains('intel')
  # User wants this datasource, hence fail if can not be built
  error('i915-perf dependency not found, can not build intel datasource')
endif
