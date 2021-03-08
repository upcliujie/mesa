#
# Copyright (C) 2021 Collabora Ltd.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

import argparse
import sys

a = 'a'
b = 'b'
c = 'c'

lower_alu = [
   # For chipfamily r600 one must do fma (2*pi ffract() - 0.5)
   (('fsin', "a@32"), ('fsin_r600', ('fadd', ('ffract', ('ffma', a, 0.15915494, 0.5)), -0.5))),
   (('fcos', "a@32"), ('fcos_r600', ('fadd', ('ffract', ('ffma', a, 0.15915494, 0.5)), -0.5))),

   (('bcsel', ('ilt', 0, 'a@32'), 'b@32', 'c@32'), ('i32csel_gt', a, b, c)),
   (('bcsel', ('ilt', 'a@32', 0), 'b@32', 'c@32'), ('i32csel_ge', a, c, b)),

   (('bcsel', ('ige', 'a@32', 0), 'b@32', 'c@32'), ('i32csel_ge', a, b, c)),
   (('bcsel', ('ige', 0, 'a@32'), 'b@32', 'c@32'), ('i32csel_gt', a, c, b)),

   (('fcsel', ('slt', 0, a), b, c), ('fcsel_gt', a, b, c)),
   (('fcsel', ('slt', a, 0), b, c), ('fcsel_ge', a, c, b)),

   (('fcsel', ('sge', a, 0), b, c), ('fcsel_ge', a, b, c)),
   (('fcsel', ('sge', 0, a), b, c), ('fcsel_gt', a, c, b)),

   (('ifind_msb', 'value'),
    ('i32csel_ge', ('ifind_msb_rev', 'value'),
                   ('isub', 31, ('ifind_msb_rev', 'value')),
                   ('ifind_msb_rev', 'value'))),

   (('ufind_msb', 'value'),
    ('i32csel_ge', ('ufind_msb_rev', 'value'),
	  	   ('isub', 31, ('ufind_msb_rev', 'value')),
		   ('ufind_msb_rev', 'value'))),
]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--import-path', required=True)
    args = parser.parse_args()
    sys.path.insert(0, args.import_path)
    run()


def run():
    import nir_algebraic  # pylint: disable=import-error

    print('#include "sfn/sfn_nir.h"')

    print(nir_algebraic.AlgebraicPass("r600_lower_alu",
                                      lower_alu).render())

if __name__ == '__main__':
    main()
