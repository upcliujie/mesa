#
# Copyright (C) 2019 Intel Corporation
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

from collections import defaultdict
import itertools
import struct
import sys
import os
import mako.template
from enum import Enum, unique

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../compiler/nir/"))
from nir_opcodes import opcodes, type_sizes

import nir_algebraic as noa

# Generate an Intel Gen architecture code generator.

@unique
class OperandType(Enum):
   # Inferred means the type of the operand is inferred from expected source
   # type of the NIR instruction.
   inferred = 0
   B, W, D, Q, UB, UW, UD, UQ, HF, F, DF, VF = range(1, 13)

   def __str__(self):
       return "BRW_REGISTER_TYPE_" + self.name


@unique
class OperandFile(Enum):
   unknown = 0
   destination = 1
   input = 2
   temporary = 3
   constant = 4
   null = 5
   grf = 6


def grf(number, subnumber, width):
   operand = RegisterOperand("g{}.{}".format(number, subnumber))
   operand.nr = number
   operand.subnr = subnumber
   operand.size = width
   operand.file = OperandFile.grf
   return operand


def retype(operand, new_type):
   if not isinstance(operand, Operand):
      operand = Operand.create(operand)

   operand.retype(new_type)
   return operand


def subscript(operand, type, index):
   if not isinstance(operand, Operand):
      operand = Operand.create(operand)

   operand.subscript(type, index)
   return operand


def abs(operand):
   if not isinstance(operand, Operand):
      operand = Operand.create(operand)

   operand.abs()
   return operand


def neg(operand):
   assert not isinstance(operand, Instruction)

   if not isinstance(operand, Operand):
      operand = Operand.create(operand)

   operand.negate()
   return operand


def imm(value, type):
    return ConstantOperand(value, type)


def null(type):
    return retype(RegisterOperand(None), type)


class Operand(object):
   @staticmethod
   def create(val):
      if isinstance(val, bytes):
         val = val.decode('utf-8')

      if isinstance(val, Operand):
         return val
      elif isinstance(val, str):
         return RegisterOperand(val)
      else:
         return ConstantOperand(val)


class ConstantOperand(Operand):
   all_constants = []

   def __init__(self, val, type):
      self.val = val
      self.file = OperandFile.constant
      self.type = type if isinstance(type, OperandType) else OperandType[type]
      self.index = len(ConstantOperand.all_constants)

      assert self.type != OperandType.inferred

      ConstantOperand.all_constants.append(self)


   def negate(self):
      raise NotImplementedError


   def abs(self):
      raise NotImplementedError


   def retype(self, new_type):
      raise NotImplementedError


   def subscript(self, type, index):
      raise NotImplementedError


   def remap_names(self, file, varset):
      pass


   def bytecode_instruction_list(self):
      return ["bytecode_instruction(append_constant, {}, {})".format(self.type, self.index)]


   def __str__(self):
      if self.type == OperandType.Q:
         return "{{ .q = {} }}".format(self.val)
      elif self.type == OperandType.UQ:
         return "{{ .uq = {} }}".format(self.val)
      elif self.type == OperandType.D:
         return "{{ .d = {} }}".format(self.val)
      elif self.type == OperandType.UD:
         return "{{ .ud = {} }}".format(self.val)
      elif self.type == OperandType.W:
         return "{{ .w = {} }}".format(self.val)
      elif self.type == OperandType.UW:
         return "{{ .uw = {} }}".format(self.val)
      elif self.type == OperandType.B:
         return "{{ .b = {} }}".format(self.val)
      elif self.type == OperandType.UB:
         return "{{ .ub = {} }}".format(self.val)
      elif self.type == OperandType.F:
         return "{{ .f = {}f }}".format(self.val)
      elif self.type == OperandType.DF:
         return "{{ .df = {} }}".format(self.val)
      elif self.type == OperandType.HF:
         return "{{ .uw = {} /* .hf = {} */ }}".format(hex(struct.unpack('<H', struct.pack('<e', self.val))[0]), self.val)
      else:
         # Unsupported types for constants
         assert False


