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

void process_instruction(pr_opt_ctx &ctx, aco_ptr<Instruction> &instr)
{
   ctx.current_instr_idx++;

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