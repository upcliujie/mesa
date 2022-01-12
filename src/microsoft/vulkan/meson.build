# Copyright © Microsoft Corporation

# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

dzn_entrypoints = custom_target(
  'dzn_entrypoints',
  input : [vk_entrypoints_gen, vk_api_xml],
  output : ['dzn_entrypoints.h', 'dzn_entrypoints.c'],
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@', '--proto', '--weak',
    '--out-h', '@OUTPUT0@', '--out-c', '@OUTPUT1@', '--prefix', 'dzn',
  ],
  depend_files : vk_entrypoints_gen_depend_files,
)

cmd_dispatch_functions = files(
  'dzn_cmd_dispatch_functions'
)

dzn_secondary_cmd = custom_target(
  'dzn_secondary_cmd',
  input : [vk_cmd_record_gen, vk_api_xml],
  output : ['dzn_secondary_cmd.c', 'dzn_secondary_cmd.h'],
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@',
    '--out-c', '@OUTPUT0@', '--out-h', '@OUTPUT1@',
    '--prefix', 'dzn_secondary',
  ],
  depend_files : [vk_cmd_record_gen],
)

dzn_cmd_dispatch = custom_target(
  'dzn_cmd_dispatch',
  input : [vk_cmd_dispatch_gen, vk_api_xml, cmd_dispatch_functions],
  output : ['dzn_cmd_dispatch.c', 'dzn_cmd_dispatch.h'],
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@',
    '--functions-list', '@INPUT2@',
    '--out-c', '@OUTPUT0@', '--out-h', '@OUTPUT1@',
    '--primary-prefix', 'dzn_primary',
    '--secondary-prefix', 'dzn_secondary',
    '--prefix', 'dzn',
  ],
  depend_files : [vk_cmd_dispatch_gen, cmd_dispatch_functions],
)

cmd_exec_functions = files(
  'dzn_cmd_exec_functions'
)

dzn_cmd_exec = custom_target(
  'dzn_cmd_exec',
  input : [vk_cmd_exec_gen, vk_api_xml, cmd_exec_functions],
  output : ['dzn_cmd_exec.c', 'dzn_cmd_exec.h'],
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@',
    '--functions-list', '@INPUT2@',
    '--out-c', '@OUTPUT0@', '--out-h', '@OUTPUT1@',
    '--primary-prefix', 'dzn_primary',
    '--prefix', 'dzn',
  ],
  depend_files : [vk_cmd_dispatch_gen, cmd_exec_functions],
)


libdzn_files = files(
  'dzn_cmd_buffer.cpp',
  'dzn_descriptor_set.cpp',
  'dzn_device.cpp',
  'dzn_fence.cpp',
  'dzn_image.cpp',
  'dzn_meta.cpp',
  'dzn_nir.c',
  'dzn_pass.cpp',
  'dzn_pipeline_cache.cpp',
  'dzn_pipeline.cpp',
  'dzn_query.cpp',
  'dzn_semaphore.cpp',
  'dzn_state.cpp',
  'dzn_util.cpp',
  'dzn_util.c',
  'dzn_wsi.cpp',
)

dzn_deps = [
  idep_libdxil_compiler,
  idep_libspirv_to_dxil,
  idep_libd3d12_common,
  idep_nir,
  idep_nir_headers,
  idep_vulkan_util,
  idep_vulkan_runtime,
  idep_vulkan_wsi,
  dep_dxheaders,
]

dzn_flags = [ ]

if with_platform_windows
  dzn_flags += '-DVK_USE_PLATFORM_WIN32_KHR'
endif

libvulkan_dzn = shared_library(
  'vulkan_dzn',
  [libdzn_files, dzn_entrypoints, sha1_h, dzn_secondary_cmd, dzn_cmd_dispatch, dzn_cmd_exec],
  vs_module_defs : 'vulkan_dzn.def',
  include_directories : [
    inc_include, inc_src, inc_mapi, inc_mesa, inc_gallium, inc_gallium_aux,
    inc_compiler, inc_util
  ],
  dependencies : [dzn_deps, idep_vulkan_wsi],
  c_args : dzn_flags,
  cpp_args : dzn_flags,
  gnu_symbol_visibility : 'hidden',
  link_args : [ld_args_bsymbolic, ld_args_gc_sections],
  name_prefix : host_machine.system() == 'windows' ? '' : 'lib',
  install : true,
  override_options: ['cpp_std=c++latest']
)

icd_file_name = 'libvulkan_dzn.so'
module_dir = join_paths(get_option('prefix'), get_option('libdir'))
if with_platform_windows
  module_dir = join_paths(get_option('prefix'), get_option('bindir'))
  icd_file_name = 'vulkan_dzn.dll'
endif

dzn_icd = custom_target(
  'dzn_icd',
  input : [vk_icd_gen, vk_api_xml],
  output : 'dzn_icd.@0@.json'.format(host_machine.cpu()),
  command : [
    prog_python, '@INPUT0@',
    '--api-version', '1.2', '--xml', '@INPUT1@',
    '--lib-path', join_paths(module_dir, icd_file_name),
    '--out', '@OUTPUT@',
  ],
  build_by_default : true,
  install_dir : with_vulkan_icd_dir,
  install : true,
)