class RegisterOperand(Operand):
   def __init__(self, name):
      self.type = OperandType.inferred
      self._abs = False
      self.neg = False
      self.subregion = None
      self.size = 0

      if name == "r":
         self.name = None
         self.file = OperandFile.destination
         self.index = 0
      elif name is None:
          self.name = None
          self.file = OperandFile.null
      else:
         self.name = name
         self.file = OperandFile.unknown
         self.index = 0


   def negate(self):
      self.neg = not self.neg


   def abs(self):
      self.neg = False
      self._abs = True


   def retype(self, new_type):
      if isinstance(new_type, OperandType):
         self.type = new_type
      else:
         self.type = OperandType[new_type]


   def subscript(self, type, index):
      assert self.subregion is None

      if not isinstance(type, OperandType):
         type = OperandType[type]

      self.subregion = (type, index)


   def remap_names(self, file, varset):
      if self.name is not None and self.file != OperandFile.grf:
         if file == OperandFile.input:
            self.file = file
            self.index = varset[self.name]
            self.name = None
         elif self.name in varset:
            self.file = file
            self.index, _ = varset[self.name]
            self.name = None


   def bytecode_instruction_list(self):
      assert self.file != OperandFile.unknown
      assert self.file != OperandFile.constant

      if self.file == OperandFile.destination:
         op = [ "bytecode_instruction(append_output)" ]
      elif self.file == OperandFile.input:
         op = [ "bytecode_instruction(append_input, {})".format(self.index) ]
      elif self.file == OperandFile.temporary:
         op = [ "bytecode_instruction(append_temporary, {})".format(self.index) ]
      elif self.file == OperandFile.null:
         assert self.type != OperandType.inferred
         op = [ "bytecode_instruction(append_null_reg, {})".format(self.type) ]
      elif self.file == OperandFile.grf:
         assert self.size != 0
         op = [ "bytecode_instruction(append_vec{}_grf, {}, {})".format(self.size, self.subnr, self.nr) ]


      if self.file != OperandFile.null:
         if self.type != OperandType.inferred:
            op.append("bytecode_instruction(retype_operand, {})".format(self.type))

         if self._abs:
            op.append("bytecode_instruction(abs_operand)")

         if self.neg:
            op.append("bytecode_instruction(neg_operand)")

         if self.subregion is not None:
            op.append("bytecode_instruction(subscript_operand, {}, {})".format(self.subregion[0], self.subregion[1]))

      return op


class Instruction(object):
   def __init__(self, opcode, dest, src0=None, src1=None, src2=None):
      self.opcode = opcode
      self.saturate_mode = False
      self.conditional_modifier = None
      self.pred = None
      self.dest = Operand.create(dest)
      self.sources = [Operand.create(s) for s in [src0, src1, src2] if s is not None]


   def remap_names(self, file, varset):
      if file == OperandFile.temporary:
         self.dest.remap_names(file, varset)

      [src.remap_names(file, varset) for src in self.sources]


   def saturate(self):
      self.saturate_mode = True
      return self


   def cmod(self, m):
      assert m in ['Z', 'NZ', 'L', 'LE', 'G', 'GE', 'O', 'U', 'EQ', 'NE', 'NEQ', 'R']
      self.conditional_modifier = 'NEQ' if m == 'NE' else m
      return self


   def predicate(self, pred="NORMAL"):
      self.pred = pred
      return self


   def bytecode_instruction_list(self):
      opcode_name = self.opcode if "_OPCODE_" in self.opcode else "BRW_OPCODE_" + self.opcode
      instr = [ "bytecode_instruction(emit_instruction, {})".format(opcode_name) ]

      if self.saturate_mode:
         instr.append("bytecode_instruction(saturate_instruction)")

      if self.conditional_modifier is not None:
         instr.append("bytecode_instruction(conditional_mod, BRW_CONDITIONAL_{})".format(self.conditional_modifier))

      if self.pred is not None:
         instr.append("bytecode_instruction(predicate_instruction, BRW_PREDICATE_{})".format(self.pred))

      return self.dest.bytecode_instruction_list() +\
             [x for src in self.sources for x in src.bytecode_instruction_list() ] +\
             instr


class TempReg(object):
   """Declare a temporary register in a list of instructions."""

   def __init__(self, name, type):
      self.name = name
      self.type = type


