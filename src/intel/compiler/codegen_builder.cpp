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

#include "codegen_builder.h"

void
emit_instructions_from_bytecode(const brw::fs_builder &bld,
                                const struct bytecode_instruction *bi,
                                const fs_reg &result,
                                const fs_reg *inputs,
                                const union immediate_value *imm,
                                bool implicit_saturate)
{
   fs_reg operands[16];
   unsigned num_operands = 0;
   fs_reg temporaries[16];
   unsigned num_temps = 0;
   fs_inst *inst = NULL;

   while (bi->op != end_of_stream) {
      switch (bi->op) {
      case append_output:
         assert(num_operands < ARRAY_SIZE(operands));
         operands[num_operands++] = result;
         break;

      case append_input:
         assert(num_operands < ARRAY_SIZE(operands));
         operands[num_operands++] = inputs[bi->index];
         break;

      case append_constant:
         assert(num_operands < ARRAY_SIZE(operands));

         switch (bi->reg_type) {
         case BRW_REGISTER_TYPE_DF:
            operands[num_operands++] = brw_imm_df(imm[bi->index].df);
            break;
         case BRW_REGISTER_TYPE_F:
            operands[num_operands++] = brw_imm_f(imm[bi->index].f);
            break;

         case BRW_REGISTER_TYPE_Q:
            operands[num_operands++] = brw_imm_q(imm[bi->index].q);
            break;
         case BRW_REGISTER_TYPE_UQ:
            operands[num_operands++] = brw_imm_uq(imm[bi->index].uq);
            break;
         case BRW_REGISTER_TYPE_D:
            operands[num_operands++] = brw_imm_d(imm[bi->index].d);
            break;
         case BRW_REGISTER_TYPE_UD:
            operands[num_operands++] = brw_imm_ud(imm[bi->index].ud);
            break;
         case BRW_REGISTER_TYPE_W:
            operands[num_operands++] = brw_imm_w(imm[bi->index].w);
            break;
         case BRW_REGISTER_TYPE_UW:
            operands[num_operands++] = brw_imm_uw(imm[bi->index].uw);
            break;

         case BRW_REGISTER_TYPE_NF:
         case BRW_REGISTER_TYPE_HF:
         case BRW_REGISTER_TYPE_VF:
         case BRW_REGISTER_TYPE_B:
         case BRW_REGISTER_TYPE_UB:
         case BRW_REGISTER_TYPE_V:
         case BRW_REGISTER_TYPE_UV:
            unreachable("Unsupported type");
         };
         break;

      case append_temporary:
         assert(num_operands < ARRAY_SIZE(operands));
         assert(bi->index < num_temps);
         operands[num_operands++] = temporaries[bi->index];
         break;

      case append_null_reg:
         assert(num_operands < ARRAY_SIZE(operands));
         operands[num_operands++] = retype(bld.null_reg_ud(),
                                           bi->reg_type);
         break;

      case append_vec1_grf:
         assert(num_operands < ARRAY_SIZE(operands));
         operands[num_operands++] = brw_vec1_grf(bi->index, bi->blob);
         break;

      case append_vec2_grf:
         assert(num_operands < ARRAY_SIZE(operands));
         operands[num_operands++] = brw_vec2_grf(bi->index, bi->blob);
         break;

      case append_vec4_grf:
         assert(num_operands < ARRAY_SIZE(operands));
         operands[num_operands++] = brw_vec4_grf(bi->index, bi->blob);
         break;

      case append_vec8_grf:
         assert(num_operands < ARRAY_SIZE(operands));
         operands[num_operands++] = brw_vec8_grf(bi->index, bi->blob);
         break;

      case append_vec16_grf:
         assert(num_operands < ARRAY_SIZE(operands));
         operands[num_operands++] = brw_vec16_grf(bi->index, bi->blob);
         break;

      case declare_temporary:
         assert(num_temps < ARRAY_SIZE(temporaries));

         if (bi->reg_type == BRW_REGISTER_TYPE_VF)
            temporaries[num_temps++] = bld.vgrf(result.type, 1);
         else
            temporaries[num_temps++] = bld.vgrf(bi->reg_type, 1);
         break;

      case neg_operand:
         assert(num_operands > 0);
         operands[num_operands - 1].negate = !operands[num_operands - 1].negate;
         break;

      case abs_operand:
         assert(num_operands > 0);
         operands[num_operands - 1].negate = false;
         operands[num_operands - 1].abs = true;
         break;

      case retype_operand:
         assert(num_operands > 0);
         operands[num_operands - 1] = retype(operands[num_operands - 1],
                                             bi->reg_type);
         break;

      case subscript_operand:
         assert(num_operands > 0);
         operands[num_operands - 1] = subscript(operands[num_operands - 1],
                                                bi->reg_type,
                                                bi->index);
         break;

      case emit_instruction:
         if (num_operands == 0) {
            inst = bld.emit(bi->gen_opcode);
         } else {
            inst = bld.emit(bi->gen_opcode,
                            operands[0],
                            &operands[1],
                            num_operands - 1);
            num_operands = 0;
         }

         break;

      case saturate_instruction:
         assert(inst != NULL);
         inst->saturate = true;
         break;

      case conditional_mod:
         assert(inst != NULL);
         inst->conditional_mod = bi->cmod;
         break;

      case predicate_instruction:
         assert(inst != NULL);
         inst->predicate = bi->pred;
         break;

      case end_of_stream:
         unreachable("Should have already exited loop.");
      }

      bi++;
   }

   if (implicit_saturate)
      inst->saturate = true;

   /* There should be no operands hanging around. */
   assert(num_operands == 0);
}
