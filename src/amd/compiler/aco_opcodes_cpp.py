
template = """\
/* 
 * Copyright (c) 2018 Valve Corporation
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
 *
 */

#include "aco_ir.h"

namespace aco {


<%
opcode_names = sorted(opcodes.keys())
can_use_input_modifiers = "".join([opcodes[name].input_mod for name in reversed(opcode_names)])
can_use_output_modifiers = "".join([opcodes[name].output_mod for name in reversed(opcode_names)])
is_atomic = "".join([opcodes[name].is_atomic for name in reversed(opcode_names)])
%>

extern const aco::Info instr_info = {
   .opcode_gfx7 = {
      % for name in opcode_names:
      ${opcodes[name].opcode_gfx7},
      % endfor
   },
   .opcode_gfx9 = {
      % for name in opcode_names:
      ${opcodes[name].opcode_gfx9},
      % endfor
   },
   .opcode_gfx10 = {
      % for name in opcode_names:
      ${opcodes[name].opcode_gfx10},
      % endfor
   },
   .can_use_input_modifiers = std::bitset<${len(opcode_names)}>("${can_use_input_modifiers}"),
   .can_use_output_modifiers = std::bitset<${len(opcode_names)}>("${can_use_output_modifiers}"),
   .is_atomic = std::bitset<${len(opcode_names)}>("${is_atomic}"),
   .name = {
      % for name in opcode_names:
      "${name}",
      % endfor
   },
   .format = {
      % for name in opcode_names:
      aco::Format::${str(opcodes[name].format.name)},
      % endfor
   },
   .operand_size = {
      % for name in opcode_names:
      ${opcodes[name].operand_size},
      % endfor
   },
   .definition_size = {
      % for name in opcode_names:
      ${opcodes[name].definition_size},
      % endfor
   },
   .classes = {
      % for name in opcode_names:
      (instr_class)${opcodes[name].cls.value},
      % endfor
   },
};

aco_opcode v_cmp_to_cmpx(aco_opcode c)
{
   switch (c) {
   % for name in opcode_names:
   % if name.startswith("v_cmp_"):
   case aco_opcode::${name}:
      return aco_opcode::${name.replace("v_cmp_", "v_cmpx_")};
   % endif
   % endfor
   default:
      return aco_opcode::num_opcodes;
   }

   return aco_opcode::num_opcodes;
}

bool salu_reads_exec_implicitly(aco_opcode c)
{
   switch (c) {
   % for name in opcode_names:
   % if name.startswith("s_") and ("saveexec" in name or "wrexec" in name):
   case aco_opcode::${name}:
   % endif
   % endfor
      return true;
   default:
      return false;
   }

   return false;
}

}
"""

from aco_opcodes import opcodes
from mako.template import Template

print(Template(template).render(opcodes=opcodes))
