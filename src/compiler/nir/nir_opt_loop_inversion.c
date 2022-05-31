/*
 * Copyright © 2022 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "nir.h"
#include "nir_builder.h"
#include "nir_control_flow.h"
#include "nir_loop_analyze.h"


/**
 * \file nir_opt_loop_inversion.c
 * Rearrarange loops to use a backward branching style.
 *
 * Traditional CISC CPUs had instructions like IA-32 LOOP or MC68000 DBRA
 * which would decrement a register and branch if the new value was non-zero.
 * On these architectures, it would be advantageous in some cases to rearrange
 * a traditional for-loop like
 *
 *    for (i = 0; i < 6; i++) {
 *       ...
 *    }
 *
 * to behave more like
 *
 *    int c = 6;
 *    do {
 *       ...
 *    } while (--c > 0);
 *
 * The exact details depend on the CPU.
 *
 * On modern CPUs, such instructions are less important.  They are also often
 * not used due to various limitations (e.g., can only use a 16-bit counter).
 * However, branch predictors will treat backward conditional branches
 * differently than forward conditional branches.
 *
 * Even without these CPU oriented hardware optimizations, orgranizing a loop
 * in this way can be beneficial.  If a loop has 10 iterations, the first
 * pattern will result in the backward branch being taken 10 times, and the
 * forward branch will be taken once.  The second pattern will result in the
 * backward branch being taking 9 times, and the forward branch will be taken
 * once.
 *
 * This can be further improved if the GPU supports a conditional "loop back
 * to the top" instruction.  In this case, the backward branch is taken 9
 * times, and there is no forward branch.  If the loop condition can be
 * rewritten as a comparison with zero, it may also be possible to eliminate
 * the explicit comparison instruction.
 *
 * In compiler literature, this is transformation is often called loop
 * inversion. See also https://en.wikipedia.org/wiki/Loop_inversion.
 */

/**
 * Determine if an if-statement contains only a break.
 */
static bool
if_contains_only_break(const nir_if *nif, bool continue_from_then)
{
   const struct exec_list *const brk =
      continue_from_then ? &nif->else_list : &nif->then_list;

   if (!exec_list_is_singular(brk))
      return false;

   nir_block *b = nir_cf_node_as_block(exec_node_data(nir_cf_node,
                                                      brk->head_sentinel.next,
                                                      node));
   if (nir_block_last_instr(b) != nir_block_first_instr(b) ||
       !nir_block_ends_in_break(b))  {
      return false;
   }

   const struct exec_list *const cnt =
      continue_from_then ? &nif->then_list : &nif->else_list;

   return nir_cf_list_is_empty_block((struct exec_list *) cnt);
}

static bool
nir_src_is_phi(const nir_src src)
{
   return src.is_ssa &&
          src.ssa->parent_instr->type == nir_instr_type_phi;
}

static bool
is_condition_of_phi_and_constant(const nir_instr *instr)
{
   if (instr->type != nir_instr_type_alu)
      return false;

   const nir_alu_instr *const alu = nir_instr_as_alu(instr);

   if (!nir_alu_instr_is_comparison(alu) ||
       nir_op_infos[alu->op].num_inputs != 2)
      return false;

   /* This doesn't explicitly check that the phi is from the loop
    * header. Perhaps it should? Is that redundant?
    */
   return ((nir_src_is_const(alu->src[0].src) &&
            nir_src_is_phi(alu->src[1].src)) ||
           (nir_src_is_phi(alu->src[0].src) &&
            nir_src_is_const(alu->src[1].src)));
}

/**
 * Determine whether a block is part of the CFG tree of the specificed loop.
 */
static bool
is_block_in_loop(nir_loop *loop, const nir_block *block)
{
   nir_foreach_block_in_cf_node(b, &loop->cf_node) {
      if (b == block)
         return true;
   }

   return false;
}

/**
 * Like nir_phi_get_src_from_block
 */
static nir_phi_src *
nir_phi_get_src_from_loop(nir_phi_instr *phi, nir_loop *loop)
{
   nir_foreach_phi_src(phi_src, phi) {
      if (is_block_in_loop(loop, phi_src->pred))
         return phi_src;
   }

   return NULL;
}

