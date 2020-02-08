# Copyright © 2020 Intel Corporation

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

libi915_pipe_drm_shim = shared_library(
  ['i915_pipe_drm_shim'],
  'i915_pipe.c',
  include_directories: [inc_include, inc_src, inc_intel],
  link_with: libintel_dev,
  dependencies: [dep_libdrm, dep_drm_shim, idep_mesautil, idep_libintel_common],
  c_args : [no_override_init_args],
  install : true,
)

libs_intel_mi_runner = []

foreach g : ['70', '75', '80', '90', '100', '110', '120']
  libs_intel_mi_runner += static_library(
    'intel_mi_runner_@0@'.format(g),
    files('genX_mi_runner.c'),
    include_directories : [
      inc_include, inc_src, inc_compiler, inc_include, inc_intel,
    ],
    c_args : [
      no_override_init_args, c_sse2_args,
      '-DGEN_VERSIONx10=@0@'.format(g),
    ],
    dependencies : [
      idep_genxml_unpack, idep_mesautil,
    ],
  )
endforeach

libi915_pipe_name = 'libi915_pipe_drm_shim.so'
i915_pipe_path = join_paths(get_option('prefix'), get_option('libdir'), libi915_pipe_name)

intel_mi_runner = executable(
  'intel_mi_runner',
  files('intel_mi_runner.cpp',
        '../aubinator_viewer_decoder.cpp'),
  dependencies : [idep_mesautil, dep_zlib, dep_dl, dep_thread, dep_m, libintel_imgui_gtk_dep, idep_genxml_unpack],
  include_directories : [inc_include, inc_src, inc_intel, inc_intel_tools],
  link_with : [libintel_common, libintel_compiler, libintel_dev, libs_intel_mi_runner],
  c_args : [no_override_init_args],
  cpp_args : ['-fpermissive', '-Wno-parentheses', '-Wno-class-memaccess',
              '-DI915_PIPE_PATH="@0@"'.format(i915_pipe_path),
              '-DLIBI915_PIPE_NAME="@0@"'.format(libi915_pipe_name)],
  install : true
)
