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
#include "util/bitscan.h"

namespace aco {

namespace {

/* Maximum addressable register */
constexpr size_t max_reg_cnt = 512;

struct Node
{
   int index;
   int priority;
   int latency;
   int start_cycle = -1;
   std::unordered_set<Node *> successors;
   std::unordered_set<Node *> predecessors;
   bool scheduled = false;

   Node(int i, int prio, int lat) : index(i), priority(prio), latency(lat) {}

   bool operator ==(const struct Node &other) const
   {
      return index == other.index;
   }

   bool operator <(const struct Node &other) const
   {
      return index < other.index;
   }
};

struct NodePtrPriorityCompare {
   inline bool operator()(const Node *n1, const Node *n2)
   {
      if (n1 == n2)
         return false;

      /* Make sure we're not adding the same instruction twice. */
      assert(n1->index != n2->index);

      if (n1->priority == n2->priority)
         return n1->index < n2->index;

      return n1->priority > n2->priority;
   }
};

struct sched_ctx
{
   Program *program;
   Block *block;
   std::vector<aco_ptr<Instruction>> new_instructions;
   std::vector<Node> nodes;

   std::set<Node *, NodePtrPriorityCompare> candidates;
   Node *writes[max_reg_cnt] = {0};
   std::unordered_set<Node *> writeless_reads[max_reg_cnt];

   /* Collect these so that we can handle barriers correctly */
   int last_acquirer[storage_count];
   std::vector<Node *> acquired_nodes[storage_count];

   int total_cycles = 0;
   int reg_rd_dn_cycle[max_reg_cnt] = {0};
   int reg_wr_dn_cycle[max_reg_cnt] = {0};

   /* here we can maintain information about the functional units */
   sched_ctx(Program *p, Block *b) : program(p), block(b)
   {
      nodes.reserve(b->instructions.size());

      for (unsigned i = 0; i < storage_count; i++) {
         last_acquirer[i] = -1;
         acquired_nodes[i].reserve(10);
      }
   }

