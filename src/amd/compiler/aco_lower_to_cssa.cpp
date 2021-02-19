/*
 * Copyright © 2019 Valve Corporation
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

#include <unordered_map>
#include "aco_ir.h"
#include "aco_builder.h"

/*
 * Implements an algorithm to lower to Concentional SSA Form (CSSA).
 * After "Revisiting Out-of-SSA Translation for Correctness, CodeQuality, and Efficiency"
 * by B. Boissinot, A. Darte, F. Rastello, B. Dupont de Dinechin, C. Guillon,
 *
 * By lowering the IR to CSSA, the insertion of parallelcopies is separated from
 * the register coalescing problem. Additionally, correctness is ensured w.r.t. spilling.
 * The algorithm tries to find beneficial insertion points by checking if a basic block
 * is empty and if the variable already has a new definition in a dominating block.
 */


namespace aco {
namespace {

typedef std::vector<Temp> merge_set;

struct copy {
   Definition def;
   Operand op;
};

struct merge_node {
   Operand value;
   uint32_t index;
   uint32_t defined_at;
};

struct cssa_ctx {
   Program* program;
   live& live_vars;
   std::vector<std::vector<copy>> parallelcopies; // copies per block
   std::vector<merge_set> merge_sets; // each vector is one (ordered) merge set
   std::unordered_map<uint32_t, merge_node> merge_node_table; // tempid -> merge set index
};


void collect_parallelcopies(cssa_ctx& ctx)
{
   ctx.parallelcopies.resize(ctx.program->blocks.size());
   Builder bld(ctx.program);
   for (Block& block : ctx.program->blocks) {
      for (aco_ptr<Instruction>& phi : block.instructions) {
         if (phi->opcode != aco_opcode::p_phi &&
             phi->opcode != aco_opcode::p_linear_phi)
            break;

         std::vector<unsigned>& preds = phi->opcode == aco_opcode::p_phi ?
                                        block.logical_preds :
                                        block.linear_preds;
         const Definition& def = phi->definitions[0];
         uint32_t index = ctx.merge_sets.size();
         merge_set set;

         for (unsigned i = 0; i < phi->operands.size(); i++) {
            Operand op = phi->operands[i];
            if (op.isUndefined())
               continue;

            /* create new temporary and rename operands */
            Temp tmp = bld.tmp(def.regClass());
            ctx.parallelcopies[preds[i]].emplace_back(copy{Definition(tmp), op});
            phi->operands[i] = Operand(tmp);

            /* place the new operands in the same merge set */
            set.emplace_back(tmp);
            ctx.merge_node_table[tmp.id()] = {op, index, preds[i]};

            /* update the liveness information */
            if (op.isKill())
               ctx.live_vars.live_out[preds[i]].erase(op.tempId());
            ctx.live_vars.live_out[preds[i]].insert(tmp.id());
         }
         /* place the definition in dominance-order */
         if (def.isTemp()) {
            if (block.kind & block_kind_loop_header)
               set.emplace(std::next(set.begin()), def.getTemp());
            else
               set.emplace(set.end(), def.getTemp());
            ctx.merge_node_table[def.tempId()] = {Operand(def.getTemp()), index, block.index};
         }
         ctx.merge_sets.emplace_back(set);
      }
   }
}

/* check whether the defining block of a comes after b. */
bool defined_after(cssa_ctx& ctx, Temp a, Temp b)
{
   merge_node& node_a = ctx.merge_node_table[a.id()];
   merge_node& node_b = ctx.merge_node_table[b.id()];
   if (node_a.defined_at > node_b.defined_at)
      return true;
   else
      return false;
}

/* check whether block(a) dominates block(b) */
bool dominates(cssa_ctx& ctx, Temp a, Temp b)
{
   merge_node& node_a = ctx.merge_node_table[a.id()];
   merge_node& node_b = ctx.merge_node_table[b.id()];
   unsigned idom = node_b.defined_at;
   while (idom > node_a.defined_at)
      idom = b.regClass().type() == RegType::vgpr ?
             ctx.program->blocks[idom].logical_idom :
             ctx.program->blocks[idom].linear_idom;

   return idom == node_a.defined_at;
}

/* check interference between parent and var:
 * We already know that block(parent) dominates block(var). */
bool interference(cssa_ctx& ctx, Temp var, Temp parent)
{
   merge_node& node_var = ctx.merge_node_table[var.id()];
   merge_node& node_parent = ctx.merge_node_table[parent.id()];

   /* If they are already in the same set, there is no need to re-check */
   if (node_var.index == node_parent.index)
      return false;

   /* equal values don't interfere */
   if (node_var.value == node_parent.value)
      return false;

   /* if both variables are live-out, they interfere */
   uint32_t block_idx = node_var.defined_at;
   bool parent_live = ctx.live_vars.live_out[block_idx].count(parent.id());
   bool var_live = ctx.live_vars.live_out[block_idx].count(var.id());
   if (parent_live && var_live)
      return true;

   /* parent is defined in a different block than var */
   if (node_parent.defined_at < node_var.defined_at) {
      /* if parent is live-in and live-out, they interfere */
      if (parent_live)
         return true;

      /* if the parent is not live-in, they don't interfere */
      std::vector<uint32_t>& preds = var.type() == RegType::vgpr ?
                                     ctx.program->blocks[block_idx].logical_preds :
                                     ctx.program->blocks[block_idx].linear_preds;
      for (uint32_t pred : preds) {
         if (!ctx.live_vars.live_out[pred].count(parent.id()))
            return false;
      }
   }

   /* both, parent and var, are present in the same block */

   // TODO

   return true;
}

/* tries to merge set_b into set_a of given temporary */
bool try_merge_merge_set(cssa_ctx& ctx, Temp dst, merge_set& set_b)
{
   auto def_node_it = ctx.merge_node_table.find(dst.id());
   uint32_t index = def_node_it->second.index;
   merge_set& set_a = ctx.merge_sets[index];
   std::vector<Temp> dom; /* stack of the traversal */
   merge_set union_set;
   uint32_t i_a = 0;
   uint32_t i_b = 0;

   while (i_a < set_a.size() || i_b < set_b.size()) {
      Temp current;
      if (i_a == set_a.size())
         current = set_b[i_b++];
      else if (i_b == set_b.size())
         current = set_a[i_a++];
      /* else pick the one defined first */
      else if (defined_after(ctx, set_a[i_a], set_b[i_b]))
         current = set_b[i_b++];
      else
         current = set_a[i_a++];

      Temp other = dom.empty() ? Temp() : dom.back();
      while (other != Temp() && !dominates(ctx, other, current)) {
         dom.pop_back(); /* not the desired parent, remove */
         other = dom.empty() ? Temp() : dom.back(); /* consider next one */
      }
      Temp parent = other;

      if (parent != Temp() && interference(ctx, current, parent))
         return false; /* intersection detected */

      dom.emplace_back(current); /* otherwise, keep checking */
      if (current != dst)
         union_set.emplace_back(current);
   }

   /* replace set_a and update hashmap */
   for (Temp t : set_b)
      ctx.merge_node_table[t.id()].index = index;
   set_b = merge_set(); /* free the old set_b */
   ctx.merge_sets[index] = union_set;
   ctx.merge_node_table.erase(def_node_it);
   return true;
}

bool try_coalesce_copy(cssa_ctx& ctx, copy copy, uint32_t block_idx)
{
   /* we can only coalesce copies of the same register class */
   if (!copy.op.isTemp())
      return false;
   if (copy.op.regClass() != copy.def.regClass())
      return false;

   assert(ctx.merge_node_table.find(copy.def.tempId()) != ctx.merge_node_table.end());
   auto&& merge_node_it = ctx.merge_node_table.find(copy.op.tempId());
   if (merge_node_it != ctx.merge_node_table.end()) {
      /* check if the operand already has a merge_set */
      if (merge_node_it->second.index != -1u)
         return try_merge_merge_set(ctx, copy.def.getTemp(), ctx.merge_sets[merge_node_it->second.index]);

      merge_set op_set = merge_set{copy.op.getTemp()};
      return try_merge_merge_set(ctx, copy.def.getTemp(), op_set);
   }

   /* find defining block of operand */
   uint32_t pred = block_idx;
   do {
      block_idx = pred;
      pred = copy.op.regClass().type() == RegType::vgpr ?
             ctx.program->blocks[pred].logical_idom :
             ctx.program->blocks[pred].linear_idom;
   } while (block_idx != pred &&
            ctx.live_vars.live_out[pred].count(copy.op.tempId()));

   ctx.merge_node_table[copy.op.tempId()] = {copy.op, -1u, block_idx};
   merge_set op_set = merge_set{copy.op.getTemp()};
   return try_merge_merge_set(ctx, copy.def.getTemp(), op_set);
}

/* node in the location-transfer-graph */
struct ltg_node {
   copy cp;
   uint32_t num_uses;
   uint32_t read_idx;
   uint32_t write_idx;
};

void emit_copies_block(Builder bld, std::vector<ltg_node>& ltg, RegType type)
{
   auto&& it = ltg.begin();
   unsigned num = 0; /* number of remaining circular copies */
   while (it != ltg.end()) {
      /* already emitted or wrong regclass */
      if (it->write_idx == -1u || it->cp.def.regClass().type() != type) {
         ++it;
         continue;
      }
      /* the target is still needed as operand */
      if (it->num_uses > 0) {
         num++;
         ++it;
         continue;
      }

      bld.copy(it->cp.def, it->cp.op);

      for (ltg_node& node : ltg) {
         if (it->write_idx == node.read_idx)
            node.num_uses--;
      }

      it->write_idx = -1u;
      it = ltg.begin();
      num = 0;
   }

   /* if there are circular dependencies, we just emit them as single parallelcopy */
   if (num) {
      aco_ptr<Pseudo_instruction> copy{create_instruction<Pseudo_instruction>(aco_opcode::p_parallelcopy, Format::PSEUDO, num, num)};
      it = ltg.begin();
      for (unsigned i = 0; i < num; i++) {
         while (it->write_idx == -1u || it->cp.def.regClass().type() != type)
            ++it;

         copy->definitions[i] = it->cp.def;
         copy->operands[i] = it->cp.op;
         it->write_idx = -1u;
      }
      bld.insert(std::move(copy));
   }
}

void emit_parallelcopies(cssa_ctx& ctx)
{
   std::unordered_map<uint32_t, Operand> renames;

   for (unsigned i = 0; i < ctx.program->blocks.size(); i++) {
      std::vector<ltg_node> ltg;
      /* first, try to coalesce all parallelcopies */
      for (const copy& cp : ctx.parallelcopies[i]) {
         if (try_coalesce_copy(ctx, cp, i)) {
            renames.emplace(cp.def.tempId(), cp.op);
            /* update liveness info */
            ctx.live_vars.live_out[i].erase(cp.def.tempId());
            if (cp.op.isTemp())
               ctx.live_vars.live_out[i].insert(cp.op.tempId());
         } else {
            uint32_t read_idx = -1u;
            if (cp.op.isTemp())
               read_idx = ctx.merge_node_table[cp.op.tempId()].index;
            uint32_t write_idx = ctx.merge_node_table[cp.def.tempId()].index;
            assert(write_idx != -1u);
            ltg.emplace_back(ltg_node{cp, 0, read_idx, write_idx});
         }
      }

      /* build location-transfer-graph */
      for (ltg_node& node : ltg) {
         if (!node.cp.op.isTemp())
            continue;

         for (ltg_node& other : ltg) {
            if (other.write_idx == node.read_idx)
               node.num_uses++;
         }
      }

      /* emit parallelcopies ordered */
      Builder bld(ctx.program);
      Block& block = ctx.program->blocks[i];

      /* emit VGPR copies */
      auto IsLogicalEnd = [] (const aco_ptr<Instruction>& inst) -> bool {
         return inst->opcode == aco_opcode::p_logical_end;
      };
      auto it = std::find_if(block.instructions.rbegin(), block.instructions.rend(), IsLogicalEnd);
      bld.reset(&block.instructions, std::prev(it.base()));
      emit_copies_block(bld, ltg, RegType::vgpr);

      /* emit SGPR copies */
      aco_ptr<Instruction> branch = std::move(block.instructions.back());
      block.instructions.pop_back();
      bld.reset(&block.instructions);
      emit_copies_block(bld, ltg, RegType::sgpr);
      bld.insert(std::move(branch));
   }

   /* finally, rename coalesced phi operands */
   for (Block& block : ctx.program->blocks) {
      for (aco_ptr<Instruction>& phi : block.instructions) {
         if (phi->opcode != aco_opcode::p_phi &&
             phi->opcode != aco_opcode::p_linear_phi)
            break;

         for (Operand& op : phi->operands) {
            if (!op.isTemp())
               continue;
            auto&& it = renames.find(op.tempId());
            if (it != renames.end()) {
               op = it->second;
               renames.erase(it);
            }
         }
      }
   }
   assert(renames.empty());
}

} /* end namespace */


void lower_to_cssa(Program* program, live& live_vars)
{
   cssa_ctx ctx = {program, live_vars};
   collect_parallelcopies(ctx);
   emit_parallelcopies(ctx);

   /* update live variable information */
   live_vars = live_var_analysis(program);
}
}