class InstructionList(object):
   """Represents a list of instructions and any temporary registers used by those
   instructions.

   """

   def __init__(self, instructions):
      self.instructions = []
      self.temporaries = {}

      temp_ids = itertools.count()
      for item in instructions:
         if isinstance(item, TempReg):
            self.temporaries[item.name] = (next(temp_ids), OperandType[item.type])
         else:
            assert(isinstance(item, Instruction))
            self.instructions.append(item)

      [instr.remap_names(OperandFile.temporary, self.temporaries) for instr in self.instructions]


   def remap_names(self, varset):
      [instr.remap_names(OperandFile.input, varset) for instr in self.instructions]


   def bytecode_instruction_list(self):
      declare_temps = ["bytecode_instruction(declare_temporary, {})".format(type) for (_, (_, type)) in self.temporaries.items()]
      instr = [x for instr in self.instructions for x in instr.bytecode_instruction_list()]

      return declare_temps + instr

_optimization_ids = itertools.count()

condition_list = ['true']

class SearchAndReplace(object):
   def __init__(self, transform):
      self.id = next(_optimization_ids)

      search = transform[0]
      replace = transform[1]
      if len(transform) > 2:
         self.condition = transform[2]
      else:
         self.condition = 'true'

      if self.condition not in condition_list:
         condition_list.append(self.condition)
      self.condition_index = condition_list.index(self.condition)

      varset = noa.VarSet()
      if isinstance(search, noa.Expression):
         self.search = search
      else:
         self.search = noa.Expression(search, "search{0}".format(self.id), varset)

      varset.lock()

      if isinstance(replace, list):
         self.replace = InstructionList(replace)
      else:
         assert isinstance(replace, Instruction)
         self.replace = InstructionList([replace])

      noa.BitSizeValidator(varset).validate(self.search, self.replace)

      self.replace.remap_names(varset)

