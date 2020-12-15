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
 */
#include "aco_ir.h"
#include "util/crc32.h"

namespace aco {

/* sgpr_presched/vgpr_presched */
void collect_presched_stats(Program *program)
{
   RegisterDemand presched_demand;
   for (Block& block : program->blocks)
      presched_demand.update(block.register_demand);
   program->statistics[statistic_sgpr_presched] = presched_demand.sgpr;
   program->statistics[statistic_vgpr_presched] = presched_demand.vgpr;
}

/* instructions/branches/vmem_clauses/smem_clauses/cycles */
void collect_preasm_stats(Program *program)
{
   double cycles = 0.0;
   for (Block& block : program->blocks) {
      std::set<Temp> vmem_clause_res;
      std::set<Temp> smem_clause_res;

      program->statistics[statistic_instructions] += block.instructions.size();

      for (aco_ptr<Instruction>& instr : block.instructions) {
         if (instr->isSOPP() && instr->sopp().block != -1)
            program->statistics[statistic_branches]++;

         if (instr->opcode == aco_opcode::p_constaddr)
            program->statistics[statistic_instructions] += 2;

         if (instr->isVMEM() && !instr->operands.empty()) {
            vmem_clause_res.insert(instr->operands[0].getTemp());
         } else {
            program->statistics[statistic_vmem_clauses] += vmem_clause_res.size();
            vmem_clause_res.clear();
         }

         if (instr->isSMEM() && !instr->operands.empty()) {
            if (instr->operands[0].size() == 2)
               smem_clause_res.insert(Temp(0, s2));
            else
               smem_clause_res.insert(instr->operands[0].getTemp());
         } else {
            program->statistics[statistic_smem_clauses] += smem_clause_res.size();
            smem_clause_res.clear();
          }

         /* TODO: this incorrectly assumes instructions always take 4 cycles */
         /* TODO: it would be nice to be able to consider estimated loop trip counts */
         /* assume loops execute 4 times, uniform branches are taken 50% the time,
          * and all lanes take a side of a divergent branch 25% of the time. */
         double loop_trip_count = 4.0;
         double uniform_if_taken = 0.5;
         double divergent_if_all_taken = 0.25;

         double iter = pow(loop_trip_count, block.loop_nest_depth);
         iter *= pow(uniform_if_taken, block.uniform_if_depth);
         iter *= pow(1.0 - divergent_if_all_taken, block.divergent_if_logical_depth);
         iter *= pow(divergent_if_all_taken, block.divergent_if_linear_depth);

         unsigned instr_cycles = instr->opcode == aco_opcode::v_mul_lo_u32 ? 16 : 4;
         cycles += instr_cycles * iter;
      }

      program->statistics[statistic_vmem_clauses] += vmem_clause_res.size();
      program->statistics[statistic_smem_clauses] += smem_clause_res.size();
   }

   program->statistics[statistic_cycles] = cycles;
}

void collect_postasm_stats(Program *program, const std::vector<uint32_t>& code)
{
   program->statistics[aco::statistic_hash] = util_hash_crc32(code.data(), code.size() * 4);
}

}
