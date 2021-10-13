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
 */

#include "aco_builder.h"
#include "aco_ir.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <vector>

namespace aco {
namespace {

constexpr const size_t max_reg_cnt = 512;

struct Idx {
   bool operator==(const Idx& other) const { return block == other.block && instr == other.instr; }
   bool operator!=(const Idx& other) const { return !operator==(other); }

   bool found() const { return block != UINT32_MAX; }

   uint32_t block;
   uint32_t instr;
};

Idx not_written_in_block{UINT32_MAX, 0};
Idx clobbered{UINT32_MAX, 1};
Idx const_or_undef{UINT32_MAX, 2};
Idx written_by_multiple_instrs{UINT32_MAX, 3};

bool
is_instr_after(Idx second, Idx first)
{
   if (first == not_written_in_block && second != not_written_in_block)
      return true;

   if (!first.found() || !second.found())
      return false;

   return second.block > first.block || (second.block == first.block && second.instr > first.instr);
}

struct pr_opt_ctx {
   Program* program;
   Block* current_block;
   uint32_t current_instr_idx;
   std::vector<uint16_t> uses;
   std::vector<std::array<Idx, max_reg_cnt>> instr_idx_by_regs;
   std::vector<std::bitset<max_reg_cnt>> regs_read;

   void reset_block(Block* block)
   {
      current_block = block;
      current_instr_idx = 0;

      if ((block->kind & block_kind_loop_header) || block->linear_preds.empty()) {
         std::fill(instr_idx_by_regs[block->index].begin(), instr_idx_by_regs[block->index].end(),
                   not_written_in_block);

         /* We don't look ahead to see which registers are read, so just assume all of them are. */
         regs_read[block->index].set();
      } else {
         unsigned first_pred = block->linear_preds[0];
         for (unsigned i = 0; i < max_reg_cnt; i++) {
            bool all_same = std::all_of(
               std::next(block->linear_preds.begin()), block->linear_preds.end(),
               [&](unsigned pred)
               { return instr_idx_by_regs[pred][i] == instr_idx_by_regs[first_pred][i]; });

            if (all_same)
               instr_idx_by_regs[block->index][i] = instr_idx_by_regs[first_pred][i];
            else
               instr_idx_by_regs[block->index][i] = not_written_in_block;
         }

         /* Update registers read from predecessor blocks. */
         for (unsigned p = 0; p < block->linear_preds.size(); ++p) {
            regs_read[block->index] |= regs_read[block->linear_preds[p]];
         }
      }
   }

   Instruction* get(Idx idx) { return program->blocks[idx.block].instructions[idx.instr].get(); }
};

void
save_reg_writes(pr_opt_ctx& ctx, aco_ptr<Instruction>& instr)
{
   for (const Definition& def : instr->definitions) {
      assert(def.regClass().type() != RegType::sgpr || def.physReg().reg() <= 255);
      assert(def.regClass().type() != RegType::vgpr || def.physReg().reg() >= 256);

      unsigned dw_size = DIV_ROUND_UP(def.bytes(), 4u);
      unsigned r = def.physReg().reg();
      Idx idx{ctx.current_block->index, ctx.current_instr_idx};

      if (def.regClass().is_subdword())
         idx = clobbered;

      assert((r + dw_size) <= max_reg_cnt);
      assert(def.size() == dw_size || def.regClass().is_subdword());
      std::fill(ctx.instr_idx_by_regs[ctx.current_block->index].begin() + r,
                ctx.instr_idx_by_regs[ctx.current_block->index].begin() + r + dw_size, idx);

      /* The registers just written are not yet read after the current instruction. */
      for (unsigned dw = 0; dw < dw_size; ++dw)
         ctx.regs_read[ctx.current_block->index].reset(r + dw);
   }
}

void
save_reg_reads(pr_opt_ctx& ctx, aco_ptr<Instruction>& instr)
{
   for (const Operand& op : instr->operands) {
      if (op.isConstant() || op.isUndefined())
         continue;

      assert(op.regClass().type() != RegType::sgpr || op.physReg().reg() <= 255);
      assert(op.regClass().type() != RegType::vgpr || op.physReg().reg() >= 256);

      unsigned dw_size = DIV_ROUND_UP(op.bytes(), 4u);
      unsigned r = op.physReg().reg();

      for (unsigned dw = 0; dw < dw_size; ++dw)
         ctx.regs_read[ctx.current_block->index].set(r + dw);
   }
}

bool
test_reg_read(const pr_opt_ctx& ctx, unsigned reg, unsigned dw_size)
{
   for (unsigned dw = 0; dw < dw_size; ++dw)
      if (ctx.regs_read[ctx.current_block->index].test(reg + dw))
         return true;

   return false;
}

template<typename T>
bool
test_reg_read(const pr_opt_ctx& ctx, const T& t)
{
   return test_reg_read(ctx, t.physReg().reg(), t.size());
}

Idx
last_writer_idx(pr_opt_ctx& ctx, PhysReg physReg, RegClass rc)
{
   /* Verify that all of the operand's registers are written by the same instruction. */
   assert(physReg.reg() < max_reg_cnt);
   Idx instr_idx = ctx.instr_idx_by_regs[ctx.current_block->index][physReg.reg()];
   unsigned dw_size = DIV_ROUND_UP(rc.bytes(), 4u);
   unsigned r = physReg.reg();
   bool all_same =
      std::all_of(ctx.instr_idx_by_regs[ctx.current_block->index].begin() + r,
                  ctx.instr_idx_by_regs[ctx.current_block->index].begin() + r + dw_size,
                  [instr_idx](Idx i) { return i == instr_idx; });

   return all_same ? instr_idx : written_by_multiple_instrs;
}

Idx
last_writer_idx(pr_opt_ctx& ctx, const Operand& op)
{
   if (op.isConstant() || op.isUndefined())
      return const_or_undef;

   assert(op.physReg().reg() < max_reg_cnt);
   Idx instr_idx = ctx.instr_idx_by_regs[ctx.current_block->index][op.physReg().reg()];

#ifndef NDEBUG
   /* Debug mode:  */
   instr_idx = last_writer_idx(ctx, op.physReg(), op.regClass());
   assert(instr_idx != written_by_multiple_instrs);
#endif

   return instr_idx;
}

void
try_apply_branch_vcc(pr_opt_ctx& ctx, aco_ptr<Instruction>& instr)
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

