/*
 * Copyright © 2021 Valve Corporation
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

struct pr_opt_ctx
{
   Program *program;
   Block *current_block;
   int current_instr_idx;
   std::vector<uint16_t> uses;
   std::array<int, max_reg_cnt * 4u> instr_idx_by_reg_bytes;

   void reset_block(Block *b)
   {
      current_block = b;
      current_instr_idx = -1;
      std::fill(instr_idx_by_reg_bytes.begin(), instr_idx_by_reg_bytes.end(), 0);
   }
};

void save_reg_writes(pr_opt_ctx &ctx, aco_ptr<Instruction> &instr)
{
   for (const Definition &def : instr->definitions) {
      for (unsigned b = 0; b < def.bytes(); ++b) {
         assert(def.regClass().type() != RegType::sgpr || def.physReg().reg() <= 255);
         assert(def.regClass().type() != RegType::vgpr || def.physReg().reg() >= 256);

         unsigned reg_byte = def.physReg().reg() * 4u + b;
         ctx.instr_idx_by_reg_bytes[reg_byte] = ctx.current_instr_idx;
      }
   }
}

int last_writer_idx(pr_opt_ctx &ctx, PhysReg physReg, RegClass rc)
{
#ifdef NDEBUG
   return ctx.instr_idx_by_reg_bytes[physReg.reg() * 4u];
#else
   /* Sanity check: verify that all of the operand's register bytes are written by the same instruction. */
   int instr_idx = -1;

   for (unsigned b = 0; b < rc.bytes(); ++b) {
      assert(rc.type() != RegType::sgpr || physReg.reg() <= 255);
      assert(rc.type() != RegType::vgpr || physReg.reg() >= 256);

      unsigned reg_byte = physReg.reg() * 4u + b;
      int idx = ctx.instr_idx_by_reg_bytes[reg_byte];

      assert(instr_idx == -1 || idx == instr_idx);
      instr_idx = idx;
   }

   return instr_idx;
#endif
}

int last_writer_idx(pr_opt_ctx &ctx, const Operand &op)
{
   if (op.isConstant() || op.isUndefined())
      return -1;

   return last_writer_idx(ctx, op.physReg(), op.regClass());
}

void try_apply_branch_vcc(pr_opt_ctx &ctx, aco_ptr<Instruction> &instr)
{
   /* We are looking for the following pattern:
    *
    * vcc = ...                      ; last_vcc_wr
    * sX, scc = s_and_bXX vcc, exec  ; op0_instr
    * (...vcc and exec must not be clobbered inbetween...)
    * s_cbranch_XX scc               ; instr
    *
    * If possible, the above is optimized into:
    *
    * vcc = ...                      ; last_vcc_wr
    * s_cbranch_XX vcc               ; instr modified to use vcc
    */

   if (instr->format != Format::PSEUDO_BRANCH ||
       instr->operands.size() == 0 ||
       instr->operands[0].physReg() != scc)
      return;

   int op0_instr_idx = last_writer_idx(ctx, instr->operands[0]);
   int last_vcc_wr_idx = last_writer_idx(ctx, vcc, ctx.program->lane_mask);
   int last_exec_wr_idx = last_writer_idx(ctx, exec, ctx.program->lane_mask);

   if (op0_instr_idx == -1 || last_vcc_wr_idx == -1)
      return;
   if (last_exec_wr_idx > last_vcc_wr_idx)
      return;

   aco_ptr<Instruction> &op0_instr = ctx.current_block->instructions[op0_instr_idx];
   aco_ptr<Instruction> &last_vcc_wr = ctx.current_block->instructions[last_vcc_wr_idx];

   if ((op0_instr->opcode != aco_opcode::s_and_b64 /* wave64 */ &&
        op0_instr->opcode != aco_opcode::s_and_b32 /* wave32 */) ||
       op0_instr->operands[0].physReg() != vcc ||
       op0_instr->operands[1].physReg() != exec ||
       !last_vcc_wr->isVOPC() ||
       last_vcc_wr->definitions[0].tempId() != op0_instr->operands[0].tempId())
      return;

   /* Reduce the uses of the SCC def */
   ctx.uses[instr->operands[0].tempId()]--;
   /* Use VCC instead of SCC in the branch */
   instr->operands[0] = op0_instr->operands[0];
}

