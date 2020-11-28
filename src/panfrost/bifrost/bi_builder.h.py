# Copyright (C) 2020 Collabora, Ltd.
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

SKIP = set(["lane", "lanes", "lanes", "replicate", "swz", "widen", "swap", "neg", "abs", "not", "sign", "extend", "divzero"])

TEMPLATE = """
#ifndef _BI_BUILDER_H_
#define _BI_BUILDER_H_

#include "compiler.h"

% for opcode in ops:
static inline
bi_instr * bi_${opcode.replace('.', '_').lower()}_to(${signature(ops[opcode], 1, modifiers)})
{
    bi_instr *I = rzalloc(b->shader, bi_instr);
    I->op = BI_OPCODE_${opcode.replace('.', '_').upper()};
    I->dest[0] = dest0;
% for src in range(src_count(ops[opcode])):
    I->src[${src}] = src${src};
% endfor
% for mod in ops[opcode]["modifiers"]:
% if mod[0:-1] not in SKIP and mod not in SKIP:
    I->${mod} = ${mod};
% endif
% endfor
% for imm in ops[opcode]["immediates"]:
    I->${imm} = ${imm};
% endfor
    bi_builder_insert(&b->cursor, I);
    return I;
}

% if opcode.split(".")[0] not in ["JUMP", "BRANCHZ", "BRANCH"]:
static inline
bi_index bi_${opcode.replace('.', '_').lower()}(${signature(ops[opcode], 0, modifiers)})
{
    return (bi_${opcode.replace('.', '_').lower()}_to(${arguments(ops[opcode], 1)}))->dest[0];
}

%endif
%endfor
#endif"""

import sys
from bifrost_isa import *
from mako.template import Template

instructions = parse_instructions(sys.argv[1], include_pseudo = True)
ir_instructions = partition_mnemonics(instructions)
modifier_lists = order_modifiers(ir_instructions)

# Generate type signature for a builder routine

def should_skip(mod):
    return mod in SKIP or mod[0:-1] in SKIP

def modifier_signature(op):
    return sorted([m for m in op["modifiers"].keys() if not should_skip(m)])

def signature(op, dest_count, modifiers):
    return ", ".join(
        ["bi_builder *b"] +
        ["bi_index dest{}".format(i) for i in range(dest_count)] +
        ["bi_index src{}".format(i) for i in range(src_count(op))] +
        ["{} {}".format(
        "bool" if len(modifiers[T[0:-1]] if T[-1] in "0123" else modifiers[T]) == 2 else
        "enum bi_" + T[0:-1] if T[-1] in "0123" else
        "enum bi_" + T,
        T) for T in modifier_signature(op)] +
        ["uint32_t {}".format(imm) for imm in op["immediates"]])

def arguments(op, dest_count):
    return ", ".join(
        ["b"] +
        (["bi_temp(b->shader)"] * dest_count) +
        ["src{}".format(i) for i in range(src_count(op))] +
        modifier_signature(op) +
        op["immediates"])

print(Template(COPYRIGHT + TEMPLATE).render(ops = ir_instructions, modifiers = modifier_lists, signature = signature, arguments = arguments, src_count = src_count, SKIP = SKIP))