   /* Don't try to optimize this on GFX6-7 because SMEM may corrupt the vccz bit. */
   if (ctx.program->chip_class < GFX8)
      return;

   if (instr->format != Format::PSEUDO_BRANCH || instr->operands.size() == 0 ||
       instr->operands[0].physReg() != scc)
      return;

   Idx op0_instr_idx = last_writer_idx(ctx, instr->operands[0]);
   Idx last_vcc_wr_idx = last_writer_idx(ctx, vcc, ctx.program->lane_mask);
   Idx last_exec_wr_idx = last_writer_idx(ctx, exec, ctx.program->lane_mask);

   /* We need to make sure:
    * - the operand register used by the branch, and VCC were both written in the current block
    * - VCC was NOT written after the operand register
    * - EXEC is sane and was NOT written after the operand register
    */
   if (!op0_instr_idx.found() || !last_vcc_wr_idx.found() ||
       !is_instr_after(last_vcc_wr_idx, last_exec_wr_idx) ||
       !is_instr_after(op0_instr_idx, last_vcc_wr_idx))
      return;

   Instruction* op0_instr = ctx.get(op0_instr_idx);
   Instruction* last_vcc_wr = ctx.get(last_vcc_wr_idx);

   if ((op0_instr->opcode != aco_opcode::s_and_b64 /* wave64 */ &&
        op0_instr->opcode != aco_opcode::s_and_b32 /* wave32 */) ||
       op0_instr->operands[0].physReg() != vcc || op0_instr->operands[1].physReg() != exec ||
       !last_vcc_wr->isVOPC())
      return;

   assert(last_vcc_wr->definitions[0].tempId() == op0_instr->operands[0].tempId());

