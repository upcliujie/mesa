#
# Copyright © 2020 Google, Inc.
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

from mako.template import Template
from isa import ISA
import sys

template = """\
/* Copyright (C) 2020 Google, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "isa.h"

/*
 * enum tables, these don't have any link back to other tables so just
 * dump them up front before the bitset tables
 */

%for name, enum in isa.enums.items():
static const struct isa_enum ${enum.get_c_name()} = {
    .num_values = ${len(enum.values)},
    .values = {
%   for val, display in enum.values.items():
        { .val = ${val}, .display = "${display}" },
%   endfor
    },
};
%endfor

/*
 * expression tables, can be linked from bitset tables, so also dump
 * them up front
 */

%for name, expr in isa.expressions.items():
static const struct isa_expr ${expr.get_c_name()} = {
    .num_instructions = ${len(expr.instructions)},
    .instructions = {
%   for instr in expr.instructions:
        { .opc = ISA_INSTR_${instr[0]},
%      if instr[0] == 'LITERAL' or instr[0] == 'RETLIT':
          .literal = ${instr[1]},
%      elif instr[0] == 'VAR':
          .variable = "${instr[1]}",
%      endif
        },
%endfor
    },
};
%endfor

/*
 * Forward-declarations (so we don't have to figure out which order to
 * emit various tables when they have pointers to each other)
 */

%for name, bitset in isa.bitsets.items():
static const struct isa_bitset bitset_${bitset.get_c_name()};
%endfor

%for root_name, root in isa.roots.items():
const struct isa_bitset *${root.get_c_name()}[];
%endfor

/*
 * bitset tables:
 */

%for name, bitset in isa.bitsets.items():
%   for case in bitset.cases:
%      for field_name, field in case.fields.items():
%         if field.get_c_typename() == 'TYPE_BITSET':
%            if len(field.remaps) > 0:
static const struct isa_field_remap ${case.get_c_name()}_${field.get_c_name()} = {
       .num_remaps = ${len(field.remaps)},
       .remaps = {
%               for remap in field.remaps:
           { .from = "${remap[0]}",  .to = "${remap[1]}" },
%               endfor

       },
};
%            endif
%         endif
%      endfor
static const struct isa_case ${case.get_c_name()} = {
%   if case.expr is not None:
       .expr     = &${isa.expressions[case.expr].get_c_name()},
%   endif
%   if case.display is not None:
       .display  = "${case.display}",
%   endif
       .num_fields = ${len(case.fields)},
       .fields   = {
%   for field_name, field in case.fields.items():
          { .name = "${field_name}", .low = ${field.low}, .high = ${field.high},
%      if field.expr is not None:
            .expr = &${isa.expressions[field.expr].get_c_name()},
%      endif
%      if field.display is not None:
            .display = "${field.display}",
%      endif
            .type = ${field.get_c_typename()},
%      if field.get_c_typename() == 'TYPE_BITSET':
            .bitsets = ${isa.roots[field.type].get_c_name()},
%         if len(field.remaps) > 0:
            .remap = &${case.get_c_name()}_${field.get_c_name()},
%         endif
%      endif
%      if field.get_c_typename() == 'TYPE_ENUM':
            .enums = &${isa.enums[field.type].get_c_name()},
%      endif
%      if field.get_c_typename() == 'TYPE_ASSERT':
            .val = ${field.val},
%      endif
          },
%   endfor
       },
};
%   endfor
static const struct isa_bitset bitset_${bitset.get_c_name()} = {
<% pattern = bitset.get_pattern() %>
%   if bitset.extends is not None:
       .parent   = &bitset_${isa.bitsets[bitset.extends].get_c_name()},
%   endif
       .name     = "${name}",
       .match    = ${hex(pattern.match)},
       .dontcare = ${hex(pattern.dontcare)},
       .mask     = ${hex(pattern.mask)},
       .num_cases = ${len(bitset.cases)},
       .cases    = {
%   for case in bitset.cases:
            &${case.get_c_name()},
%   endfor
       },
};
%endfor

/*
 * bitset hierarchy root tables (where decoding starts from):
 */

%for root_name, root in isa.roots.items():
const struct isa_bitset *${root.get_c_name()}[] = {
%   for leaf_name, leaf in isa.leafs.items():
%      if leaf.get_root() == root:
          &bitset_${leaf.get_c_name()},
%      endif
%   endfor
    (void *)0
};
%endfor

"""

xml = sys.argv[1]
dst = sys.argv[2]

isa = ISA(xml)

with open(dst, 'wb') as f:
    f.write(Template(template, output_encoding='utf-8').render(isa=isa))
