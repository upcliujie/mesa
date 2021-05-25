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
parser.add_argument('-C', '--src')
parser.add_argument('-H', '--hdr')
args = parser.parse_args()
sys.path.insert(0, args.import_path)


from u_trace import Header
from u_trace import Tracepoint
from u_trace import TracepointArg
from u_trace import utrace_generate

#
# Tracepoint definitions:
#

Header('pipe/p_state.h')
Header('util/format/u_format.h')

Tracepoint('surface',
    args=[TracepointArg(type='uint16_t',     var_name='width',      c_format='%u'),
          TracepointArg(type='uint16_t',     var_name='height',     c_format='%u'),
          TracepointArg(type='uint8_t',      var_name='nr_samples', c_format='%u'),
          TracepointArg(type='const char *', var_name='format',     c_format='%s')],
    tp_print=['%ux%u@%u, fmt=%s',
        '__entry->width',
        '__entry->height',
        '__entry->nr_samples',
        '__entry->format'],
)

# Note: called internally from trace_framebuffer_state()
Tracepoint('framebuffer',
    args=[TracepointArg(type='uint16_t',     var_name='width',      c_format='%u'),
          TracepointArg(type='uint16_t',     var_name='height',     c_format='%u'),
          TracepointArg(type='uint8_t',      var_name='layers',     c_format='%u'),
          TracepointArg(type='uint8_t',      var_name='samples',    c_format='%u'),
          TracepointArg(type='uint8_t',      var_name='nr_cbufs',   c_format='%u')],
    tp_print=['%ux%ux%u@%u, nr_cbufs: %u',
        '__entry->width',
        '__entry->height',
        '__entry->layers',
        '__entry->samples',
        '__entry->nr_cbufs'],
)

Tracepoint('grid_info',
    args=[TracepointArg(type='uint8_t',  var_name='work_dim',  c_format='%u'),
          TracepointArg(type='uint16_t', var_name='block_x',   c_format='%u'),
          TracepointArg(type='uint16_t', var_name='block_y',   c_format='%u'),
          TracepointArg(type='uint16_t', var_name='block_z',   c_format='%u'),
          TracepointArg(type='uint16_t', var_name='grid_x',    c_format='%u'),
          TracepointArg(type='uint16_t', var_name='grid_y',    c_format='%u'),
          TracepointArg(type='uint16_t', var_name='grid_z',    c_format='%u')],
    tp_print=['work_dim=%u, block=%ux%ux%u, grid=%ux%ux%u', '__entry->work_dim',
        '__entry->block_x', '__entry->block_y', '__entry->block_z',
        '__entry->grid_x', '__entry->grid_y', '__entry->grid_z'],
)

utrace_generate(cpath=args.src, hpath=args.hdr, ctx_param='struct pipe_context *pctx')
