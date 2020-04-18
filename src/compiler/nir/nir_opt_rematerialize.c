/*
 * Copyright © 2019 Intel Corporation
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
#include "util/hash_table.h"

struct remat_state {
   bool progress;

   nir_move_options options;

   nir_shader *shader;
   nir_block *block;

   struct hash_table *clones;
};

static nir_ssa_def *
rematerialize_for_src(nir_src *src, bool is_if_condition,
                      struct remat_state *state)
{
   /* Don't bother cloning an instruction in our block */
   if (src->ssa->parent_instr->block == state->block)
      return NULL;

   assert(src->is_ssa);
   if (!nir_can_move_instr(src->ssa->parent_instr, state->options))
      return NULL;

   /* This only works for ALU instructions for now */
   nir_alu_instr *src_alu = nir_instr_as_alu(src->ssa->parent_instr);

   nir_alu_instr *clone;
   struct hash_entry *block_entry =
      _mesa_hash_table_search(state->clones, src_alu);
   if (block_entry) {
      clone = block_entry->data;
   } else {
      clone = nir_alu_instr_clone(state->shader, src_alu);
      nir_instr_insert(nir_before_src(src, is_if_condition), &clone->instr);
      _mesa_hash_table_insert(state->clones, src_alu, clone);
   }

   return &clone->dest.dest.ssa;
}

static bool
rematerialize_at_instr_src(nir_src *src, void *_state)
{
   struct remat_state *state = _state;

   nir_ssa_def *remat = rematerialize_for_src(src, false, state);
   if (remat != NULL) {
      nir_instr_rewrite_src(src->parent_instr, src, nir_src_for_ssa(remat));
      state->progress = true;
   }

   return true;
}

static bool
rematerialize_at_if_src(nir_if *if_stmt, struct remat_state *state)
{
   nir_ssa_def *remat = rematerialize_for_src(&if_stmt->condition, true, state);
   if (remat != NULL) {
      nir_if_rewrite_condition(if_stmt, nir_src_for_ssa(remat));
      state->progress = true;
   }

   return true;
}

static bool
nir_opt_rematerialize_impl(nir_function_impl *impl, nir_move_options options)
{
   struct remat_state state = {
      .options = options,
      .shader = impl->function->shader,
   };

   nir_foreach_block(block, impl) {
      state.block = block;

      if (state.clones == NULL)
         state.clones = _mesa_pointer_hash_table_create(NULL);
      else
         _mesa_hash_table_clear(state.clones, NULL);

      nir_foreach_instr_safe(instr, block)
         nir_foreach_src(instr, rematerialize_at_instr_src, &state);

      nir_if *if_stmt = nir_block_get_following_if(block);
      if (if_stmt)
         rematerialize_at_if_src(if_stmt, &state);
   }

   _mesa_hash_table_destroy(state.clones, NULL);

   if (state.progress) {
      nir_metadata_preserve(impl, nir_metadata_block_index |
                                  nir_metadata_dominance);
   }

   return state.progress;
}

bool
nir_opt_rematerialize(nir_shader *shader, nir_move_options options)
{
   bool progress = false;

   nir_foreach_function(func, shader) {
      if (func->impl && nir_opt_rematerialize_impl(func->impl, options))
         progress = true;
   }

   return progress;
}