   /* Reduce the uses of the SCC def */
   ctx.uses[instr->operands[0].tempId()]--;
   /* Use VCC instead of SCC in the branch */
   instr->operands[0] = op0_instr->operands[0];
}

void
try_optimize_scc_nocompare(pr_opt_ctx& ctx, aco_ptr<Instruction>& instr)
{
   /* We are looking for the following pattern:
    *
    * s_bfe_u32 s0, s3, 0x40018  ; outputs SGPR and SCC if the SGPR != 0
    * s_cmp_eq_i32 s0, 0         ; comparison between the SGPR and 0
    * s_cbranch_scc0 BB3         ; use the result of the comparison, eg. branch or cselect
    *
    * If possible, the above is optimized into:
    *
    * s_bfe_u32 s0, s3, 0x40018  ; original instruction
    * s_cbranch_scc1 BB3         ; modified to use SCC directly rather than the SGPR with comparison
    *
    */

   if (!instr->isSALU() && !instr->isBranch())
      return;

   if (instr->isSOPC() &&
       (instr->opcode == aco_opcode::s_cmp_eq_u32 || instr->opcode == aco_opcode::s_cmp_eq_i32 ||
        instr->opcode == aco_opcode::s_cmp_lg_u32 || instr->opcode == aco_opcode::s_cmp_lg_i32 ||
        instr->opcode == aco_opcode::s_cmp_eq_u64 || instr->opcode == aco_opcode::s_cmp_lg_u64) &&
       (instr->operands[0].constantEquals(0) || instr->operands[1].constantEquals(0)) &&
       (instr->operands[0].isTemp() || instr->operands[1].isTemp())) {
      /* Make sure the constant is always in operand 1 */
      if (instr->operands[0].isConstant())
         std::swap(instr->operands[0], instr->operands[1]);

      if (ctx.uses[instr->operands[0].tempId()] > 1)
         return;

      /* Make sure both SCC and Operand 0 are written by the same instruction. */
      Idx wr_idx = last_writer_idx(ctx, instr->operands[0]);
      Idx sccwr_idx = last_writer_idx(ctx, scc, s1);
      if (!wr_idx.found() || wr_idx != sccwr_idx)
         return;

      Instruction* wr_instr = ctx.get(wr_idx);
      if (!wr_instr->isSALU() || wr_instr->definitions.size() < 2 ||
          wr_instr->definitions[1].physReg() != scc)
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
      case aco_opcode::s_not_b32:
      case aco_opcode::s_not_b64:
      case aco_opcode::s_nor_b32:
      case aco_opcode::s_nor_b64:
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
      case aco_opcode::s_abs_i32:
      case aco_opcode::s_absdiff_i32: break;
      default: return;
      }

      /* Use the SCC def from wr_instr */
      ctx.uses[instr->operands[0].tempId()]--;
      instr->operands[0] = Operand(wr_instr->definitions[1].getTemp(), scc);
      ctx.uses[instr->operands[0].tempId()]++;

      /* Set the opcode and operand to 32-bit */
      instr->operands[1] = Operand::zero();
      instr->opcode =
         (instr->opcode == aco_opcode::s_cmp_eq_u32 || instr->opcode == aco_opcode::s_cmp_eq_i32 ||
          instr->opcode == aco_opcode::s_cmp_eq_u64)
            ? aco_opcode::s_cmp_eq_u32
            : aco_opcode::s_cmp_lg_u32;
   } else if ((instr->format == Format::PSEUDO_BRANCH && instr->operands.size() == 1 &&
               instr->operands[0].physReg() == scc) ||
              instr->opcode == aco_opcode::s_cselect_b32) {

      /* For cselect, operand 2 is the SCC condition */
      unsigned scc_op_idx = 0;
      if (instr->opcode == aco_opcode::s_cselect_b32) {
         scc_op_idx = 2;
      }

      Idx wr_idx = last_writer_idx(ctx, instr->operands[scc_op_idx]);
      if (!wr_idx.found())
         return;

      Instruction* wr_instr = ctx.get(wr_idx);

      /* Check if we found the pattern above. */
      if (wr_instr->opcode != aco_opcode::s_cmp_eq_u32 &&
          wr_instr->opcode != aco_opcode::s_cmp_lg_u32)
         return;
      if (wr_instr->operands[0].physReg() != scc)
         return;
      if (!wr_instr->operands[1].constantEquals(0))
         return;

      /* The optimization can be unsafe when there are other users. */
      if (ctx.uses[instr->operands[scc_op_idx].tempId()] > 1)
         return;

      if (wr_instr->opcode == aco_opcode::s_cmp_eq_u32) {
         /* Flip the meaning of the instruction to correctly use the SCC. */
         if (instr->format == Format::PSEUDO_BRANCH)
            instr->opcode = instr->opcode == aco_opcode::p_cbranch_z ? aco_opcode::p_cbranch_nz
                                                                     : aco_opcode::p_cbranch_z;
         else if (instr->opcode == aco_opcode::s_cselect_b32)
            std::swap(instr->operands[0], instr->operands[1]);
         else
            unreachable(
               "scc_nocompare optimization is only implemented for p_cbranch and s_cselect");
      }

      /* Use the SCC def from the original instruction, not the comparison */
      ctx.uses[instr->operands[scc_op_idx].tempId()]--;
      instr->operands[scc_op_idx] = wr_instr->operands[0];
   }
}

