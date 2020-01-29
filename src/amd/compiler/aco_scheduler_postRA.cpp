/*
 * Copyright © 2018 Valve Corporation
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
 *    Daniel Schürmann (daniel.schuermann@campus.tu-berlin.de)
 *
 */

#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>

#include "aco_ir.h"

namespace aco {

namespace {

/* Maximum addressable register */
constexpr size_t max_reg_cnt = 512;

struct Node
{
   int index;
   int priority;
   std::unordered_set<Node *> successors;
   std::unordered_set<Node *> predecessors;
   bool scheduled = false;

   Node(int i) : index(i) {}

   bool operator ==(const struct Node &other) const
   {
      return index == other.index;
   }

   bool operator <(const struct Node &other) const
   {
      return index < other.index;
   }
};

struct sched_ctx
{
   Block *block;
   std::vector<aco_ptr<Instruction>> new_instructions;
   std::vector<Node> nodes;

   std::set<Node *> candidates;
   Node *writes[max_reg_cnt] = {0};
   std::unordered_set<Node *> writeless_reads[max_reg_cnt];

   /* here we can maintain information about the functional units */
   sched_ctx(Block *b) : block(b)
   {
      nodes.reserve(b->instructions.size());
   }

   void barrier()
   {
      assert(candidates.empty());

      /* Clear read/write info */
      for (unsigned i = 0; i < max_reg_cnt; ++i) {
         writes[i] = nullptr;
         writeless_reads[i].clear();
      }
   }
};

bool is_unschedulable(const Instruction *instr)
{
   if (instr->format == Format::SOPP) {
      switch (instr->opcode) {
      case aco_opcode::s_sendmsg:
         return false;
      default:
         return true;
      }
   } else if (instr->format == Format::SOPK) {
      switch (instr->opcode) {
      case aco_opcode::s_call_b64:
      case aco_opcode::s_subvector_loop_begin:
      case aco_opcode::s_subvector_loop_end:
      case aco_opcode::s_waitcnt:
      case aco_opcode::s_waitcnt_vscnt:
      case aco_opcode::s_waitcnt_vmcnt:
      case aco_opcode::s_waitcnt_expcnt:
      case aco_opcode::s_waitcnt_lgkmcnt:
      case aco_opcode::s_waitcnt_depctr:
         return true;
      default:
         return false;
      }
   } else if (instr->format == Format::SMEM) {
      switch (instr->opcode) {
      case aco_opcode::s_dcache_wb:
      case aco_opcode::s_dcache_wb_vol:
         return true;
      default:
         return false;
      }
   }

   return false;
}

bool reads_exec_implicitly(const Instruction *instr)
{
   if (instr->isSALU()) {
      switch (instr->opcode) {
         case aco_opcode::s_or_saveexec_b64:
         case aco_opcode::s_and_saveexec_b64:
         case aco_opcode::s_xor_saveexec_b64:
         case aco_opcode::s_andn2_saveexec_b64:
         case aco_opcode::s_orn2_saveexec_b64:
         case aco_opcode::s_nand_saveexec_b64:
         case aco_opcode::s_nor_saveexec_b64:
         case aco_opcode::s_xnor_saveexec_b64:
            return true;
         default:
            return false;
      }
   }

   if (instr->opcode == aco_opcode::v_readlane_b32 ||
       instr->opcode == aco_opcode::v_readfirstlane_b32 ||
       instr->opcode == aco_opcode::v_writelane_b32)
      return false;

   return true;
}

bool writes_exec_implicitly(const Instruction *instr)
{
   /* TODO v_cmpx_* */
   return false;
}

bool is_new_candidate(const Node *node)
{
   for (Node *pre : node->predecessors) {
      if (!pre->scheduled)
         return false;
   }

   return true;
}

Node* select_candidate(sched_ctx &ctx)
{
   std::set<Node *>::iterator it = ctx.candidates.begin();
   /* TODO: choose candidate based on priority */
   Node *next = *it;
   ctx.candidates.erase(it);
   assert(!next->scheduled);
   next->scheduled = true;

   /* add successors to list of potential candidates */
   for (Node *n : next->successors) {
      assert(n != next);
      assert(!n->scheduled);

      if (is_new_candidate(n))
         ctx.candidates.insert(n);
   }

   return next;
}

bool add_predecessor(sched_ctx &ctx, Node *node, Node *predecessor)
{
   assert(predecessor != node);

   if (predecessor->scheduled)
      return false;

   if (predecessor->successors.insert(node).second)
      node->predecessors.insert(predecessor);

   return true;
}

bool handle_read(sched_ctx &ctx, Node *node, unsigned reg)
{
   assert(reg < max_reg_cnt);
   bool is_candidate = true;
   Node *write = ctx.writes[reg];

   if (write && !write->scheduled) {
      is_candidate = false;
      add_predecessor(ctx, node, write);
   } else if (!write) {
      /* This register isn't written by any instruction, but the current one reads it. */
      ctx.writeless_reads[reg].insert(node);
   }

   return is_candidate;
}

bool handle_write(sched_ctx &ctx, Node *node, unsigned reg)
{
   assert(reg < max_reg_cnt);
   bool is_candidate = true;
   Node *write = ctx.writes[reg];

   if (write && !write->scheduled) {
      is_candidate = false;

      /* add all uses of previous write to predecessors */
      for (Node* use : write->successors) {
         if (use == node)
            continue;

         add_predecessor(ctx, node, use);
      }

      /* add previous write as predecessor */
      add_predecessor(ctx, node, write);
   } else if (!write && ctx.writeless_reads[reg].size()) {
      /* Add writeless reads as predecessors */
      for (Node *read : ctx.writeless_reads[reg]) {
         if (read == node || read->scheduled)
            continue;

         add_predecessor(ctx, node, read);

         is_candidate = false;
      }
      ctx.writeless_reads[reg].clear();
   }

   ctx.writes[reg] = node;
   return is_candidate;
}

void add_to_dag(sched_ctx &ctx, const Instruction *instr, unsigned index)
{
   assert(!is_unschedulable(instr));
   ctx.nodes.emplace_back(index);
   Node* node = &ctx.nodes.back();
   bool is_candidate = true;

   /* Read after Write */
   for (unsigned i = 0; i < instr->operands.size(); i++) {
      if (instr->operands[i].isConstant())
         continue;

      unsigned reg = instr->operands[i].physReg().reg();
      if (instr->operands[i].regClass().type() == RegType::vgpr && reg < 256)
         reg += 256;

      assert(instr->operands[i].regClass().type() != RegType::sgpr || reg <= 255);
      assert(instr->operands[i].regClass().type() != RegType::vgpr || reg >= 256);

      for (unsigned k = 0; k < instr->operands[i].size() && (reg + k) < max_reg_cnt; k++) {
         if (!handle_read(ctx, node, reg + k))
            is_candidate = false;
      }
   }

   if (reads_exec_implicitly(instr)) {
      for (unsigned reg = exec_lo.reg(); reg <= exec_hi.reg(); reg++) {
         if (!handle_read(ctx, node, reg))
            is_candidate = false;
      }
   }

   /* Write after Write/Read */
   for (unsigned i = 0; i < instr->definitions.size(); i++) {
      unsigned reg = instr->definitions[i].physReg().reg();
      if (instr->definitions[i].regClass().type() == RegType::vgpr && reg < 256)
         reg += 256;

      assert(instr->definitions[i].regClass().type() != RegType::sgpr || reg <= 255);
      assert(instr->definitions[i].regClass().type() != RegType::vgpr || reg >= 256);

      for (unsigned k = 0; k < instr->definitions[i].size() && (reg + k) < max_reg_cnt; k++) {
         if (!handle_write(ctx, node, reg + k))
            is_candidate = false;
      }
   }

   if (writes_exec_implicitly(instr)) {
      for (unsigned reg = exec_lo.reg(); reg <= exec_hi.reg(); reg++) {
         if (!handle_write(ctx, node, reg))
            is_candidate = false;
      }
   }

   if (is_candidate)
      ctx.candidates.insert(node);
}

void select_candidates(sched_ctx &ctx)
{
   while (!ctx.candidates.empty()) {
      Node *next_instr = select_candidate(ctx);
      ctx.new_instructions.emplace_back(std::move(ctx.block->instructions[next_instr->index]));
   }
}

} /* end of anonymous namespace */

void schedule_postRA(Program *program)
{
   for (auto &block : program->blocks) {
      sched_ctx ctx(&block);

      for (unsigned index = 0; index < ctx.block->instructions.size(); index++) {
         const Instruction* instr = ctx.block->instructions[index].get();

         if (is_unschedulable(instr)) {
            select_candidates(ctx);
            ctx.nodes.emplace_back(index);
            ctx.new_instructions.emplace_back(std::move(block.instructions[index]));
            ctx.barrier();
         } else {
            add_to_dag(ctx, instr, index);
         }
      }

      select_candidates(ctx);
      assert(ctx.candidates.empty());
      assert(block.instructions.size() == ctx.new_instructions.size());
      block.instructions.swap(ctx.new_instructions);
   }
}

}
