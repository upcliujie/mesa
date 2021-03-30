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
#include <array>

namespace aco {
namespace {

constexpr const size_t max_reg_cnt = 512;
struct pr_opt_reg_info
{
   Block *block = nullptr;
   int instr_idx = -1;
};

struct pr_opt_ctx
{
   Program *program;
   Block *current_block;
   int current_instr_idx;
   std::vector<uint16_t> uses;
   std::array<pr_opt_reg_info, max_reg_cnt> info;
};

void save_reg_writes(pr_opt_ctx &ctx, aco_ptr<Instruction> &instr)
{
   for (const Definition &def : instr->definitions) {
      for (unsigned k = 0; k < def.size(); ++k) {
         unsigned reg = def.physReg().reg() + k;
         assert(def.regClass().type() != RegType::sgpr || reg <= 255);
         assert(def.regClass().type() != RegType::vgpr || reg >= 256);

         pr_opt_reg_info &info = ctx.info[reg];
         info.block = ctx.current_block;
         info.instr_idx = ctx.current_instr_idx;
      }
   }
}

int last_writer_idx(pr_opt_ctx &ctx, PhysReg physReg, RegClass rc)
{
   int instr_idx = -1;

   for (unsigned k = 0; k < rc.size(); ++k) {
      unsigned reg = physReg.reg() + k;
      assert(rc.type() != RegType::sgpr || reg <= 255);
      assert(rc.type() != RegType::vgpr || reg >= 256);

      const pr_opt_reg_info &info = ctx.info[reg];
      if (info.block != ctx.current_block)
         return -1; /* Register was written in a different block */
      if (instr_idx != -1 && info.instr_idx != instr_idx)
         return -1; /* Not all of the operand's registers were written by the same instruction */
      else if (instr_idx == -1)
         instr_idx = info.instr_idx;
   }

   return instr_idx;
}

int last_writer_idx(pr_opt_ctx &ctx, const Operand &op)
{
   if (op.isConstant() || op.isUndefined())
      return -1;

   return last_writer_idx(ctx, op.physReg(), op.regClass());
}

void try_apply_branch_vcc(pr_opt_ctx &ctx, aco_ptr<Instruction> &instr)
{
   /* Check if we have a branch that uses SCC */
   if (instr->format != Format::PSEUDO_BRANCH ||
       instr->operands.size() == 0 ||
       instr->operands[0].physReg() != scc)
      return;

   /* We are looking for the following pattern:
    *
    * vcc = ...                      ; last_vcc_wr
    * sX, scc = s_and_bXX vcc, exec  ; op0_instr
    * (...vcc must not be clobbered inbetween...)
    * s_cbranch_XX scc               ; instr
    */

   int op0_instr_idx = last_writer_idx(ctx, instr->operands[0]);
   int last_vcc_wr_idx = last_writer_idx(ctx, vcc, ctx.program->lane_mask);

   if (op0_instr_idx == -1 || last_vcc_wr_idx == -1)
      return;

   aco_ptr<Instruction> &op0_instr = ctx.current_block->instructions[op0_instr_idx];
   aco_ptr<Instruction> &last_vcc_wr = ctx.current_block->instructions[last_vcc_wr_idx];

   if ((op0_instr->opcode != aco_opcode::s_and_b64 /* wave64 */ &&
        op0_instr->opcode != aco_opcode::s_and_b32 /* wave32 */) ||
       op0_instr->operands[0].physReg() != vcc ||
       op0_instr->operands[1].physReg() != exec ||
       last_vcc_wr->definitions[0].tempId() != op0_instr->operands[0].tempId())
      return;

   /* Reduce the uses of the SCC def */
   ctx.uses[instr->operands[0].tempId()]--;
   /* Use VCC instead of SCC in the branch */
   instr->operands[0] = op0_instr->operands[0];
}

void try_shortcircuit_uniform_bool(pr_opt_ctx &ctx, aco_ptr<Instruction> &instr)
{
   /*
    * Eliminates an SCC copy by applying a "short-circuit" to uniform boolean logic.
    * In order to do the short-circuit transform, the pattern should look like this:
    *
    * sN = p_parallelcopy scc        ; used only once
    * scc = s_cmp_xxx A, B           ; used only once
    * sM, scc = s_and/or_b32 sN, scc
    *
    * The p_parallelcopy is replaced by an s_cselect and s_cmp_xxx adjusted
    * to use the result from the cselect, and s_and is eliminated:
    *
    * sN = s_cselect X, Y, scc  ; X and Y depend on the opcodes
    * scc = s_cmp_xxx sN, B     ; scc is equivalent to what was previously produced by s_and/or_b32
    *
    * When sM is used, a parallelcopy is inserted which copies scc to it,
    * in the hopes that we might be able to delete it later.
    */

   if ((instr->opcode != aco_opcode::s_and_b32 &&
        instr->opcode != aco_opcode::s_or_b32) ||
       (instr->operands[0].physReg() != scc &&
        instr->operands[1].physReg() != scc))
      return;

   /* Move SCC to the second operand to reduce possible permutations */
   if (!instr->operands[0].isConstant() && instr->operands[0].physReg() == scc)
      std::swap(instr->operands[0], instr->operands[1]);

   int op0_instr_idx = last_writer_idx(ctx, instr->operands[0]);
   int op1_instr_idx = last_writer_idx(ctx, instr->operands[1]);

   /* Both operands must have a defining instr, and operand 1 must come later */
   if (op0_instr_idx == -1 || op1_instr_idx <= op0_instr_idx)
      return;

   aco_ptr<Instruction> &op0_scc2sgpr = ctx.current_block->instructions[op0_instr_idx];
   aco_ptr<Instruction> &op1_scc = ctx.current_block->instructions[op1_instr_idx];

   if (op0_scc2sgpr->opcode != aco_opcode::p_parallelcopy ||
       op0_scc2sgpr->operands[0].physReg() != scc)
      return;

   if (op1_scc->format == Format::SOPC) {
      /* Make sure if there is a constant, it's always in the 2nd operand */
      if (op1_scc->operands[0].isConstant()) {
         /* Flip the opcode so that it has the same meaning with the constant in the 2nd operand */
         aco_opcode op = aco_opcode::num_opcodes;
         /* NOTE: s_cmp_le and s_cmp_gt are not used in this manner, so those are not covered here */
         switch (op1_scc->opcode) {
         case aco_opcode::s_cmp_eq_u32:
         case aco_opcode::s_cmp_eq_i32:
         case aco_opcode::s_cmp_lg_u32:
         case aco_opcode::s_cmp_lg_i32:
            op = op1_scc->opcode;
            break;
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
         default:
            return;
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
         return;

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
         return;

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
         return;

      if (instr->opcode != aco_opcode::s_and_b32 &&
          (!op1_scc->operands[1].isConstant() ||
           op1_scc->operands[1].constantEquals(const_op)))
         return;

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
         return;

      if (instr->opcode != aco_opcode::s_or_b32 &&
          (!op1_scc->operands[1].isConstant() ||
           op1_scc->operands[1].constantEquals(const_op)))
         return;

      csel_op0 = instr->opcode == aco_opcode::s_or_b32
               ? op1_scc->operands[1]
               : op1_scc->operands[0];
      csel_op1 = instr->opcode == aco_opcode::s_or_b32
               ? op1_scc->operands[0]
               : Operand(const_op);
   } else {
      return;
   }

   /* Create a conditional select which will choose the 1st operand of the 2nd SOPC instruction */
   SOP2_instruction *csel = create_instruction<SOP2_instruction>(aco_opcode::s_cselect_b32, Format::SOP2, 3, 1);
   csel->definitions[0] = op0_scc2sgpr->definitions[0];
   csel->operands[0] = csel_op0;
   csel->operands[1] = csel_op1;
   csel->operands[2] = op0_scc2sgpr->operands[0];

   /* Replace SCC copy */
   op0_scc2sgpr.reset(csel);

   /* Edit the scc producer to use the definition from the cselect, and replace the current instr */
   op1_scc->definitions[0] = instr->definitions[1];
   op1_scc->operands[0] = Operand(csel->definitions[0].getTemp(), csel->definitions[0].physReg());

   if (ctx.uses[instr->definitions[0].tempId()] != 0) {
      /* Insert a new SCC copy, which can be potentially still deleted later */
      ctx.uses[instr->definitions[1].tempId()]++;
      Pseudo_instruction *copy = create_instruction<Pseudo_instruction>(aco_opcode::p_parallelcopy, Format::PSEUDO, 1, 1);
      copy->definitions[0] = instr->definitions[0];
      copy->operands[0] = Operand(instr->definitions[1].getTemp(), scc);
      instr.reset(copy);
   } else {
      /* Delete the current instr */
      instr.reset();
   }
}

void process_instruction(pr_opt_ctx &ctx, aco_ptr<Instruction> &instr)
{
   ctx.current_instr_idx++;

   for (Operand &op : instr->operands) {
      int wr_idx = last_writer_idx(ctx, op);
      if (wr_idx < 0)
         continue;

      /* Find which instruction writes the register read by the current operand */
      aco_ptr<Instruction> &wr_instr = ctx.current_block->instructions[wr_idx];
      /* If the operand's register is written by a parallelcopy, see if we can get rid of it */
      if (wr_instr->opcode == aco_opcode::p_parallelcopy &&
          wr_instr->operands[0].regClass() == wr_instr->definitions[0].regClass()) {
         /* Find the index of the instruction that writes what is copied */
         int copied_wr_idx = last_writer_idx(ctx, wr_instr->operands[0]);
         if (copied_wr_idx != -1 && copied_wr_idx < wr_idx) {
            /* The register isn't overwritten between the copy and the current instr,
             * so let's use that directly instead. This may let us delete the copy.
             */
            ctx.uses[op.tempId()]--;
            op = wr_instr->operands[0];
            ctx.uses[op.tempId()]++;
         }
      }
   }

   try_apply_branch_vcc(ctx, instr);

   try_shortcircuit_uniform_bool(ctx, instr);

   if (instr)
      save_reg_writes(ctx, instr);
}

} /* End of empty namespace */

void optimize_postRA(Program* program)
{
   pr_opt_ctx ctx;
   ctx.program = program;
   ctx.uses = dead_code_analysis(program);

   /* Forward pass
    * Goes through each instruction exactly once, and can transform
    * instructions or adjust the use counts of temps.
    */
   for (auto &block : program->blocks) {
      ctx.current_block = &block;
      ctx.current_instr_idx = -1;
      for (aco_ptr<Instruction> &instr : block.instructions)
         process_instruction(ctx, instr);
   }

   /* Cleanup pass
    * Gets rid of instructions which are manually deleted or
    * no longer have any uses.
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