void
try_combine_dpp(pr_opt_ctx& ctx, aco_ptr<Instruction>& instr)
{
   if (!instr->isVALU() || instr->isDPP() || !can_use_DPP(instr, false))
      return;

   for (unsigned i = 0; i < MIN2(2, instr->operands.size()); i++) {
      Idx op_instr_idx = last_writer_idx(ctx, instr->operands[i]);
      if (!op_instr_idx.found())
         continue;

      Instruction* mov = ctx.get(op_instr_idx);
      if (mov->opcode != aco_opcode::v_mov_b32 || !mov->isDPP())
         continue;

      /* If we aren't going to remove the v_mov_b32, we have to ensure that it doesn't overwrite
       * it's own operand before we use it.
       */
      if (mov->definitions[0].physReg() == mov->operands[0].physReg() &&
          (!mov->definitions[0].tempId() || ctx.uses[mov->definitions[0].tempId()] > 1))
         continue;

      Idx mov_src_idx = last_writer_idx(ctx, mov->operands[0]);
      if (is_instr_after(mov_src_idx, op_instr_idx))
         continue;

      if (i && !can_swap_operands(instr, &instr->opcode))
         continue;

      /* anything else doesn't make sense in SSA */
      assert(mov->dpp().row_mask == 0xf && mov->dpp().bank_mask == 0xf);

      if (--ctx.uses[mov->definitions[0].tempId()])
         ctx.uses[mov->operands[0].tempId()]++;

      convert_to_DPP(instr);

      DPP_instruction* dpp = &instr->dpp();
      if (i) {
         std::swap(dpp->operands[0], dpp->operands[1]);
         std::swap(dpp->neg[0], dpp->neg[1]);
         std::swap(dpp->abs[0], dpp->abs[1]);
      }
      dpp->operands[0] = mov->operands[0];
      dpp->dpp_ctrl = mov->dpp().dpp_ctrl;
      dpp->bound_ctrl = true;
      dpp->neg[0] ^= mov->dpp().neg[0] && !dpp->abs[0];
      dpp->abs[0] |= mov->dpp().abs[0];
      return;
   }
}

