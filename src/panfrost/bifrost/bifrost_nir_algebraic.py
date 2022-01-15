# Copyright (C) 2021 Collabora, Ltd.
# Copyright (C) 2016 Intel Corporation
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
import math

a = 'a'
b = 'b'
c = 'c'

algebraic_late = [
    # Canonical form. The scheduler will convert back if it makes sense.
    (('fmul', a, 2.0), ('fadd', a, a)),

    # Fuse Mali-specific clamps
    (('fmin', ('fmax', a, -1.0), 1.0), ('fsat_signed_mali', a)),
    (('fmax', ('fmin', a, 1.0), -1.0), ('fsat_signed_mali', a)),
    (('fmax', a, 0.0), ('fclamp_pos_mali', a)),

    (('fabs', ('fddx', a)), ('fabs', ('fddx_must_abs_mali', a))),
    (('fabs', ('fddy', b)), ('fabs', ('fddy_must_abs_mali', b))),

    # In NIR, b2f16 can take a vectorized 32-bit source. We do not have a direct
    # opcode for this. However, 32-bit NIR booleans are 0/-1 as integers, so
    # fneg(i2f16) implements b2f16, with both operations vectorized.
    #
    # Lowering this in NIR allows the fneg to be optimized, for example if the
    # result of the b2f16 is negated.
    #
    # Test case: KHR-GLES31.core.compute_shader.pipeline-post-fs
    (('b2f16', 'a@32'), ('fneg', ('i2f16', a))),
]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--import-path', required=True)
    args = parser.parse_args()
    sys.path.insert(0, args.import_path)
    run()


def run():
    import nir_algebraic  # pylint: disable=import-error

    print('#include "bifrost_nir.h"')

    print(nir_algebraic.AlgebraicPass("bifrost_nir_lower_algebraic_late",
                                      algebraic_late).render())


if __name__ == '__main__':
    main()
