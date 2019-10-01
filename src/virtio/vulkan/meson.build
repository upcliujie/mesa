# Copyright 2019 Google LLC
# SPDX-License-Identifier: MIT
#
# based in part on anv and radv which are:
# Copyright © 2017 Intel Corporation

vn_extensions_py = files('vn_extensions.py')

vn_entrypoints = custom_target(
  'vn_entrypoints.[ch]',
  input : ['vn_entrypoints_gen.py', vk_api_xml],
  output : ['vn_entrypoints.h', 'vn_entrypoints.c'],
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@', '--outdir',
    meson.current_build_dir()
  ],
  depend_files : vn_extensions_py,
)

vn_extensions_c = custom_target(
  'vn_extensions.c',
  input : ['vn_extensions_gen.py', vk_api_xml],
  output : 'vn_extensions.c',
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@', '--out-c', '@OUTPUT@',
  ],
  depend_files : vn_extensions_py,
)

vn_extensions_h = custom_target(
  'vn_extensions.h',
  input : ['vn_extensions_gen.py', vk_api_xml],
  output : 'vn_extensions.h',
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@', '--out-h', '@OUTPUT@',
  ],
  depend_files : vn_extensions_py,
)

virtio_icd = custom_target(
  'virtio_icd',
  input : 'vn_icd.py',
  output : 'virtio_icd.@0@.json'.format(host_machine.cpu()),
  command : [
    prog_python, '@INPUT@',
    '--lib-path', join_paths(get_option('prefix'), get_option('libdir')),
    '--out', '@OUTPUT@',
  ],
  depend_files : vn_extensions_py,
  build_by_default : true,
  install_dir : with_vulkan_icd_dir,
  install : true,
)

libvn_files = files(
  'vn_common.c',
  'vn_cs.c',
  'vn_device.c',
  'vn_icd.c',
)

vn_deps = []
vn_flags = []

libvulkan_virtio = shared_library(
  'vulkan_virtio',
  [libvn_files, vn_entrypoints, vn_extensions_c, vn_extensions_h],
  include_directories : [
    inc_include, inc_src, inc_vulkan_wsi, inc_virtio,
  ],
  link_with : [
    libvulkan_wsi,
  ],
  dependencies : [
    dep_thread, idep_mesautil, idep_vulkan_util, vn_deps,
  ],
  c_args : [no_override_init_args, vn_flags],
  link_args : [ld_args_bsymbolic, ld_args_gc_sections],
  gnu_symbol_visibility : 'hidden',
  install : true,
)