void try_optimize_scc_nocompare(pr_opt_ctx &ctx, aco_ptr<Instruction> &instr)
{
   /* We are looking for the following pattern:
    *
    *	s_bfe_u32 s0, s3, 0x40018  ; outputs SGPR and SCC if the SGPR != 0
	 * s_cmp_eq_i32 s0, 0         ; comparison between the SGPR and 0
	 * s_cbranch_scc0 BB3         ; use the result of the comparison, eg. branch or cselect
    *
    * If possible, the above is optimized into:
    *
    *	s_bfe_u32 s0, s3, 0x40018  ; original instruction
	 * s_cbranch_scc1 BB3         ; modified to use SCC directly rather than the SGPR with comparison
    *
    */

   if (!instr->isSALU() && !instr->isBranch())
      return;

   if ((instr->opcode == aco_opcode::s_cmp_eq_u32 || instr->opcode == aco_opcode::s_cmp_eq_i32 ||
        instr->opcode == aco_opcode::s_cmp_lg_u32 || instr->opcode == aco_opcode::s_cmp_lg_i32 ||
        instr->opcode == aco_opcode::s_cmp_eq_u64 ||
        instr->opcode == aco_opcode::s_cmp_lg_u64) &&
       (instr->operands[0].constantEquals(0) || instr->operands[1].constantEquals(0)) &&
       (instr->operands[0].isTemp() || instr->operands[1].isTemp())) {
      /* Make sure the constant is always in operand 1 */
      if (instr->operands[0].isConstant())
         std::swap(instr->operands[0], instr->operands[1]);

      if (ctx.uses[instr->operands[0].tempId()] > 1)
         return;

      int wr_idx = last_writer_idx(ctx, instr->operands[0]);
      int sccwr_idx = last_writer_idx(ctx, scc, s1);
      if (wr_idx < 0 || wr_idx != sccwr_idx)
         return;

      aco_ptr<Instruction> &wr_instr = ctx.current_block->instructions[wr_idx];
      if (!wr_instr->isSALU() || wr_instr->definitions.size() < 2 || wr_instr->definitions[1].physReg() != scc)
         return;

      /* Look for instructions which set SCC := (D != 0) */
      switch (wr_instr->opcode) {
      case aco_opcode::s_bfe_i32:
      case aco_opcode::s_bfe_i64:
      case aco_opcode::s_bfe_u32:
      case aco_opcode::s_bfe_u64:
      case aco_opcode::s_and_b32:
      case aco_opcode::s_and_b64:
      case aco_opcode::s_andn2_b32:
      case aco_opcode::s_andn2_b64:
      case aco_opcode::s_or_b32:
      case aco_opcode::s_or_b64:
      case aco_opcode::s_orn2_b32:
      case aco_opcode::s_orn2_b64:
      case aco_opcode::s_xor_b32:
      case aco_opcode::s_xor_b64:
      case aco_opcode::s_xnor_b32:
      case aco_opcode::s_xnor_b64:
      case aco_opcode::s_nand_b32:
      case aco_opcode::s_nand_b64:
      case aco_opcode::s_lshl_b32:
      case aco_opcode::s_lshl_b64:
      case aco_opcode::s_lshr_b32:
      case aco_opcode::s_lshr_b64:
      case aco_opcode::s_ashr_i32:
      case aco_opcode::s_ashr_i64:
      case aco_opcode::s_absdiff_i32:
         break;
      default:
         return;
      }

      /* Use the SCC def from wr_instr */
      ctx.uses[instr->operands[0].tempId()]--;
      instr->operands[0] = Operand(wr_instr->definitions[1].getTemp(), scc);
      ctx.uses[instr->operands[0].tempId()]++;

      /* Set the opcode and operand to 32-bit */
      instr->operands[1] = Operand(0u);
      instr->opcode = (instr->opcode == aco_opcode::s_cmp_eq_u32 ||
                       instr->opcode == aco_opcode::s_cmp_eq_i32 ||
                       instr->opcode == aco_opcode::s_cmp_eq_u64)
                      ? aco_opcode::s_cmp_eq_u32
                      : aco_opcode::s_cmp_lg_u32;
   } else if ((instr->format == Format::PSEUDO_BRANCH &&
               instr->operands.size() == 1 &&
               instr->operands[0].physReg() == scc) ||
              instr->opcode == aco_opcode::s_cselect_b32) {

      /* For cselect, operand 2 is the SCC condition */
      unsigned scc_op_idx = 0;
      if (instr->opcode == aco_opcode::s_cselect_b32) {
         scc_op_idx = 2;
      }

      int wr_idx = last_writer_idx(ctx, instr->operands[scc_op_idx]);
      if (wr_idx < 0)
         return;

      aco_ptr<Instruction> &wr_instr = ctx.current_block->instructions[wr_idx];
      if ((wr_instr->opcode != aco_opcode::s_cmp_eq_u32 && wr_instr->opcode != aco_opcode::s_cmp_lg_u32) ||
          wr_instr->operands[0].physReg() != scc ||
          !(wr_instr->operands[1].constantEquals(0) || wr_instr->operands[1].constantEquals(1)))
         return;

      if (instr->format == Format::PSEUDO_BRANCH) {
         /* Flip the opcode if necessary */
         if (instr->opcode == aco_opcode::p_cbranch_z && wr_instr->opcode == aco_opcode::s_cmp_eq_u32)
            instr->opcode = aco_opcode::p_cbranch_nz;
         else if (instr->opcode == aco_opcode::p_cbranch_nz && wr_instr->opcode == aco_opcode::s_cmp_eq_u32)
            instr->opcode = aco_opcode::p_cbranch_z;
      } else if (instr->opcode == aco_opcode::s_cselect_b32) {
         /* Swap the operands if necessary */
         if (wr_instr->opcode == aco_opcode::s_cmp_eq_u32)
            std::swap(instr->operands[0], instr->operands[1]);
      } else {
         unreachable("this optimization is only implemented for p_cbranch and s_cselect");
      }

      /* Use the SCC def from the original instruction, not the comparison */
      ctx.uses[instr->operands[scc_op_idx].tempId()]--;
      instr->operands[scc_op_idx] = wr_instr->operands[0];
   }
}

void process_instruction(pr_opt_ctx &ctx, aco_ptr<Instruction> &instr)
{
   ctx.current_instr_idx++;

   try_apply_branch_vcc(ctx, instr);

   try_optimize_scc_nocompare(ctx, instr);

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
      ctx.reset_block(&block);

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