static bool
process_loop(nir_shader *sh, nir_cf_node *cf_node, nir_builder *bld)
{
   bool progress = false;
   nir_loop *loop;

   switch (cf_node->type) {
   case nir_cf_node_block:
      return false;

   case nir_cf_node_if: {
      nir_if *if_stmt = nir_cf_node_as_if(cf_node);

      foreach_list_typed_safe(nir_cf_node, nested_node, node, &if_stmt->then_list)
         progress = process_loop(sh, nested_node, bld) || progress;

      foreach_list_typed_safe(nir_cf_node, nested_node, node, &if_stmt->else_list)
         progress = process_loop(sh, nested_node, bld) || progress;

      return progress;
   }

   case nir_cf_node_loop: {
      loop = nir_cf_node_as_loop(cf_node);

      foreach_list_typed_safe(nir_cf_node, nested_node, node, &loop->body)
         progress = process_loop(sh, nested_node, bld) || progress;

      break;
   }

   default:
      unreachable("unknown cf node type");
   }

   /* "Complex" loops might not have all the terminator information in the
    * list of terminators.  Just bail out on those.
    */
   if (loop->info->complex_loop)
      return progress;

   if (loop->info->exact_trip_count_known) {
      assert(list_is_singular(&loop->info->loop_terminator_list));

      /* This is very important, and it's a little subtle. Check that the
       * terminator is of the form
       *
       *     if (some_phi_node cmp constant)
       *         break;
       *
       * This check is used partially as a proxy for checking that this loop
       * hasn't already been modified by this optimization pass.  Once the
       * loop has been modified, it will have the form
       *
       *     ssa_XYZ = some_phi_node MATH constant
       *     if (ssa_XYZ cmp constant)
       *         break;
       *
       * This could trip over some cases like
       *
       *    for (int i = 0; i < imin(x, 4); i++)
       *       ...
       *
       * But those shouldn't hit the `exact_trip_count_known` path.
       *
       * The checks are fairly strict for another reason.  The terminating
       * if-statement is going to be moved to the bottom of the loop.  If the
       * body of that if-statement contains any uses of phi-nodes from the
       * loop header, they would need to be modified to use the phi sources
       * from the body of the loop.  Rather than deal with that, require the
       * if-statement contain only the break.
       */
      nir_loop_terminator *term =
         list_first_entry(&loop->info->loop_terminator_list,
                          nir_loop_terminator, loop_terminator_link);

      assert(!term->exact_trip_count_unknown);

      /* Loop unrolling should have already handled this case. */
      if (loop->info->max_trip_count == 0)
         return progress;

      if (!is_condition_of_phi_and_constant(term->conditional_instr))
         return progress;

      if (!if_contains_only_break(term->nif, term->continue_from_then))
         return progress;

      nir_block *const loop_header = nir_loop_first_block(loop);
      const nir_alu_instr *const orig_cmp =
         nir_instr_as_alu(term->conditional_instr);
      bld->cursor = nir_after_block(nir_loop_last_block(loop));

      nir_ssa_def *src[2] = { NULL, NULL };

      for (unsigned i = 0; i < 2; i++) {
         if (nir_src_is_phi(orig_cmp->src[i].src)) {
            nir_phi_instr *phi = nir_src_as_phi(orig_cmp->src[i].src);

            if (phi->instr.block == loop_header) {
               /* For this process to work, this phi should have two sources.
                * One is from inside the loop and the other from outside.  The
                * source from inside the loop will become the source of the
                * new loop terminator comparison.
                */
               assert(exec_list_length(&phi->srcs) == 2);
               nir_foreach_phi_src(phi_src, phi) {
                  if (is_block_in_loop(loop, phi_src->pred)) {
                     const unsigned swiz = orig_cmp->src[i].swizzle[0];

                     src[i] = nir_swizzle(bld, phi_src->src.ssa, &swiz, 1);
                     assert(!orig_cmp->src[i].negate);
                     assert(!orig_cmp->src[i].abs);

                     break;
                  }
               }

               continue;
            }
         }

         /* Make a copy of the original source to apply any swizzles, etc. */
         src[i] = nir_mov_alu(bld, orig_cmp->src[i], 1);
      }

      assert(src[0] != NULL && src[1] != NULL);

      nir_ssa_def *cmp = nir_build_alu2(bld, orig_cmp->op, src[0], src[1]);

      nir_push_if(bld, cmp);

      if (term->continue_from_then)
         nir_push_else(bld, NULL);

      /* Save this block and cursor location for later use. */
      nir_block *const break_block = nir_cursor_current_block(bld->cursor);
      nir_cursor before_new_break = bld->cursor;

      nir_jump(bld, nir_jump_break);

      nir_pop_if(bld, NULL);

      /* Update the loop-closing phi nodes. */
      nir_block *block_after_loop =
         nir_block_cf_tree_next(nir_loop_last_block(loop));
      nir_foreach_instr(instr, block_after_loop) {
         if (instr->type != nir_instr_type_phi)
            break;

         /* In the old loop form, the source of each loop-closing phi node
          * should be either a phi node from the loop header or a vecN whose
          * sources are all phi nodes from the loop header.  In this case, add
          * a source to the loop-closing phi node that is the source of the
          * loop-header phi that is calculated inside the loop.
          */
         nir_phi_instr *lc_phi = nir_instr_as_phi(instr);

         assert(exec_list_is_singular(&lc_phi->srcs));

         nir_phi_src *phi_src =
            (nir_phi_src *) exec_list_get_head_raw(&lc_phi->srcs);

         nir_instr *asdf = phi_src->src.ssa->parent_instr;

         if (asdf->type == nir_instr_type_phi) {
            nir_phi_instr *lh_phi = nir_instr_as_phi(asdf);

            nir_phi_src *phi_src_from_loop =
               nir_phi_get_src_from_loop(lh_phi, loop);

            nir_phi_src *new_lc_phi_src =
               nir_phi_instr_add_src(lc_phi, break_block, phi_src_from_loop->src);

            list_addtail(&new_lc_phi_src->src.use_link,
                         &phi_src_from_loop->src.ssa->uses);
         } else if (asdf->type == nir_instr_type_alu &&
                    nir_op_is_vec(nir_instr_as_alu(asdf)->op)) {
            /* If the loop-closing phi node has a source that is a vecN, a new
             * vecN must be constructed in the block with the break.
             */
            nir_alu_instr *vecN = nir_instr_as_alu(asdf);

            bld->cursor = before_new_break;

            nir_ssa_def *vecN_srcs[NIR_ALU_MAX_INPUTS];

            for (unsigned i = 0; i < nir_op_infos[vecN->op].num_inputs; i++) {
               nir_phi_instr *lh_phi =
                  nir_instr_as_phi(vecN->src[i].src.ssa->parent_instr);

               nir_phi_src *phi_src_from_loop =
                  nir_phi_get_src_from_loop(lh_phi, loop);

               vecN_srcs[i] = nir_ssa_for_src(bld, phi_src_from_loop->src, 1);
            }

            nir_ssa_def *new_vecN =
               nir_build_alu_src_arr(bld, vecN->op, vecN_srcs);

            nir_phi_src *new_lc_phi_src =
               nir_phi_instr_add_src(lc_phi, break_block,
                                     nir_src_for_ssa(new_vecN));

            list_addtail(&new_lc_phi_src->src.use_link,
                         &new_vecN->uses);
         } else {
            assert(!"Phi source type is boats.");
         }
      }

      nir_cf_node_remove(&term->nif->cf_node);

      progress = true;
   }

   return progress;
}

static bool
nir_opt_loop_inversion_impl(nir_function_impl *impl)
{
   bool progress = false;

   nir_metadata_require_loop_analysis(impl, nir_var_all, false);
   nir_metadata_require(impl, nir_metadata_block_index);

   nir_builder b;
   nir_builder_init(&b, impl);

   foreach_list_typed_safe(nir_cf_node, node, node, &impl->body)
      progress = process_loop(impl->function->shader, node, &b) || progress;

   if (progress) {
      nir_metadata_preserve(impl, nir_metadata_none);
   } else {
      nir_metadata_preserve(impl, nir_metadata_all);
   }

   return progress;
}

bool
nir_opt_loop_inversion(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (function->impl)
         progress = nir_opt_loop_inversion_impl(function->impl) || progress;
   }

   return progress;
}
