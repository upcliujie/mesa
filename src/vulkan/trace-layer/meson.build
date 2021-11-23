# Copyright © 2019 Intel Corporation

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

trace_layer_gen = custom_target(
  'trace_layer_gen',
  input : ['trace_layer_gen.py', vk_api_xml],
  output : ['trace_layer_gen.cc', 'trace_layer_gen.h'],
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@',
    '--out-cc', '@OUTPUT0@', '--out-h', '@OUTPUT1@',
  ],
  depend_files : vk_dispatch_table_gen_depend_files,
)

vklayer_trace_files = files(
  'trace_layer.cc',
)

vklayer_trace_flags = []
vklayer_trace_deps = [dep_perfetto, dep_thread, idep_mesautil, idep_vulkan_util]

if with_platform_wayland
  vklayer_trace_flags += '-DVK_USE_PLATFORM_WAYLAND_KHR'
endif

if with_platform_x11
  vklayer_trace_flags += [
    '-DVK_USE_PLATFORM_XCB_KHR',
    '-DVK_USE_PLATFORM_XLIB_KHR',
  ]
endif

if with_platform_android
  vklayer_trace_flags += '-DVK_USE_PLATFORM_ANDROID_KHR'
endif

vklayer_mesa_trace = shared_library(
  'VkLayer_MESA_trace',
  [vklayer_trace_files, trace_layer_gen],
  cpp_args : [vklayer_trace_flags],
  gnu_symbol_visibility : 'hidden',
  dependencies : [vklayer_trace_deps],
  include_directories : [inc_include, inc_src],
  link_args : cc.get_supported_link_arguments(['-Wl,-Bsymbolic-functions', '-Wl,-z,relro']),
  install : true
)

install_data(
  files('VkLayer_MESA_trace.json'),
  install_dir : join_paths(get_option('datadir'), 'vulkan', 'implicit_layer.d'),
)
