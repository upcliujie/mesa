#
# Copyright © 2021 Igalia S.L.
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
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
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

Header('util/u_dump.h')

Tracepoint('start_render_pass',
    tp_perfetto='tu_start_render_pass'
)
Tracepoint('end_render_pass',
    args=[['uint32_t', 'submit_id'],
          ['uint16_t', 'width'],
          ['uint16_t', 'height'],
          ['uint8_t', 'mrts'],
          ['uint8_t', 'samples'],
          ['uint16_t', 'nbins'],
          ['uint16_t', 'binw'],
          ['uint16_t', 'binh']],
    tp_perfetto='tu_end_render_pass')

Tracepoint('start_binning_ib',
    tp_perfetto='tu_start_binning_ib')
Tracepoint('end_binning_ib',
    tp_perfetto='tu_end_binning_ib')

Tracepoint('start_resolve',
    tp_perfetto='tu_start_resolve')
Tracepoint('end_resolve',
    tp_perfetto='tu_end_resolve')

Tracepoint('start_draw_ib_sysmem',
    tp_perfetto='tu_start_draw_ib_sysmem')
Tracepoint('end_draw_ib_sysmem',
    tp_perfetto='tu_end_draw_ib_sysmem')

Tracepoint('start_draw_ib_gmem',
    tp_perfetto='tu_start_draw_ib_gmem')
Tracepoint('end_draw_ib_gmem',
    tp_perfetto='tu_end_draw_ib_gmem')

Tracepoint('start_blit',
    tp_perfetto='tu_start_blit',
)
Tracepoint('end_blit',
    tp_perfetto='tu_end_blit')

Tracepoint('start_compute',
    tp_perfetto='tu_start_compute')
Tracepoint('end_compute',
    tp_perfetto='tu_end_compute')

utrace_generate(cpath=args.src, hpath=args.hdr, ctx_param='struct tu_device *dev')