void
try_recolor_copy(pr_opt_ctx& ctx, aco_ptr<Instruction>& instr)
{
   /* Post-RA register recoloring (backwards copy propagation).
    * The main motivation of this optimization is to write exec without copying an SGPR.
    * It may also be useful for eliminating some unlucky RA shuffle instructions.
    *
    * We're looking for the following pattern:
    *
    * sN = ...                  ; can be any instruction writing a non-special SGPR
    * (...sM must not be read or written here...)
    * sM = p_parallelcopy sN    ; copies the aforementioned SGPR into another SGPR
    *
    * If possible, the above is optimized into:
    *
    * sM = ...                  ; instruction is altered to write the other SGPR instead
    *
    */

   if (instr->opcode != aco_opcode::p_parallelcopy)
      return;

   assert(instr->operands.size() == instr->definitions.size());

   /* Record the registers that we copy from and to. */
   std::bitset<max_reg_cnt> regs_copied_from;
   std::bitset<max_reg_cnt> regs_copied_to;
   for (const Operand& op : instr->operands)
      for (unsigned dw = 0; dw < op.size(); ++dw)
         regs_copied_from.set(op.physReg() + dw, true);
   for (const Definition& def : instr->definitions)
      for (unsigned dw = 0; dw < def.size(); ++dw)
         regs_copied_to.set(def.physReg() + dw, true);

   for (unsigned i = 0; i < instr->definitions.size(); ++i) {
      Definition& def = instr->definitions[i];
      Operand& op = instr->operands[i];
      assert(def.bytes() == op.bytes());

      /* Make sure not to disturb shuffles. Skip recoloring when
       * the same register is copied both to and from by the current instruction.
       * Recoloring such a copy could eg. mess up a swap.
       */
      bool disturbs_shuffle = false;
      for (unsigned dw = 0; dw < op.size(); ++dw) {
         if (regs_copied_to.test(op.physReg() + dw) ||
             regs_copied_from.test(def.physReg() + dw)) {
            disturbs_shuffle = true;
            break;
         }
      }
      if (disturbs_shuffle)
         continue;

      /* Only propagate if the current parallelcopy is the only use. */
      if (!op.isTemp() || ctx.uses[op.tempId()] > 1)
         continue;

      /* Don't mess with special registers. */
      if (op.physReg() > 107 && op.physReg() < 256 && op.physReg() != exec)
         continue;
      if (op.regClass() != def.regClass())
         continue;
      if (def.physReg() == scc)
         continue;

      /* Make sure we can find the instruction that wrote the current operand's register. */
      Idx op_wr_idx = last_writer_idx(ctx, op);
      if (!op_wr_idx.found())
         continue;
      /* This is currently not safe accross blocks (it may break loops). */
      if (op_wr_idx.block != ctx.current_block->index)
         continue;

      /* Make sure the definition's register isn't used between the operand's instruction and the
       * copy. */
      if (test_reg_read(ctx, def) || test_reg_read(ctx, op))
         continue;

      /* Make sure the definition register isn't written after op_wr_instr. */
      Idx def_wr_idx = last_writer_idx(ctx, def.physReg(), def.regClass());
      if (def_wr_idx.found() && def_wr_idx.block == op_wr_idx.block &&
          def_wr_idx.instr > op_wr_idx.instr)
         continue;

      Instruction* op_wr_instr = ctx.get(op_wr_idx);

      /* Only propagate to definitions of certain instruction types. */
      if (is_phi(op_wr_instr))
         continue; /* Don't mess with phis. */
      if (op_wr_instr->opcode == aco_opcode::p_startpgm)
         continue; /* p_startpgm definitions are shader arguments, we can't change those. */
      if (op_wr_instr->isSALU() && op_wr_instr->definitions.size() == 3 &&
          op_wr_instr->definitions[2].physReg() == op.physReg())
         continue; /* We can't change the 3rd definition of s_and/or_saveexec and similar. */
      if (op_wr_instr->isSOPC() || op_wr_instr->isSOPP() || op_wr_instr->isSOPK())
         continue; /* These don't benefit. */

      /* TODO: Handle VOPC instructions.
       * For example, if VOPC result is copied to exec, convert to v_cmpx, etc.
       */
      if (op_wr_instr->isVOPC())
         continue;

      /* At the operand's writer, find the definition that writes the operand. */
      for (unsigned j = 0; j < op_wr_instr->definitions.size(); ++j) {
         if (op_wr_instr->definitions[j].physReg() != op.physReg())
            continue;

         /* If the definition of the writer doesn't cover the operand entirely,
          * don't do anything.
          */
         if (op_wr_instr->definitions[j].bytes() != op.bytes())
            break;

         assert(ctx.uses[op_wr_instr->definitions[j].tempId()] == 1);

         /* Move the copy's definition up to the instruction whose def being copied. */
         op_wr_instr->definitions[j] = def;

         /* Compact the copy's operands and definitions and remove the propagated ones. */
         std::copy(std::next(instr->definitions.begin(), i + 1), instr->definitions.end(),
                   std::next(instr->definitions.begin(), i));
         instr->definitions.pop_back();
         std::copy(std::next(instr->operands.begin(), i + 1), instr->operands.end(),
                   std::next(instr->operands.begin(), i));
         instr->operands.pop_back();
         i--;
         break;
      }
   }

   /* If nothing is being copied anymore, delete the copy instruction */
   if (instr->definitions.size() == 0)
      instr.reset();
}

void
process_instruction(pr_opt_ctx& ctx, aco_ptr<Instruction>& instr)
{
   try_apply_branch_vcc(ctx, instr);

   try_optimize_scc_nocompare(ctx, instr);

   try_combine_dpp(ctx, instr);

   try_recolor_copy(ctx, instr);

   if (instr) {
      save_reg_reads(ctx, instr);
      save_reg_writes(ctx, instr);
   }

   ctx.current_instr_idx++;
}

} // namespace

void
optimize_postRA(Program* program)
{
   pr_opt_ctx ctx;
   ctx.program = program;
   ctx.uses = dead_code_analysis(program);
   ctx.instr_idx_by_regs.resize(program->blocks.size());
   ctx.regs_read.resize(program->blocks.size());

   /* Forward pass
    * Goes through each instruction exactly once, and can transform
    * instructions or adjust the use counts of temps.
    */
   for (auto& block : program->blocks) {
      ctx.reset_block(&block);

      for (aco_ptr<Instruction>& instr : block.instructions)
         process_instruction(ctx, instr);
   }

   /* Cleanup pass
    * Gets rid of instructions which are manually deleted or
    * no longer have any uses.
    */
   for (auto& block : program->blocks) {
      auto new_end = std::remove_if(block.instructions.begin(), block.instructions.end(),
                                    [&ctx](const aco_ptr<Instruction>& instr)
                                    { return !instr || is_dead(ctx.uses, instr.get()); });
      block.instructions.resize(new_end - block.instructions.begin());
   }
}

} // namespace aco
