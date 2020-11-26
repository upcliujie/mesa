#
# Copyright (C) 2020 Google, Inc.
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
#

import argparse
import sys

#
# TODO can we do this with less boilerplate?
#
parser = argparse.ArgumentParser()
parser.add_argument('-p', '--import-path', required=True)
parser.add_argument('-C', '--src', required=True)
parser.add_argument('-H', '--hdr', required=True)
args = parser.parse_args()
sys.path.insert(0, args.import_path)


from u_trace import Header
from u_trace import Tracepoint
from u_trace import utrace_generate

#
# Tracepoint definitions:
#

Header('freedreno_batch.h')

Tracepoint('flush_batch',
    args=[['struct fd_batch *', 'batch'],
          ['uint16_t', 'cleared'],
          ['uint16_t', 'gmem_reason'],
          ['uint16_t', 'num_draws']],
    tp_print=['%p: cleared=%x, gmem_reason=%x, num_draws=%u', '__entry->batch',
        '__entry->cleared', '__entry->gmem_reason', '__entry->num_draws']
)

Tracepoint('render_gmem',
    args=[['uint16_t', 'nbins_x'],
          ['uint16_t', 'nbins_y'],
          ['uint16_t', 'bin_w'],
          ['uint16_t', 'bin_h']],
    tp_print=['%ux%u bins of %ux%u',
        '__entry->nbins_x', '__entry->nbins_y', '__entry->bin_w', '__entry->bin_h']
)

Tracepoint('render_nondraw')
Tracepoint('render_sysmem')

Tracepoint('start_binning_ib')
Tracepoint('end_binning_ib')

Tracepoint('start_vsc_overflow_test')
Tracepoint('end_vsc_overflow_test')

Tracepoint('start_prologue')
Tracepoint('end_prologue')

Tracepoint('start_clear_restore',
    args=[['uint16_t', 'fast_cleared']],
    tp_print=['fast_cleared: 0x%x', '__entry->fast_cleared'])
Tracepoint('end_clear_restore')

Tracepoint('start_resolve')
Tracepoint('end_resolve')

Tracepoint('start_tile',
    args=[['uint16_t', 'bin_h'],
          ['uint16_t', 'yoff'],
          ['uint16_t', 'bin_w'],
          ['uint16_t', 'xoff']],
    tp_print=['bin_h=%d, yoff=%d, bin_w=%d, xoff=%d',
        '__entry->bin_h', '__entry->yoff', '__entry->bin_w', '__entry->xoff'])

Tracepoint('start_draw_ib')
Tracepoint('end_draw_ib')

utrace_generate(cpath=args.src, hpath=args.hdr)
