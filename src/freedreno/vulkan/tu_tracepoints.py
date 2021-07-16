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
parser.add_argument('--utrace-src', required=True)
parser.add_argument('--utrace-hdr', required=True)
parser.add_argument('--perfetto-hdr', required=True)
args = parser.parse_args()
sys.path.insert(0, args.import_path)


from u_trace import Header
from u_trace import ForwardDecl
from u_trace import Tracepoint
from u_trace import TracepointArg
from u_trace import utrace_generate
from u_trace import utrace_generate_perfetto_utils

#
# Tracepoint definitions:
#

Header('util/u_dump.h')
Header('vk_format.h')

ForwardDecl('struct tu_device')

Tracepoint('start_render_pass',
    tp_perfetto='tu_start_render_pass'
)
Tracepoint('end_render_pass',
    args=[TracepointArg(type='uint16_t', var_name='width',        c_format='%u'),
          TracepointArg(type='uint16_t', var_name='height',       c_format='%u'),
          TracepointArg(type='uint8_t',  var_name='mrts',         pretty_name='MRTs', c_format='%u'),
          TracepointArg(type='uint8_t',  var_name='samples',      c_format='%u'),
          TracepointArg(type='uint16_t', var_name='numberOfBins', c_format='%u'),
          TracepointArg(type='uint16_t', var_name='binWidth',     c_format='%u'),
          TracepointArg(type='uint16_t', var_name='binHeight',    c_format='%u')],
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

Tracepoint('start_gmem_clear',
    tp_perfetto='tu_start_gmem_clear')
Tracepoint('end_gmem_clear',
    args=[TracepointArg(type='enum VkFormat',  var_name='format',  c_format='%s', to_prim_type='vk_format_description({})->short_name'),
          TracepointArg(type='uint8_t',        var_name='samples', c_format='%u')],
    tp_perfetto='tu_end_gmem_clear')

Tracepoint('start_sysmem_clear',
    tp_perfetto='tu_start_sysmem_clear')
Tracepoint('end_sysmem_clear',
    args=[TracepointArg(type='enum VkFormat',  var_name='format',      c_format='%s', to_prim_type='vk_format_description({})->short_name'),
          TracepointArg(type='uint8_t',        var_name='uses_3d_ops', c_format='%u'),
          TracepointArg(type='uint8_t',        var_name='samples',     c_format='%u')],
    tp_perfetto='tu_end_sysmem_clear')

Tracepoint('start_sysmem_clear_all',
    tp_perfetto='tu_start_sysmem_clear_all')
Tracepoint('end_sysmem_clear_all',
    args=[TracepointArg(type='uint8_t',        var_name='mrt_count',   c_format='%u'),
          TracepointArg(type='uint8_t',        var_name='rect_count',  c_format='%u')],
    tp_perfetto='tu_end_sysmem_clear_all')

Tracepoint('start_gmem_load',
    tp_perfetto='tu_start_gmem_load')
Tracepoint('end_gmem_load',
    args=[TracepointArg(type='enum VkFormat',  var_name='format',   c_format='%s', to_prim_type='vk_format_description({})->short_name'),
          TracepointArg(type='uint8_t',        var_name='force_load', c_format='%u')],
    tp_perfetto='tu_end_gmem_load')

Tracepoint('start_gmem_store',
    tp_perfetto='tu_start_gmem_store')
Tracepoint('end_gmem_store',
    args=[TracepointArg(type='enum VkFormat',  var_name='format',   c_format='%s', to_prim_type='vk_format_description({})->short_name'),
          TracepointArg(type='uint8_t',        var_name='fast_path', c_format='%u'),
          TracepointArg(type='uint8_t',        var_name='unaligned', c_format='%u')],
    tp_perfetto='tu_end_gmem_store')

Tracepoint('start_sysmem_resolve',
    tp_perfetto='tu_start_sysmem_resolve')
Tracepoint('end_sysmem_resolve',
    args=[TracepointArg(type='enum VkFormat',  var_name='format',   c_format='%s', to_prim_type='vk_format_description({})->short_name')],
    tp_perfetto='tu_end_sysmem_resolve')

Tracepoint('start_blit',
    tp_perfetto='tu_start_blit',
)
Tracepoint('end_blit',
    # TODO: add source megapixels count and target megapixels count arguments
    args=[TracepointArg(type='uint8_t',        var_name='uses_3d_blit', c_format='%u'),
          TracepointArg(type='enum VkFormat',  var_name='src_format',   c_format='%s', to_prim_type='vk_format_description({})->short_name'),
          TracepointArg(type='enum VkFormat',  var_name='dst_format',   c_format='%s', to_prim_type='vk_format_description({})->short_name'),
          TracepointArg(type='uint8_t',        var_name='layers',       c_format='%u')],
    tp_perfetto='tu_end_blit')

Tracepoint('start_compute',
    tp_perfetto='tu_start_compute')
Tracepoint('end_compute',
    args=[TracepointArg(type='uint8_t',  var_name='indirect',       c_format='%u'),
          TracepointArg(type='uint16_t', var_name='local_size_x',   c_format='%u'),
          TracepointArg(type='uint16_t', var_name='local_size_y',   c_format='%u'),
          TracepointArg(type='uint16_t', var_name='local_size_z',   c_format='%u'),
          TracepointArg(type='uint16_t', var_name='num_groups_x',   c_format='%u'),
          TracepointArg(type='uint16_t', var_name='num_groups_y',   c_format='%u'),
          TracepointArg(type='uint16_t', var_name='num_groups_z',   c_format='%u')],
    tp_perfetto='tu_end_compute')

utrace_generate(cpath=args.utrace_src, hpath=args.utrace_hdr, ctx_param='struct tu_device *dev')
utrace_generate_perfetto_utils(hpath=args.perfetto_hdr)
