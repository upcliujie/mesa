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

   save_reg_writes(ctx, instr);
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