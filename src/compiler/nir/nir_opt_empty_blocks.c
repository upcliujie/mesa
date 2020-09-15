/*
 * Copyright © 2020 Intel Corporation
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
 */

#include "nir.h"
#include "nir_vla.h"

static nir_phi_src *
phi_src_for_pred(nir_phi_instr *phi, nir_block *pred)
{
   nir_foreach_phi_src(src, phi) {
      if (src->pred == pred)
         return src;
   }
   return NULL;
}

static void
phi_add_src(nir_phi_instr *phi, nir_block *pred, nir_src src)
{
   nir_phi_src *new_src = rzalloc(phi, nir_phi_src);
   new_src->pred = pred;
   new_src->src = NIR_SRC_INIT;
   exec_list_push_tail(&phi->srcs, &new_src->node);
   nir_instr_rewrite_src(&phi->instr, &new_src->src, src);
}

static int
compare_blocks(const void *_a, const void *_b)
{
   const nir_block * const * a = _a;
   const nir_block * const * b = _b;

   return (*a)->index - (*b)->index;
}

static bool
merge_phis(nir_block *succ, nir_block *pred, bool test)
{
   if (succ->predecessors->entries == 1) {
      /* If this is the unique predecessor of the successor, there's nothing
       * interesting to do; we just copy in the phis.
       */
      if (test)
         return true;

      nir_foreach_instr_reverse_safe(instr, pred) {
         if (instr->type != nir_instr_type_phi) {
            assert(instr->type == nir_instr_type_jump);
            continue;
         }
         nir_instr_remove(instr);
         nir_instr_insert(nir_before_block(succ), instr);
      }
   } else {
      const unsigned num_pred_preds = pred->predecessors->entries;
      NIR_VLA(nir_block *, pred_preds, num_pred_preds);
      {
         unsigned i = 0;
         set_foreach(pred->predecessors, entry)
            pred_preds[i++] = (nir_block *)entry->key;
         assert(i == num_pred_preds);
      }
      qsort(pred_preds, num_pred_preds, sizeof(*pred_preds), compare_blocks);

      nir_foreach_instr(instr, succ) {
         if (instr->type != nir_instr_type_phi)
            break;

         nir_phi_instr *phi = nir_instr_as_phi(instr);
         nir_phi_src *phi_src = phi_src_for_pred(phi, pred);
         assert(phi_src->src.is_ssa);
         if (phi_src->src.ssa->parent_instr->block == pred) {
            /* In this case, the phi source comes from a phi in the
             * predecessor.   We know a priori that the predecessor only
             * contains phis and jumps so this must be a phi.
             */
            nir_phi_instr *pred_phi =
               nir_instr_as_phi(phi_src->src.ssa->parent_instr);
            if (test) {
               /* We need to ensure that any shared predecessors have the same
                * value.
                */
               nir_foreach_phi_src(pred_phi_src, pred_phi) {
                  nir_foreach_phi_src(succ_phi_src, phi) {
                     if (succ_phi_src->pred == pred_phi_src->pred &&
                         succ_phi_src->src.ssa != pred_phi_src->src.ssa)
                        return false;
                  }
               }
            } else {
               /* Any sources for non-shared predecessors need to get moved to
                * the successor block.
                */
               nir_foreach_phi_src_safe(pred_phi_src, pred_phi) {
                  if (_mesa_set_search(succ->predecessors, pred_phi_src->pred))
                     continue;

                  phi_add_src(phi, pred_phi_src->pred, pred_phi_src->src);
               }
            }
         } else {
            /* In this case, the phi source comes from something that
             * dominates pred.
             */
            if (test) {
               /* We need to ensure that any shared predecessors have the same
                * value.
                */
               nir_foreach_phi_src(succ_phi_src, phi) {
                  if (_mesa_set_search(pred->predecessors,
                                       succ_phi_src->pred) &&
                      succ_phi_src->src.ssa != phi_src->src.ssa)
                     return false;
               }
            } else {
               /* We need to add sources for any non-shared predecessors. */
               for (unsigned i = 0; i < num_pred_preds; i++) {
                  if (_mesa_set_search(succ->predecessors, pred_preds[i]))
                     continue;

                  phi_add_src(phi, pred_preds[i], phi_src->src);
               }
            }
         }
      }
   }

   return true;
}

