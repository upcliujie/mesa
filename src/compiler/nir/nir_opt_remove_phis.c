/*
 * Copyright © 2015 Connor Abbott
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
 *    Connor Abbott (cwabbott0@gmail.com)
 *
 */

#include "nir.h"
#include "nir_builder.h"

static nir_alu_instr *
get_parent_mov(nir_ssa_def *ssa)
{
   if (ssa->parent_instr->type != nir_instr_type_alu)
      return NULL;

   nir_alu_instr *alu = nir_instr_as_alu(ssa->parent_instr);
   return (alu->op == nir_op_imov || alu->op == nir_op_fmov) ? alu : NULL;
}

static bool
matching_mov(nir_alu_instr *mov1, nir_ssa_def *ssa)
{
   if (!mov1)
      return false;

   nir_alu_instr *mov2 = get_parent_mov(ssa);
   return mov2 && nir_alu_srcs_equal(mov1, mov2, 0, 0);
}

/*
 * This is a pass for removing phi nodes that look like:
 * a = phi(b, b, b, ...)
 *
 * Note that we can't ignore undef sources here, or else we may create a
 * situation where the definition of b isn't dominated by its uses. We're
 * allowed to do this since the definition of b must dominate all of the
 * phi node's predecessors, which means it must dominate the phi node as well
 * as all of the phi node's uses. In essence, the phi node acts as a copy
 * instruction. b can't be another phi node in the same block, since the only
 * time when phi nodes can source other phi nodes defined in the same block is
 * at the loop header, and in that case one of the sources of the phi has to
 * be from before the loop and that source can't be b.
 */

static bool
remove_phis_instr(nir_block *block, nir_instr *instr, nir_builder *b)
{
   nir_phi_instr *phi = nir_instr_as_phi(instr);
   nir_ssa_def *def = NULL;
   nir_alu_instr *mov = NULL;
   bool srcs_same = true;

   nir_foreach_phi_src(src, phi) {
      assert(src->src.is_ssa);

      /* For phi nodes at the beginning of loops, we may encounter some
       * sources from backedges that point back to the destination of the
       * same phi, i.e. something like:
       *
       * a = phi(a, b, ...)
       *
       * We can safely ignore these sources, since if all of the normal
       * sources point to the same definition, then that definition must
       * still dominate the phi node, and the phi will still always take
       * the value of that definition.
       */
      if (src->src.ssa == &phi->dest.ssa)
         return false;

      if (def == NULL) {
         def  = src->src.ssa;
         mov = get_parent_mov(def);
      } else {
         if (src->src.ssa != def && !matching_mov(mov, src->src.ssa)) {
            srcs_same = false;
            break;
         }
      }
   }

   if (!srcs_same)
      return false;


   /* We must have found at least one definition, since there must be at
    * least one forward edge.
    */
   assert(def != NULL);

   if (mov) {
      /* If the sources were all movs from the same source with the same
       * swizzle, then we can't just pick a random move because it may not
       * dominate the phi node. Instead, we need to emit our own move after
       * the phi which uses the shared source, and rewrite uses of the phi
       * to use the move instead. This is ok, because while the movs may
       * not all dominate the phi node, their shared source does.
       */

      b->cursor = nir_after_phis(block);
      def = mov->op == nir_op_imov ?
         nir_imov_alu(b, mov->src[0], def->num_components) :
         nir_fmov_alu(b, mov->src[0], def->num_components);
   }

   assert(phi->dest.is_ssa);
   nir_ssa_def_rewrite_uses(&phi->dest.ssa, nir_src_for_ssa(def));
   nir_instr_remove(instr);
   return true;
}

/*
 * Convert phis of bool consts to bcsel.
 *
 * This converts phis which are just true/false as arguments into
 * bcsel using the if condition of the blocks.
 */
static bool
phis_to_bools(nir_block *block, nir_instr *instr, nir_builder *b)
{
   nir_phi_instr *phi = nir_instr_as_phi(instr);

   if (exec_list_length(&phi->srcs) != 2)
      return false;

   nir_cf_node *prev_if_block = NULL;
   bool swap_args = false;
   unsigned idx = 0;

   nir_foreach_phi_src(src, phi) {
      assert(src->src.is_ssa);
      nir_cf_node *prev = src->pred->cf_node.parent;
      if (!prev)
         return false;
      if (prev->type != nir_cf_node_if)
         return false;

      /* make sure both phi srcs point to same if block */
      if (!prev_if_block)
         prev_if_block = prev;
      else if (prev != prev_if_block)
         return false;

      if (!nir_src_is_const(src->src))
         return false;

      if (src->src.ssa->bit_size != 1)
         return false;

      bool b = nir_src_as_bool(src->src);
      if (idx == 0 && b == true)
         swap_args = true;
      idx++;
   }

   /* convert the bool phi into a bcsel, algebraic will lower it later */
   idx = 0;
   nir_phi_src *srcs[2];
   nir_foreach_phi_src(src, phi) {
      srcs[swap_args ? (1 - idx) : idx] = src;
      idx++;
   }
   b->cursor = nir_after_phis(instr->block);
   nir_ssa_def *dst = nir_bcsel(b, nir_cf_node_as_if(prev_if_block)->condition.ssa, srcs[0]->src.ssa, srcs[1]->src.ssa);
   nir_ssa_def_rewrite_uses(&phi->dest.ssa, nir_src_for_ssa(dst));
   nir_instr_remove(instr);
   return true;
}

static bool
remove_phis_block(nir_block *block, nir_builder *b)
{
   bool progress = false;

   nir_foreach_instr_safe(instr, block) {
      bool removed = false;
      if (instr->type != nir_instr_type_phi)
         break;

      removed = remove_phis_instr(block, instr, b);
      progress |= removed;
      if (!removed)
         progress |= phis_to_bools(block, instr, b);
   }

   return progress;
}

static bool
nir_opt_remove_phis_impl(nir_function_impl *impl)
{
   bool progress = false;
   nir_builder bld;
   nir_builder_init(&bld, impl);

   nir_foreach_block(block, impl) {
      progress |= remove_phis_block(block, &bld);
   }

   if (progress) {
      nir_metadata_preserve(impl, nir_metadata_block_index |
                                  nir_metadata_dominance);
   } else {
#ifndef NDEBUG
      impl->valid_metadata &= ~nir_metadata_not_properly_reset;
#endif
   }

   return progress;
}

bool
nir_opt_remove_phis(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function(function, shader)
      if (function->impl)
         progress = nir_opt_remove_phis_impl(function->impl) || progress;

   return progress;
}

