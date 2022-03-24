# Copyright © 2022 Emmanuel Gil Peyrot

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

glide_version = ['2', '2', '0']

glide_link_with = [
    libgallium, libmesa_format,
    libpipe_loader_dynamic, libloader, libws_null, libwsw,
]

rust = import('unstable-rust')
gallium_bindgen = rust.bindgen(
  input: 'bindgen.h',
  output: 'gallium.rs',
  include_directories: include_directories(['../../../../include', '../../../../src', '../../include', '../../auxiliary']),
  args: ['--default-enum-style=rust'],
  c_args: ['-DUTIL_ARCH_LITTLE_ENDIAN=1', '-DUTIL_ARCH_BIG_ENDIAN=0'],
)

gallium_sys = static_library(
  'gallium_sys',
  gallium_bindgen,
)

glide_frontend = static_library(
  'glide_frontend',
  files('glide.rs', 'gr.rs', 'pipe.rs'),
  link_with : [glide_link_with, gallium_sys],
  dependencies : [
    idep_xmlconfig, idep_mesautil,
  ],
)