_code_generator_template = mako.template.Template("""
#include "nir.h"
#include "brw_fs.h"
#include "brw_nir.h"
#include "brw_eu.h"
#include "codegen_builder.h"
#include "nir_search.h"
#include "nir_search_helpers.h"
#include "brw_nir_search_helpers.h"

using namespace brw;

struct codegen_transform {
   const nir_search_expression *search;
   unsigned bytecode;
   unsigned condition_offset;
};

static const struct bytecode_instruction ${pass_name}_bytecode[] = {
% for b in bytecode:
   ${b},
% endfor
};

static const union immediate_value ${pass_name}_immediates[] = {
% for i in immediates:
   ${i},
% endfor
};

<% cache = {} %>
% for (opcode, xform_list) in sorted(opcode_xforms.items()):
   % for xform in xform_list:
${xform.search.render(cache)}
   % endfor
% endfor


static const struct codegen_transform ${pass_name}_xforms[] = {
% for name, first_bytecode, condition_index in expression_bytecode_tuples:
   { &${cache.get(name, name)}, ${first_bytecode}, ${condition_index} },
% endfor
};

bool
nir_emit_alu_${pass_name}(fs_visitor *v, const struct gen_device_info *devinfo,
                          struct hash_table *range_ht,
                          const fs_builder &bld, nir_alu_instr *alu,
                          bool need_dest)
{
   const struct brw_wm_prog_key *const fs_key = (struct brw_wm_prog_key *) v->key;
   const unsigned execution_mode =
      bld.shader->nir->info.float_controls_execution_mode;

   /* These may be unused by some generations. */
   (void) fs_key;
   (void) execution_mode;

   const bool condition_flags[${len(condition_list)}] = {
   % for index, condition in enumerate(condition_list):
      ${condition},        /* ${index} */
   % endfor
   };

   unsigned first = 0;
   unsigned count = 0;

   switch (alu->op) {
   % for opcode in opcode_dict.keys():
   case nir_op_${opcode}:
      first = ${opcode_dict[opcode][0]};
      count = ${opcode_dict[opcode][1]};
      break;

   % endfor
   % for opcode, reason in unsupported:
      % if not isinstance(opcode, str):
         % for o in opcode:
   case nir_op_${o}:
         % endfor
      % else:
   case nir_op_${opcode}:
      % endif
      unreachable("${reason}");
   % endfor
   default:
      /* unreachable("unhandled instruction"); */
      break;
   }

   for (unsigned i = 0; i < count; i++) {
      uint8_t swizzle[NIR_MAX_VEC_COMPONENTS];
      unsigned num_variables;
      nir_alu_src variables[NIR_SEARCH_MAX_VARIABLES];
      uint8_t which_src[NIR_SEARCH_MAX_VARIABLES];

      if (condition_flags[${pass_name}_xforms[first + i].condition_offset] &&
          nir_match_instr(alu, ${pass_name}_xforms[first + i].search,
                          range_ht, swizzle, &num_variables, variables,
                          which_src)) {
         /* Deal with the destination of the instruction. */
         fs_reg result =
            need_dest ? v->get_nir_dest(alu->dest.dest) : bld.null_reg_ud();

         result.type = brw_type_for_nir_type(devinfo,
            (nir_alu_type)(nir_op_infos[alu->op].output_type |
                           nir_dest_bit_size(alu->dest.dest)));

         /* This code path should never see an instruction that operates on
          * more than a single channel.  Therefore, we can just adjust the
          * source and destination registers for that channel and emit the
          * instruction.
          */
         unsigned channel = 0;
         if (nir_op_infos[alu->op].output_size == 0) {
            /* Since NIR is doing the scalarizing for us, we should only ever
             * see vectorized operations with a single channel.
             */
            assert(util_bitcount(alu->dest.write_mask) == 1);
            channel = ffs(alu->dest.write_mask) - 1;

            result = offset(result, bld, channel);
         }

         /* Deal with the sources of the "instruction." */
         fs_reg op[NIR_SEARCH_MAX_VARIABLES];
         for (unsigned i = 0; i < num_variables; i++) {
            /* Instruction that originally used this variable. */
            assert(variables[i].src.parent_instr->type == nir_instr_type_alu);
            nir_alu_instr *const orig_user =
               nir_instr_as_alu(variables[i].src.parent_instr);

            op[i] = v->get_nir_src(variables[i].src);
            op[i].type = brw_type_for_nir_type(devinfo,
               (nir_alu_type)(nir_op_infos[orig_user->op].input_types[which_src[i]] |
                              nir_src_bit_size(variables[i].src)));
            op[i].abs = variables[i].abs;
            op[i].negate = variables[i].negate;

            assert(nir_op_infos[orig_user->op].input_sizes[i] < 2);
            op[i] = offset(op[i], bld, orig_user->src[which_src[i]].swizzle[channel]);
         }

         emit_instructions_from_bytecode(bld,
                                         &${pass_name}_bytecode[${pass_name}_xforms[first + i].bytecode],
                                         result,
                                         op,
                                         ${pass_name}_immediates,
                                         alu->dest.saturate);

         if (devinfo->gen <= 5 &&
             !result.is_null() &&
             (alu->instr.pass_flags & BRW_NIR_BOOLEAN_MASK) == BRW_NIR_BOOLEAN_NEEDS_RESOLVE) {
            fs_reg masked = v->vgrf(glsl_type::int_type);
            bld.AND(masked, result, brw_imm_d(1));
            masked.negate = true;
            bld.MOV(retype(result, BRW_REGISTER_TYPE_D), masked);
         }

         return true;
      }
   }

   return false;
}
""")


class CodeGeneratorGenerator(noa.AlgebraicPass):
   def __init__(self, pass_name, transforms, unsupported):
      super().__init__(pass_name, transforms, SearchAndReplace)
      self.unsupported = unsupported


   def render(self):
      # Generate two tables reprensenting the data related to the code
      # transformations:
      #
      # 1. Table of bytecode instructions.
      # 2. Table of <expression, bytecode> tuples.

      bytecode = []
      expression_bytecode_tuples = []
      opcode_dict = defaultdict(list)

      for (opcode, xform_list) in sorted(self.opcode_xforms.items()):
         first = len(expression_bytecode_tuples)

         for xform in xform_list:
            expression_bytecode_tuples.append((xform.search.name, len(bytecode), xform.condition_index))
            bytecode = bytecode + xform.replace.bytecode_instruction_list() + ["bytecode_instruction(end_of_stream)"]

         count = len(expression_bytecode_tuples) - first
         opcode_dict[opcode] = (first, count)

      return _code_generator_template.render(pass_name=self.pass_name,
                                             opcode_xforms=self.opcode_xforms,
                                             bytecode=bytecode,
                                             expression_bytecode_tuples=expression_bytecode_tuples,
                                             opcode_dict=opcode_dict,
                                             immediates=ConstantOperand.all_constants,
                                             condition_list=condition_list,
                                             unsupported=self.unsupported)
