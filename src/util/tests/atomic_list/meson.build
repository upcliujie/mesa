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

test_c_args = [c_msvc_compat_args]
if host_machine.cpu_family() == 'x86_64' and cc.get_id() != 'msvc'
  test_c_args += '-mcx16'
endif

if cc.get_id() != 'msvc'
  executable(
    'u_atomic_list_benchmark',
    'benchmark.c',
    dependencies : [idep_mesautil],
    include_directories : [inc_include, inc_src, inc_gallium],
    c_args : test_c_args,
    install : false,
  )
endif

test(
  'u_atomic_list_multi_threaded',
  executable(
    'multi_threaded',
    'multi_threaded.c',
    dependencies : [idep_mesautil],
    include_directories : [inc_include, inc_src, inc_gallium],
    c_args : test_c_args,
  ),
  suite : ['util'],
  timeout: 60,
)

