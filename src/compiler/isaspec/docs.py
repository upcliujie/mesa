#!/usr/bin/env python3
#
# Copyright © 2022 Etnaviv Project
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

import json
import textwrap
from isa import ISA, get_bitrange
import re
import sys

xml = sys.argv[1]
dst = sys.argv[2]

isa = ISA(xml)

def trim_string(string):
    return textwrap.dedent(string).strip()

def create_rst_section(title, content):
    print(f"{title}\n{'-' * len(title)}\n\n{content}\n\n")

def create_rst_subsection(title):
    print(f"{title}\n{'~' * len(title)}\n\n")

def create_rst_table(data):
    col_widths = [max(len(cell) for cell in column) for column in zip(*data)]
    table_content = "\n".join("  ".join(cell.ljust(width) for cell, width in zip(row, col_widths)) for row in data)
    header_separator = "  ".join("=" * width for width in col_widths)

    print(f"{header_separator}\n{table_content}\n{header_separator}\n")

def print_introduction(section, description):
    create_rst_section(section, textwrap.dedent(description).strip())

def print_enum(name, enum):
    create_rst_subsection(name)

    if enum.doc is not None:
        print(f"{trim_string(enum.doc.text)}")
        print("")

    data = [
        ["Index", "Value"]
    ]

    for value_name, value in enum.values.items():
        data.append([value_name, value.get_name()])

    create_rst_table(data)

class Pattern(object):
    """Class that encapsulates a single bitset rule
    """
    def __init__(self, low, high, text):
        self.low = low
        self.high = high
        self.name = text

def instruction_bits(ins):
    ranges = []

    for pattern in ins.pattern:
        low, high = get_bitrange(pattern)
        ranges.append(Pattern(low, high, pattern.text.strip()))

    for case in ins.cases:
        for field_name, field in case.fields.items():
            if any(x.low == field.low for x in ranges) is False:
                ranges.append(field)

    if ins.extends is not None:
        parent = isa.bitsets[ins.extends]
        ranges += instruction_bits(parent)

    return ranges

def print_instruction(instr):
    ranges = sorted(instruction_bits(instr), key=lambda x: x.low)

    bitfields_data = []

    for range in ranges:
        bitfield = {"bits": range.high - range.low + 1}
        if hasattr(range, "name"):
            bitfield["name"] = range.name
        if hasattr(range, "attr"):
            bitfield["attr"] = range.attr
        if hasattr(range, "type"):
            bitfield["type"] = range.type
        bitfields_data.append(bitfield)

    json_output = json.dumps(bitfields_data, indent=4)
    print(json_output)

def print_instruction(instr):
    create_rst_subsection(instr.get_c_name())

    if instr.doc is not None:
        print(f"{trim_string(instr.doc.text)}")
        print("")

    ranges = sorted(instruction_bits(instr), key=lambda x: x.low)
    bitfields_data = []

    for range in ranges:
        how_many_bits = range.high - range.low + 1
        bitfield = {"bits": how_many_bits}
        bitfield["name"] = range.name

        if len(range.name) > how_many_bits:
            bitfield["rotate"] = -90

        bitfields_data.append(bitfield)

    json_output = json.dumps(bitfields_data, indent=4)
    indented_json = "\n".join("           " + line for line in json_output.splitlines())

    print('.. container:: highlight')
    print('')
    print('   .. bitfield::')
    print('       :hspace: 964')
    print('       :vspace: 160')
    print('       :uneven:')
    print(f"       :bits: {isa.bitsize}")
    print(f"       :lanes: {int(isa.bitsize / 32)}")
    print('')
    print(indented_json)
    print('')
    print('')

with open(dst, 'w') as f:
    sys.stdout = f

    print_introduction("Enumerations", """
            This section describes each enumeration used in the ISA.
            Enumerations are found in the instruction metadata and as
            modifiers in individual instructions.""")

    for enum_name, enum in isa.enums.items():
        print_enum(enum_name, enum)

    print_introduction("Instruction reference", """
            The following section each known instruction in the ISA.
            It contains the instruction name, syntax, and bit pattern.""")

    for instr in isa.instructions():
        print_instruction(instr)
