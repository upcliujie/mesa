# Copyright © 2020 Google, Inc

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

isa_c = custom_target(
  'isa.c',
  input: ['decode.py', 'ir3.xml'],
  output: 'isa.c',
  command: [
    prog_python, '@INPUT0@', '@INPUT1@', '@OUTPUT@'
  ],
  depend_files: [
    'ir3-common.xml',
    'ir3-cat0.xml',
    'ir3-cat1.xml',
    'ir3-cat2.xml',
    'ir3-cat3.xml',
    'ir3-cat4.xml',
    'ir3-cat5.xml',
    'isa.py',
  ]
)

decode_files = [
  isa_c,
  'isa.h',
  'decode.c'
]

libdecode = static_library(
  'decode',
  decode_files,
)

disasm = executable(
  'disasm',
  ['disasm.c'],
  link_with: libdecode,
  build_by_default: with_tools.contains('freedreno'),
  install: false,
)
