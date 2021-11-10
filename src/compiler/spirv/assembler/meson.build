# Copyright © 2017 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

if with_tests

spirv_assembler_info_c = custom_target(
  'spirv_assembler_info.c',
  input : files('spirv_assembler_info_c.py', '../spirv.core.grammar.json', '../spirv_info_c.py'),
  output : 'spirv_assembler_info.c',
  command : [prog_python, '@INPUT0@', '@INPUT1@', '@OUTPUT@'],
)

_libspirv_assembler = static_library(
  'spirv_assembler',
  [spirv_assembler_info_c,
   files(
     'spirv_assembler.c',
     'spirv_assembler.h',
     'spirv_assembler_info.h',
   )],
  include_directories : [inc_include, inc_src, include_directories('..')],
  c_args: [c_msvc_compat_args, no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  dependencies : [],
  link_with : libcompiler,
  build_by_default : false,
)

idep_spirv_assembler = declare_dependency(
  dependencies : [idep_mesautil],
  link_with : _libspirv_assembler
)

endif