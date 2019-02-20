/* -*- c++ -*- */
/*
 * Copyright © 2019 Intel Corporation
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

#ifndef CODEGEN_BUILDER_H
#define CODEGEN_BUILDER_H

#include "brw_fs.h"

enum PACKED operation {
   append_output,

   /**
    * Appends a new operand from the input in the operand list.
    *
    * The input to be appended is selected by \c ::index.
    */
   append_input,

   /**
    * Appends a new operand from a constant in the imm list.
    *
    * The constant to be appended is selected by \c ::index, and its type is
    * specified in \c ::reg_type.
    */
   append_constant,

   /**
    * Appends a new operand from a temporary in the operand list.
    *
    * The temporary to be appended is selected by \c ::index.
    */
   append_temporary,

   /**
    * Appends a new null register in the operand list.
    *
    * The type of the null register is specified in \c ::reg_type.
    */
   append_null_reg,

   append_vec1_grf,
   append_vec2_grf,
   append_vec4_grf,
   append_vec8_grf,
   append_vec16_grf,

   /**
    * Allocate a register for the next temporary slot
    *
    * The type of the temporary is specified in \c ::reg_type.  Registers
    * cannot have type \c BRW_REGISTER_TYPE_VF, \c BRW_REGISTER_TYPE_V, or
    * \c BRW_REGISTER_TYPE_VU.  If the type \c BRW_REGISTER_TYPE_VF is used to
    * declare a temporary, it has the special meaning to copy the result type.
    */
   declare_temporary,

   /**
    * Toggles the negate flag of the last added operand.
    */
   neg_operand,

   /**
    * Sets the absolute value flag of the last added operand.
    *
    * Also clears the negate flag.
    */
   abs_operand,

   /**
    * Changes the type of the last added operand.
    *
    * The new type of the operand is specified in \c ::reg_type.
    */
   retype_operand,

   subscript_operand,

   /**
    * Emit an instruction
    *
    * The opcode of the instruction is specified in \c ::gen_opcode.  All
    * operands are consumed by the new instruction.
    */
   emit_instruction,

   /**
    * Sets the saturate flag for the most recently emitted instruction.
    */
   saturate_instruction,

   /**
    * Sets the conditional modifier for the most recently emitted instruction.
    */
   conditional_mod,

   predicate_instruction,

   /**
    * Denotes the end of the byte code stream.
    */
   end_of_stream
};

struct bytecode_instruction {
   enum operation op;

   union {
      enum opcode gen_opcode;
      enum brw_reg_type reg_type;
      enum brw_conditional_mod cmod;
      enum brw_predicate pred;
      uint8_t blob;
   };

   uint16_t index;

   bytecode_instruction(enum operation _op, enum opcode _gen_opcode)
      : op(_op), gen_opcode(_gen_opcode), index(0)
   {
      assert(op == emit_instruction);
   }

   bytecode_instruction(enum operation _op, enum brw_reg_type _reg_type)
      : op(_op), reg_type(_reg_type), index(0)
   {
      assert(op == retype_operand ||
             op == declare_temporary ||
             op == append_null_reg);
   }

   bytecode_instruction(enum operation _op, enum brw_reg_type _reg_type,
                        uint16_t _index)
      : op(_op), reg_type(_reg_type), index(_index)
   {
      assert(op == subscript_operand ||
             op == append_constant);
   }

   bytecode_instruction(enum operation _op, enum brw_predicate _pred)
      : op(_op), pred(_pred), index(0)
   {
      assert(op == predicate_instruction);
   }

   bytecode_instruction(enum operation _op, enum brw_conditional_mod _cmod)
      : op(_op), cmod(_cmod), index(0)
   {
      assert(op == conditional_mod);
   }

   bytecode_instruction(enum operation _op, uint16_t _index)
      : op(_op), blob(0), index(_index)
   {
      assert(op == append_input ||
             op == append_temporary);
   }

   bytecode_instruction(enum operation _op, uint8_t subnr, uint16_t nr)
      : op(_op), blob(subnr), index(nr)
   {
      assert(op == append_vec1_grf ||
             op == append_vec2_grf ||
             op == append_vec4_grf ||
             op == append_vec8_grf ||
             op == append_vec16_grf);
   }

   bytecode_instruction(enum operation _op)
      : op(_op), blob(0), index(0)
   {
      assert(op == abs_operand ||
             op == neg_operand ||
             op == saturate_instruction ||
             op == append_output ||
             op == end_of_stream);
   }
};

union immediate_value {
   uint64_t uq;
   int64_t q;
   double df;
   float f;
   int   d;
   unsigned ud;
   int16_t w;
   uint16_t uw;
   int8_t b;
   uint8_t ub;
};

//STATIC_ASSERT(sizeof(struct bytecode_instruction) <= 4);

void emit_instructions_from_bytecode(const brw::fs_builder &bld,
                                     const struct bytecode_instruction *bi,
                                     const fs_reg &result,
                                     const fs_reg *inputs,
                                     const union immediate_value *imm,
                                     bool implicit_saturate);

#endif /* CODEGEN_BUILDER_H */
