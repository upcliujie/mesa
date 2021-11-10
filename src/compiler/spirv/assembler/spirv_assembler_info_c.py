COPYRIGHT = """\
/*
 * Copyright (C) 2021 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
"""

import sys, os
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), "..")))

from spirv_info_c import parse_spirv_info

import argparse
from sys import stdout
from mako.template import Template

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("json")
    p.add_argument("out")
    return p.parse_args()

TEMPLATE  = Template("""\
/* DO NOT EDIT - This file is generated automatically by spirv_assembler_info_c.py script */

""" + COPYRIGHT + """\

#include "spirv_assembler_info.h"
#include <string.h>

#define matches(s, literal) ((strlen(s) == (sizeof(literal)-1)) && !memcmp(s, literal, sizeof(literal)))
#define prefix_matches(s, literal) ((strlen(s) >= (sizeof(literal)-1)) && !memcmp(s, literal, sizeof(literal)-1))

% for kind,values,category in info:

% if category == "BitEnum":

Spv${kind}Mask
spirv_string_to_${kind.lower()}(const char *s)
{
    Spv${kind}Mask result = 0;

    // TODO: prefix_matches is still not ideal.
    while (s) {
        if (matches(s, "None")) { }
        % for name,operands in values:
        % if name != "None":
        else if (prefix_matches(s, "${name}")) { result |= Spv${kind}${name}Mask; }
        % endif
        % endfor
        s = strchr(s, '|');
        if (s) s++;
    }

    return result;
}

% elif kind != "Op":

Spv${kind}
spirv_string_to_${kind.lower()}(const char *s)
{
    % for name,operands in values:
    if (matches(s, "${name}")) { return Spv${kind}${name}; }
    % endfor

    return (Spv${kind})0;
}

% else:
struct spirv_op_info
spirv_string_to_op_info(const char *s)
{
    struct spirv_op_info info;
    info.opcode = SpvOpMax;

    if (0) {}
     % for name,operands in values:
    else if (matches(s, "Op${name}")) {
        info.opcode = SpvOp${name};
        static const enum spirv_operands operands[] = {
         % for operand in operands:
             <%
                 v = operand['kind'].upper()
                 if 'quantifier' in operand:
                     if operand['quantifier'] == '?':
                         v += " | OPTIONAL"
                     elif operand['quantifier'] == '*':
                         v += " | STAR"
             %>${v},
         % endfor
            NONE,
        };
        info.operands = operands;
    }
     % endfor

    return info;
}

% endif

% endfor
""")

if __name__ == "__main__":
    pargs = parse_args()

    info = parse_spirv_info(open(pargs.json, "r").read())

    with open(pargs.out, 'w') as f:
        f.write(TEMPLATE.render(info=info))

