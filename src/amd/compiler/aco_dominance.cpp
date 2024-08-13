/*
 * Copyright © 2018 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ACO_DOMINANCE_CPP
#define ACO_DOMINANCE_CPP

#include "aco_ir.h"

/*
 * Implements the algorithms for computing the dominator tree from
 * "A Simple, Fast Dominance Algorithm" by Cooper, Harvey, and Kennedy.
 *
 * Different from the paper, our CFG allows to compute the dominator tree
 * in a single pass as it is guaranteed that the dominating predecessors
 * are processed before the current block.
 */

namespace aco {

namespace {

void
calc_indices(Program* program)
{
   std::vector<uint32_t> logical_size(program->blocks.size());
   std::vector<uint32_t> linear_size(program->blocks.size());
   std::vector<small_vec<uint32_t, 4>> logical_children(program->blocks.size());
   std::vector<small_vec<uint32_t, 4>> linear_children(program->blocks.size());

   for (int i = program->blocks.size() - 1; i >= 0; i--) {
      Block& block = program->blocks[i];

      logical_size[i]++;
      linear_size[i]++;

      if (block.logical_idom != -1 && block.logical_idom != i) {
         assert(i > block.logical_idom);
         logical_children[block.logical_idom].push_back(i);
         logical_size[block.logical_idom] += logical_size[i];
      }
      if (block.linear_idom != i) {
         assert(i > block.linear_idom);
         linear_children[block.linear_idom].push_back(i);
         linear_size[block.linear_idom] += linear_size[i];
      }
   }

   for (unsigned i = 0; i < program->blocks.size(); i++) {
      Block& block = program->blocks[i];
      if (block.logical_idom == (int)i) {
         block.logical_dom_pre_index = i;
         block.logical_dom_post_index = i + logical_size[i] - 1;
      }
      if (block.linear_idom == (int)i) {
         block.linear_dom_pre_index = i;
         block.linear_dom_post_index = i + linear_size[i] - 1;
      }

      unsigned start = block.logical_dom_pre_index;
      for (unsigned j = 0; j < logical_children[i].size(); j++) {
         unsigned child = logical_children[i][j];
         program->blocks[child].logical_dom_pre_index = start;
         program->blocks[child].logical_dom_post_index = start + logical_size[child] - 1;
         start += logical_size[child];
      }

      start = block.linear_dom_pre_index;
      for (unsigned j = 0; j < linear_children[i].size(); j++) {
         unsigned child = linear_children[i][j];
         program->blocks[child].linear_dom_pre_index = start;
         program->blocks[child].linear_dom_post_index = start + linear_size[child] - 1;
         start += linear_size[child];
      }
   }
}

} /* end namespace */

void
dominator_tree(Program* program)
{
   for (unsigned i = 0; i < program->blocks.size(); i++) {
      Block& block = program->blocks[i];

      /* If this block has no predecessor, it dominates itself by definition */
      if (block.linear_preds.empty()) {
         block.linear_idom = block.index;
         block.logical_idom = block.index;
         continue;
      }

      int new_logical_idom = -1;
      int new_linear_idom = -1;
      for (unsigned pred_idx : block.logical_preds) {
         if ((int)program->blocks[pred_idx].logical_idom == -1)
            continue;

         if (new_logical_idom == -1) {
            new_logical_idom = pred_idx;
            continue;
         }

         while ((int)pred_idx != new_logical_idom) {
            if ((int)pred_idx > new_logical_idom)
               pred_idx = program->blocks[pred_idx].logical_idom;
            if ((int)pred_idx < new_logical_idom)
               new_logical_idom = program->blocks[new_logical_idom].logical_idom;
         }
      }

      for (unsigned pred_idx : block.linear_preds) {
         if ((int)program->blocks[pred_idx].linear_idom == -1)
            continue;

         if (new_linear_idom == -1) {
            new_linear_idom = pred_idx;
            continue;
         }

         while ((int)pred_idx != new_linear_idom) {
            if ((int)pred_idx > new_linear_idom)
               pred_idx = program->blocks[pred_idx].linear_idom;
            if ((int)pred_idx < new_linear_idom)
               new_linear_idom = program->blocks[new_linear_idom].linear_idom;
         }
      }

      block.logical_idom = new_logical_idom;
      block.linear_idom = new_linear_idom;
   }

   calc_indices(program);
}

} // namespace aco
#endif