static void
rewrite_pred_jumps(nir_block *block, nir_block *new_target)
{
   set_foreach(block->predecessors, entry) {
      nir_block *pred = (nir_block *)entry->key;
      nir_jump_instr *pred_jump =
         nir_instr_as_jump(nir_block_last_instr(pred));

      if (pred_jump->type == nir_jump_goto) {
         assert(pred_jump->target == block);
         assert(pred->successors[0] == pred_jump->target);
         assert(pred->successors[1] == NULL);
         pred->successors[0] = pred_jump->target = new_target;
      } else {
         assert(pred_jump->type == nir_jump_goto_if);
         assert(pred_jump->target == block ||
                pred_jump->else_target == block);
         assert(pred->successors[0] == pred_jump->else_target);
         assert(pred->successors[1] == pred_jump->target);
         if (pred_jump->target == block)
            pred->successors[1] = pred_jump->target = new_target;
         if (pred_jump->else_target == block)
            pred->successors[0] = pred_jump->else_target = new_target;

         /* nir_validate doesn't allow a block to have both successors
          * point to the same block.  Turn goto_if into goto if both
          * blocks are the same.
          */
         if (pred_jump->target == pred_jump->else_target) {
            pred_jump->type = nir_jump_goto;
            nir_instr_rewrite_src(&pred_jump->instr,
                                  &pred_jump->condition,
                                  NIR_SRC_INIT);
            pred->successors[1] = pred_jump->else_target = NULL;
         }
      }

      _mesa_set_add(new_target->predecessors, pred);
   }
}

static bool
opt_empty_blocks_impl(nir_function_impl *impl)
{
   /* This only works on unstructured control-flow */
   if (impl->structured) {
      nir_metadata_preserve(impl, nir_metadata_all);
      return false;
   }

   bool progress = false;

   nir_foreach_block_unstructured_safe(block, impl) {
      /* If we only have one block, don't remove it, even if empty */
      if (exec_list_is_singular(&impl->body))
         break;

      /* We can only contract edges when the block has a single successor */
      nir_jump_instr *jump = nir_instr_as_jump(nir_block_last_instr(block));
      if (jump->type != nir_jump_goto)
         continue;

      assert(block->successors[1] == NULL);
      nir_block *succ = block->successors[0];

      /* Don't remove the start block if its successor has any other
       * predecessors.  That would result in the start block being a loop
       * head and that's invalid NIR.
       */
      if (block == nir_start_block(impl) && succ->predecessors->entries > 1)
         continue;

      if (succ == block) {
         /* In this case, we're an infinite loop.  That needs to be handled
          * specially.  We make it point to the end block.
          */
         rewrite_pred_jumps(block, impl->end_block);
         /* rewrite_pred_jumps will have added this block to end_block's
          * predecessor list but we want to remove it.
          */
         _mesa_set_remove_key(impl->end_block->predecessors, block);
      } else {
         /* The block must be empty except for the jump instruction and phis */
         nir_instr *prev = nir_instr_prev(&jump->instr);
         if (prev != NULL && prev->type != nir_instr_type_phi)
            continue;

         /* First, we attempt a "test" phi merge.  If it fails, then we can't
          * safely merge the phis between the two blocks.
          */
         if (!merge_phis(succ, block, true))
            continue;

         merge_phis(succ, block, false);

         rewrite_pred_jumps(block, succ);
         _mesa_set_remove_key(succ->predecessors, block);
      }

      progress = true;

      /* Clear out the block and remove it from the CF list */
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_jump)
            nir_instr_remove(instr);
      }
      exec_node_remove(&block->cf_node.node);
      break;
   }

   if (progress) {
      nir_metadata_preserve(impl, nir_metadata_none);
   } else {
      nir_metadata_preserve(impl, nir_metadata_all);
   }

   return progress;
}

bool
nir_opt_empty_blocks(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      /* This only works on unstructured control-flow */
      if (function->impl == NULL || function->impl->structured)
         continue;

      if (opt_empty_blocks_impl(function->impl))
         progress = true;
   }

   return progress;
}
