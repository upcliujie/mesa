# Copyright © 2021 Intel Corporation

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

anv_tracepoints_files = custom_target(
  'anv_tracepoints.[ch]',
  input: 'intel_tracepoints.py',
  output: ['anv_tracepoints.c', 'anv_tracepoints.h', 'anv_tracepoints_perfetto.h'],
  command: [
    prog_python, '@INPUT@',
    '-p', join_paths(meson.source_root(), 'src/util/perf/'),
    '--anv',
    '--utrace-src', '@OUTPUT0@',
    '--utrace-hdr', '@OUTPUT1@',
    '--perfetto-hdr', '@OUTPUT2@',
  ],
  depend_files: u_trace_py,
)

libanv_tracepoints = static_library(
  'anv_tracepoints',
  anv_tracepoints_files,
  include_directories : [
    inc_include, inc_src, inc_mesa, inc_util,
  ],
  dependencies : [ idep_mesautil ],
  gnu_symbol_visibility : 'hidden',
)

idep_anv_tracepoints = declare_dependency(
  include_directories : include_directories('.'),
  link_with : libanv_tracepoints,
)
