/*
 * Copyright © 2020 Valve Corporation
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
 * Authors:
 *    Timur Kristóf <timur.kristof@gmail.com
 *
 */

#include "aco_ir.h"

#include <vector>
#include <bitset>
#include <algorithm>

namespace aco {

namespace {

enum pr_opt_label
{
   label_vcc_to_scc,
   label_scc,
   label_scc_to_sgpr,
   label_try_late_shortcircuit,
   num_labels,
};

struct pr_opt_ssa_info
{
   std::bitset<num_labels> labels;
   Block *block = nullptr;
   int instr_idx = -1;

   constexpr Instruction *instr()
   {
      if (block == nullptr || instr_idx == -1)
         return nullptr;

      return block->instructions[instr_idx].get();
   }
};

struct pr_opt_ctx
{
   Program *program;
   Block *current_block;
   uint32_t last_vcc_def;
   uint32_t last_scc_def;
   int current_instr_idx;
   std::vector<uint16_t> uses;
   std::vector<pr_opt_ssa_info> info;
};

void set_label(pr_opt_ctx &ctx, uint32_t tempId, pr_opt_label label)
{
   auto &info = ctx.info[tempId];
   const Instruction *instr = ctx.current_block->instructions[ctx.current_instr_idx].get();

   /* When  */
   if (info.instr() && info.instr() != instr)
      info.labels.reset();

   info.labels.set(label);
   info.block = ctx.current_block;
   info.instr_idx = ctx.current_instr_idx;
}

bool process_shortcircuit_uniform_bool(pr_opt_ctx &ctx, aco_ptr<Instruction> &instr)
{
   /*
    * Eliminates an SCC copy by applying a "short-circuit" to uniform boolean logic.
    * In order to do the short-circuit transform, the pattern should look like this:
    *
    * p_parallelcopy sN, scc    ; used only once
    * s_cmp_xxx A, B            ; used only once
    * s_and/or_b32 sM, sN, scc  ; only scc definition used
    *
    * The p_parallelcopy is replaced by an s_cselect and s_cmp_xxx adjusted
    * to use the result from the cselect, and s_and is eliminated:
    *
    * s_cselect sN, X, Y, scc   ; X and Y depend on the opcodes
    * s_cmp_xxx sN, B           ; scc is equivalent to what was previously produced by s_and/or_b32
    *
    * Possible future improvement (likely more hassle than is worth):
    * - When the transformation is not doable, try to reorder the s_cmp_xxx with the source of the p_parallelcopy
    */

   assert(ctx.uses[instr->definitions[0].tempId()] == 0 ||
          (ctx.uses[instr->definitions[0].tempId()] == 1 &&
           ctx.uses[instr->definitions[1].tempId()] == 0));

   /* Operand 0 is: s_mov sN, scc */
   Instruction *op0_scc2sgpr = ctx.info[instr->operands[0].tempId()].instr();
   /* Operand 1 is: s_cmp ... */
   Instruction *op1_scc = ctx.info[instr->operands[1].tempId()].instr();

   if (op1_scc->format == Format::SOPC) {
      /* Make sure if there is a constant, it's always in the 2nd operand */
      if (op1_scc->operands[0].isConstant()) {
         /* Flip the opcode so that it has the same meaning with the constant in the 2nd operand */
         aco_opcode op = aco_opcode::num_opcodes;
         /* NOTE: s_cmp_le and s_cmp_gt are not used in this manner, so those are not covered here */
         switch (op1_scc->opcode) {
         case aco_opcode::s_cmp_lt_u32:
            op = aco_opcode::s_cmp_gt_u32;
            break;
         case aco_opcode::s_cmp_lt_i32:
            op = aco_opcode::s_cmp_gt_i32;
            break;
         case aco_opcode::s_cmp_ge_u32:
            op = aco_opcode::s_cmp_le_u32;
            break;
         case aco_opcode::s_cmp_ge_i32:
            op = aco_opcode::s_cmp_le_i32;
            break;
         case aco_opcode::s_cmp_eq_u32:
         case aco_opcode::s_cmp_eq_i32:
         case aco_opcode::s_cmp_lg_u32:
         case aco_opcode::s_cmp_lg_i32:
            op = op1_scc->opcode;
            break;
         default:
            return false;
         }

         op1_scc->opcode = op;
         std::swap(op1_scc->operands[0], op1_scc->operands[1]);
      }
   }

   Operand csel_op0;
   Operand csel_op1;

   if (op1_scc->opcode == aco_opcode::s_cmp_eq_u32 ||
       op1_scc->opcode == aco_opcode::s_cmp_eq_i32) {
      /* a && (b == c) => (a ? b : !c) == c (only when c is constant)
       * a || (b == c) => (a ? c : b) == c
       */
      if (!(instr->opcode == aco_opcode::s_or_b32 ||
            op1_scc->operands[1].isConstant()))
         return false;

      csel_op0 = instr->opcode == aco_opcode::s_or_b32
               ? op1_scc->operands[1]
               : op1_scc->operands[0];
      csel_op1 = instr->opcode == aco_opcode::s_or_b32
               ? op1_scc->operands[0]
               : Operand((uint32_t) !op1_scc->operands[1].constantValue());
   } else if (op1_scc->opcode == aco_opcode::s_cmp_lg_u32 ||
              op1_scc->opcode == aco_opcode::s_cmp_lg_i32) {
      /* a && (b != c) => (a ? b : c) != c
       * a || (b != c) => (a ? !c : b) != c (only when c is constant)
       */
      if (!(instr->opcode == aco_opcode::s_and_b32 ||
            op1_scc->operands[1].isConstant()))
         return false;

      csel_op0 = instr->opcode == aco_opcode::s_or_b32
               ? Operand((uint32_t) !op1_scc->operands[1].constantValue())
               : op1_scc->operands[0];
      csel_op1 = instr->opcode == aco_opcode::s_or_b32
               ? op1_scc->operands[0]
               : op1_scc->operands[1];
   } else if (op1_scc->opcode == aco_opcode::s_cmp_lt_u32 ||
              op1_scc->opcode == aco_opcode::s_cmp_gt_u32 ||
              op1_scc->opcode == aco_opcode::s_cmp_lt_i32 ||
              op1_scc->opcode == aco_opcode::s_cmp_gt_i32) {
      /* a && (b < c) => (a ? b : c) < c
       * a && (b > c) => (a ? b : c) > c
       * a || (b < c) => (a ? MIN : b) < c (only when c is constant and c != min)
       * a || (b > c) => (a ? MAX : b) > c (only when c is constant and c != max)
       */

      uint32_t const_op;
      if (op1_scc->opcode == aco_opcode::s_cmp_lt_u32)
         const_op = 0u;
      else if (op1_scc->opcode == aco_opcode::s_cmp_lt_i32)
         const_op = (uint32_t) INT32_MIN;
      else if (op1_scc->opcode == aco_opcode::s_cmp_gt_u32)
         const_op = UINT32_MAX;
      else if (op1_scc->opcode == aco_opcode::s_cmp_gt_i32)
         const_op = INT32_MAX;
      else
         unreachable("unsupported s_cmp opcode");

      if (instr->opcode != aco_opcode::s_and_b32 &&
          (!op1_scc->operands[1].isConstant() ||
           op1_scc->operands[1].constantEquals(const_op)))
         return false;

      csel_op0 = instr->opcode == aco_opcode::s_or_b32
               ? Operand(const_op)
               : op1_scc->operands[0];
      csel_op1 = instr->opcode == aco_opcode::s_or_b32
               ? op1_scc->operands[0]
               : op1_scc->operands[1];
   } else if (op1_scc->opcode == aco_opcode::s_cmp_ge_u32 ||
              op1_scc->opcode == aco_opcode::s_cmp_le_u32 ||
              op1_scc->opcode == aco_opcode::s_cmp_ge_i32 ||
              op1_scc->opcode == aco_opcode::s_cmp_le_i32) {
      /* a && (b >= c) => (a ? b : MIN) >= c (only when c is constant and c != min)
       * a && (b <= c) => (a ? b : MAX) <= c (only when c is constant and c != max)
       * a || (b >= c) => (a ? c : b) >= c
       * a || (b <= c) => (a ? c : b) <= c
       */

      uint32_t const_op;
      if (op1_scc->opcode == aco_opcode::s_cmp_ge_u32)
         const_op = 0u;
      else if (op1_scc->opcode == aco_opcode::s_cmp_ge_i32)
         const_op = (uint32_t) INT32_MIN;
      else if (op1_scc->opcode == aco_opcode::s_cmp_le_u32)
         const_op = UINT32_MAX;
      else if (op1_scc->opcode == aco_opcode::s_cmp_le_i32)
         const_op = INT32_MAX;
      else
         unreachable("unsupported s_cmp opcode");

      if (instr->opcode != aco_opcode::s_or_b32 &&
          (!op1_scc->operands[1].isConstant() ||
           op1_scc->operands[1].constantEquals(const_op)))
         return false;

      csel_op0 = instr->opcode == aco_opcode::s_or_b32
               ? op1_scc->operands[1]
               : op1_scc->operands[0];
      csel_op1 = instr->opcode == aco_opcode::s_or_b32
               ? op1_scc->operands[0]
               : Operand(const_op);
   } else {
      return false;
   }

   /* Create a conditional select which will replace the SCC copy */
   SOP2_instruction *csel = create_instruction<SOP2_instruction>(aco_opcode::s_cselect_b32, Format::SOP2, 3, 1);
   csel->definitions[0] = op0_scc2sgpr->definitions[0];
   csel->operands[0] = csel_op0;
   csel->operands[1] = csel_op1;
   csel->operands[2] = op0_scc2sgpr->operands[0];

   /* Replace SCC copy */
   Block *scc2sgpr_block = ctx.info[instr->operands[0].tempId()].block;
   int scc2sgpr_idx = ctx.info[instr->operands[0].tempId()].instr_idx;
   scc2sgpr_block->instructions[scc2sgpr_idx].reset(csel);

   /* Edit the scc producer to use the definition from the cselect, and replace the current instr */
   op1_scc->definitions[0] = instr->definitions[1];
   op1_scc->operands[0].setTemp(csel->definitions[0].getTemp());
   op1_scc->operands[0].setFixed(csel->definitions[0].physReg());

   /* Delete the current instr */
   instr.reset();

   return true;
}

void process_instruction(pr_opt_ctx &ctx, aco_ptr<Instruction> &instr)
{
   ctx.current_instr_idx++;

   /* Mark when an instruction converts VCC into SCC */
   if ((instr->opcode == aco_opcode::s_and_b64 || /* wave64 */
        instr->opcode == aco_opcode::s_and_b32) && /* wave32 */
       !instr->operands[0].isConstant() &&
       instr->operands[0].physReg() == vcc &&
       !instr->operands[1].isConstant() &&
       instr->operands[1].physReg() == exec) {
      set_label(ctx, instr->definitions[1].tempId(), label_vcc_to_scc);
   }

   /* Mark when an instruction produces SCC */
   if (instr->isSALU() &&
       (instr->definitions.size() == 1 &&
        instr->definitions[0].physReg() == scc)) {
      set_label(ctx, instr->definitions[0].tempId(), label_scc);
   } else if (instr->isSALU() &&
              (instr->definitions.size() == 2 &&
               instr->definitions[1].physReg() == scc)) {
      set_label(ctx, instr->definitions[1].tempId(), label_scc);
   }

   /* Mark when an instruction copies SCC into another SGPR */
   if (instr->opcode == aco_opcode::p_parallelcopy &&
       instr->operands.size() == 1 &&
       !instr->operands[0].isConstant() &&
       instr->operands[0].physReg() == scc) {
      set_label(ctx, instr->definitions[0].tempId(), label_scc_to_sgpr);
   }

   /* When consuming an SCC which was converted from VCC in the same block, use VCC directly */
   if (instr->format == Format::PSEUDO_BRANCH &&
       instr->operands.size() == 1 &&
       !instr->operands[0].isConstant() &&
       instr->operands[0].physReg() == scc &&
       ctx.info[instr->operands[0].tempId()].labels.test(label_vcc_to_scc) &&
       ctx.info[instr->operands[0].tempId()].block == ctx.current_block &&
       ctx.info[instr->operands[0].tempId()].instr()->operands[0].tempId() == ctx.last_vcc_def) {
      Instruction *vcc2scc = ctx.info[instr->operands[0].tempId()].instr();
      ctx.uses[instr->operands[0].tempId()]--;
      instr->operands[0] = vcc2scc->operands[0];
   }

   /* Short-circuit uniform boolean and/or */
   if (instr->opcode == aco_opcode::s_and_b32 ||
       instr->opcode == aco_opcode::s_or_b32) {
      /* Move SCC to the second operand */
      if (!instr->operands[0].isConstant() && instr->operands[0].physReg() == scc)
         std::swap(instr->operands[0], instr->operands[1]);

      if (!instr->operands[0].isConstant() &&
          ctx.uses[instr->operands[0].tempId()] == 1 &&
          ctx.info[instr->operands[0].tempId()].labels.test(label_scc_to_sgpr) &&
          !instr->operands[1].isConstant() &&
          instr->operands[1].physReg() == scc &&
          ctx.uses[instr->operands[1].tempId()] == 1) {

         /* Decide what to do based on usage */
         if (ctx.uses[instr->definitions[0].tempId()] == 0) {
            /* Only the SCC def is used, try the optimization */
            if (process_shortcircuit_uniform_bool(ctx, instr))
               return;
         } else if (ctx.uses[instr->definitions[0].tempId()] == 1 &&
                    ctx.uses[instr->definitions[1].tempId()] == 0) {
            /* Only the s1 def is used once, mark the definition to try the optimization later */
            set_label(ctx, instr->definitions[0].tempId(), label_try_late_shortcircuit);
         }
      }
   }

   for (auto &op : instr->operands) {
      if (op.isConstant())
         continue;

      auto &op_info = ctx.info[op.tempId()];

      /* See if we can still squeeze out the short-circuit optimization */
      if (op_info.labels.test(label_try_late_shortcircuit) &&
          op_info.block->index == ctx.current_block->index) {
         aco_ptr<Instruction> &shortcircuit_instr = ctx.current_block->instructions[op_info.instr_idx];
         assert(op_info.instr() == shortcircuit_instr.get());

         /* If anything clobbers SCC between the short-circuitable boolean instr and its user, give up */
         if (ctx.last_scc_def != shortcircuit_instr->definitions[1].tempId())
            continue;

         Temp sccdef = shortcircuit_instr->definitions[1].getTemp();

         /* Now it's safe to try the optimization */
         if (process_shortcircuit_uniform_bool(ctx, shortcircuit_instr)) {
            /* Use SCC instead of the SGPR */
            ctx.uses[op.tempId()]--;
            ctx.uses[ctx.last_scc_def]++;
            op.setTemp(sccdef);
            op.setFixed(scc);

            return;
         }
      }
   }

   for (auto &def : instr->definitions) {
      if (def.physReg() == vcc) {
         ctx.last_vcc_def = def.tempId();
      }
      else if (def.physReg() == scc) {
         ctx.last_scc_def = def.tempId();
      }
   }
}

} /* End of empty namespace */

void optimize_postRA(Program* program)
{
   pr_opt_ctx ctx;
   ctx.program = program;
   ctx.uses = dead_code_analysis(program);
   ctx.info.resize(program->peekAllocationId());

   /* Forward pass
    * Goes through each instruction exactly once, and can transform
    * instructions or adjust the use counts of temps.
    */
   for (auto &block : program->blocks) {
      ctx.current_block = &block;
      ctx.current_instr_idx = -1;
      ctx.last_vcc_def = 0;
      ctx.last_scc_def = 0;
      for (aco_ptr<Instruction> &instr : block.instructions)
         process_instruction(ctx, instr);
   }

   /* Cleanup pass
    * Gets rid of instructions which are deleted or no longer have
    * any uses.
    */
   for (auto &block : program->blocks) {
      ctx.current_block = &block;
      std::vector<aco_ptr<Instruction>> sel_instr;
      sel_instr.reserve(block.instructions.size());

      for (aco_ptr<Instruction> &instr : block.instructions) {
         if (instr && !is_dead(ctx.uses, instr.get()))
            sel_instr.emplace_back(std::move(instr));
      }

      block.instructions.swap(sel_instr);
   }
}

} /* End of aco namespace */