   void barrier()
   {
      assert(candidates.empty());

      /* Clear barrier info */
      for (unsigned i = 0; i < storage_count; i++) {
         last_acquirer[i] = -1;
         acquired_nodes[i].clear();
      }

      /* Clear read/write info */
      for (unsigned i = 0; i < max_reg_cnt; ++i) {
         writes[i] = nullptr;
         writeless_reads[i].clear();
      }
   }
};

Instruction *get_node_instr(sched_ctx &ctx, const Node *node)
{
   return ctx.block->instructions[node->index].get();
}

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
   } else if (instr->isVMEM() || instr->isFlatOrGlobal() || instr->format == Format::SCRATCH) {
      return true;
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

int get_latency(const sched_ctx &ctx, const Instruction *instr)
{
   assert(ctx.program->chip_class >= GFX10);

   /* These numbers are from LLVM's GFX10SpeedModel. */

   switch (instr->format) {
   case Format::SMEM: /* Ballpark estimate according to LLVM */
      return 20;
   case Format::EXP:
      return 16;
   case Format::VINTRP: /* Interpolated inputs are loaded from LDS */
   case Format::DS:
      return 20;
   case Format::PSEUDO_BARRIER:
      return 2000;
   default:
      break;
   }

   if (instr->isVALU()) /* XXX: Distinguish between Write32Bit, Write64Bit, WriteDouble etc? */
      return 5;
   else if (instr->isSALU()) /* Branches and barriers are different, but they are not scheduled anyway */
      return 3;
   else if (instr->isVMEM() || instr->isFlatOrGlobal() || instr->format == Format::SCRATCH) /* Ballpark estimate */
      return 320;

   unreachable("unsupported instruction format");
}

int get_priority(const sched_ctx &ctx, const Instruction *instr)
{
   switch (instr->format) {
   case Format::SMEM:
   case Format::DS:
   case Format::MTBUF:
   case Format::MUBUF:
   case Format::MIMG:
   case Format::FLAT:
   case Format::GLOBAL:
   case Format::SCRATCH:
      /* Priority correlates to latency.
       * In other words, priority means how much latency are we willing to suffer
       * in order to schedule more important instructions to the top.
       */
      return get_latency(ctx, instr) / 2;
   default:
      return 0;
   }
}

template <typename TFunc>
void foreach_reg_read(const Instruction *instr, const TFunc &func)
{
   for (const Operand &op : instr->operands) {
      if (op.isConstant())
         continue;

      unsigned reg = op.physReg().reg();
      if (op.regClass().type() == RegType::vgpr && reg < 256)
         reg += 256;

      assert(op.regClass().type() != RegType::sgpr || reg <= 255);
      assert(op.regClass().type() != RegType::vgpr || reg >= 256);

      for (unsigned k = 0; k < op.size() && (reg + k) < max_reg_cnt; k++) {
         func(reg + k);
      }
   }

   if (reads_exec_implicitly(instr)) {
      for (unsigned reg = exec_lo.reg(); reg <= exec_hi.reg(); reg++) {
         func(reg);
      }
   }
}

template <typename TFunc>
void foreach_reg_write(const Instruction *instr, const TFunc &func)
{
   for (const Definition &def : instr->definitions) {
      unsigned reg = def.physReg().reg();
      if (def.regClass().type() == RegType::vgpr && reg < 256)
         reg += 256;

      assert(def.regClass().type() != RegType::sgpr || reg <= 255);
      assert(def.regClass().type() != RegType::vgpr || reg >= 256);

      for (unsigned k = 0; k < def.size() && (reg + k) < max_reg_cnt; k++) {
         func(reg + k);
      }
   }

   if (writes_exec_implicitly(instr)) {
      for (unsigned reg = exec_lo.reg(); reg <= exec_hi.reg(); reg++) {
         func(reg);
      }
   }
}

bool is_new_candidate(const Node *node)
{
   for (Node *pre : node->predecessors) {
      if (!pre->scheduled)
         return false;
   }

   return true;
}

void save_candidate_cycles(sched_ctx &ctx, Node *node)
{
   assert(node);
   const Instruction *instr = get_node_instr(ctx, node);
   assert(instr);
   ctx.total_cycles = node->start_cycle;

   foreach_reg_read(instr, [&ctx, node] (unsigned reg) {
      ctx.reg_rd_dn_cycle[reg] = node->start_cycle + node->latency;
   });

   foreach_reg_write(instr, [&ctx, node] (unsigned reg) {
      ctx.reg_wr_dn_cycle[reg] = node->start_cycle + node->latency;
   });
}

int calculate_candidate_start_cycle(sched_ctx &ctx, Node *node)
{
   assert(node);
   const Instruction *instr = get_node_instr(ctx, node);
   assert(instr);
   int min_start = 0;

   /* For each register read, we have to wait for the previous write to finish */
   foreach_reg_read(instr, [&ctx, &min_start] (unsigned reg) {
      min_start = MAX2(min_start, ctx.reg_wr_dn_cycle[reg]);
   });

   /* For each register write, we have to wait for the previous write and reads to finish */
   foreach_reg_write(instr, [&ctx, &min_start] (unsigned reg) {
      min_start = MAX2(min_start, ctx.reg_rd_dn_cycle[reg]);
      min_start = MAX2(min_start, ctx.reg_wr_dn_cycle[reg]);
   });

   return MAX2(min_start + 1, ctx.total_cycles + 1);
}

Node* select_candidate(sched_ctx &ctx)
{
   /* The first candidate already has the highest priority */
   assert(ctx.candidates.size() > 0);
   std::set<Node *>::iterator selected_it = ctx.candidates.begin();

   /* The cycle when this instruction can start */
   int selected_start_cycle = calculate_candidate_start_cycle(ctx, *selected_it);

   /* In order to group higher priority instructions together, we are willing to tolerate some extra cycles */
   int selected_cost = selected_start_cycle - ((*selected_it)->priority <= 0 ? 0 : (*selected_it)->priority);

   /* Find the most suitable candidate. */
   for (auto it = std::next(selected_it);
        it != ctx.candidates.end();
        ++it) {

      int start_cycle = calculate_candidate_start_cycle(ctx, *it);
      int cost = start_cycle - ((*it)->priority <= 0 ? 0 : (*it)->priority);

      if (cost < selected_cost) {
         selected_it = it;
         selected_start_cycle = start_cycle;
         selected_cost = cost;
      }
   }

   assert(selected_it != ctx.candidates.end());
   Node *next = *selected_it;
   ctx.candidates.erase(selected_it);
   assert(!next->scheduled);
   next->scheduled = true;
   next->start_cycle = selected_start_cycle;

   /* add successors to list of potential candidates */
   for (Node *n : next->successors) {
      assert(n != next);
      assert(!n->scheduled);

      if (is_new_candidate(n))
         ctx.candidates.insert(n);
   }

   save_candidate_cycles(ctx, next);
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

bool add_predecessor_by_index(sched_ctx &ctx, Node *node, unsigned predecessor_idx)
{
   assert(predecessor_idx >= 0);
   assert(ctx.nodes.size() > predecessor_idx);
   Node *predecessor = &ctx.nodes[predecessor_idx];
   assert(predecessor != node);

   if (predecessor->scheduled)
      return false;

   add_predecessor(ctx, node, predecessor);

   return true;
}

memory_sync_info get_prs_sync_info(const Instruction *instr)
{
   if (instr->format == Format::PSEUDO_BARRIER) {
      return static_cast<const Pseudo_barrier_instruction *>(instr)->sync;
   } else if (instr->opcode == aco_opcode::s_sendmsg) {
      const SOPP_instruction *sopp = static_cast<const SOPP_instruction *>(instr);
      /* Check for MSG_GS and MSG_GS_DONE */
      if (sopp->imm & 3u)
         return memory_sync_info(storage_vmem_output, semantic_acqrel);
      if (sopp->imm & 2u)
         return memory_sync_info(storage_vmem_output);
   }

   return get_sync_info(instr);
}

bool handle_sync(sched_ctx &ctx, const Instruction *instr, unsigned index, Node *node)
{
   memory_sync_info sync = get_prs_sync_info(instr);
   unsigned str = sync.storage;
   unsigned acq = 0;
   unsigned rel = 0;

   if (sync.semantics & semantic_acquire)
      acq |= sync.storage;
   if (sync.semantics & semantic_release)
      rel |= sync.storage;

   if (sync.semantics & semantic_atomic) {
      acq |= sync.storage;
      rel |= sync.storage;
   }

   if (!str && !acq && !rel)
      return true;

   bool added_predecessor = false;

   while (rel) {
      int s = u_bit_scan(&rel);
      assert(s >= 0 && s < storage_count);

      /* Add acquired nodes as predecessors */
      for (auto &acq_node : ctx.acquired_nodes[s])
         added_predecessor |= add_predecessor(ctx, node, acq_node);

      /* Clear last acquirer and acquired nodes */
      ctx.last_acquirer[s] = -1;
      ctx.acquired_nodes[s].clear();
   }

   while (str) {
      int s = u_bit_scan(&str);
      assert(s >= 0 && s < storage_count);

      /* Add last acquirer as a predecessor */
      if (ctx.last_acquirer[s] >= 0)
         added_predecessor |= add_predecessor_by_index(ctx, node, ctx.last_acquirer[s]);

      /* Add current node into the list of acquired nodes */
      if (!((1 << s) & rel))
         ctx.acquired_nodes[s].push_back(node);
   }

   while (acq) {
      int s = u_bit_scan(&acq);
      assert(s >= 0 && s < storage_count);

      /* Set last acquirer to the current node */
      ctx.last_acquirer[s] = index;
   }

   return !added_predecessor;
}

void add_to_dag(sched_ctx &ctx, const Instruction *instr, unsigned index)
{
   assert(!is_unschedulable(instr));
   ctx.nodes.emplace_back(index, get_priority(ctx, instr), get_latency(ctx, instr));
   Node* node = &ctx.nodes.back();
   bool is_candidate = true;

   /* Read after Write */
   foreach_reg_read(instr, [&ctx, &is_candidate, node](unsigned reg) {
      if (!handle_read(ctx, node, reg))
         is_candidate = false;
   });

   /* Write after Write/Read */
   foreach_reg_write(instr, [&ctx, &is_candidate, node](unsigned reg) {
      if (!handle_write(ctx, node, reg))
         is_candidate = false;
   });

   if (!handle_sync(ctx, instr, index, node))
      is_candidate = false;

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
      sched_ctx ctx(program, &block);

      for (unsigned index = 0; index < ctx.block->instructions.size(); index++) {
         const Instruction* instr = ctx.block->instructions[index].get();

         if (is_unschedulable(instr)) {
            select_candidates(ctx);
            ctx.nodes.emplace_back(index, 0, get_latency(ctx, instr));
            save_candidate_cycles(ctx, &ctx.nodes[ctx.nodes.size() - 1]);
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
