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
 */


#include "nir.h"
#include "nir_builder.h"
#include "nir_control_flow.h"

static void
lower_loop_continue_block(nir_builder *b, nir_loop *loop)
{
   nir_block *header = nir_loop_first_block(loop);
   nir_block *cont = nir_loop_first_continue_block(loop);

   /* count continue statements excluding unreachable ones */
   nir_block *single_predecessor = NULL;
   set_foreach(cont->predecessors, entry) {
      nir_block *pred = (nir_block*) entry->key;
      if (pred->predecessors->entries == 0)
         continue;
      if (single_predecessor == NULL) {
         single_predecessor = pred;
      } else {
         single_predecessor = NULL;
         break;
      }
   }

   nir_lower_phis_to_regs_block(header);

   if (single_predecessor) {
      /* inline continue block */
      nir_cf_list extracted;
      nir_cf_list_extract(&extracted, &loop->continue_list);
      nir_cf_reinsert(&extracted, nir_after_block_before_jump(single_predecessor));
   } else {
      nir_lower_phis_to_regs_block(cont);

      /* insert the continue block at the begining of the loop */
      nir_variable *do_cont =
         nir_local_variable_create(b->impl, glsl_bool_type(), "cont");

      b->cursor = nir_before_cf_node(&loop->cf_node);
      nir_store_var(b, do_cont, nir_imm_false(b), 1);
      b->cursor = nir_before_block(header);
      nir_if *cont_if = nir_push_if(b, nir_load_var(b, do_cont));

      nir_cf_list extracted;
      nir_cf_list_extract(&extracted, &loop->continue_list);
      nir_cf_reinsert(&extracted, nir_before_cf_list(&cont_if->then_list));

      nir_pop_if(b, cont_if);
      nir_store_var(b, do_cont, nir_imm_true(b), 1);
   }

   /* change predecessors and successors */
   header = nir_loop_first_block(loop);
   cont = nir_loop_first_continue_block(loop);

   _mesa_set_remove_key(header->predecessors, cont);
   nir_loop_first_continue_block(loop)->successors[0] = NULL;

   set_foreach(cont->predecessors, entry) {
      nir_block *pred = (nir_block*) entry->key;
      pred->successors[0] = header;
      _mesa_set_add(header->predecessors, pred);
   }

   exec_node_remove(exec_list_get_head(&loop->continue_list));
}


static bool
visit_cf_list(nir_builder *b, struct exec_list *list)
{
   bool progress = false;

   foreach_list_typed(nir_cf_node, node, node, list) {
      switch (node->type) {
      case nir_cf_node_block:
         continue;
      case nir_cf_node_if: {
         nir_if *nif = nir_cf_node_as_if(node);
         progress |= visit_cf_list(b, &nif->then_list);
         progress |= visit_cf_list(b, &nif->else_list);
         break;
      }
      case nir_cf_node_loop: {
         nir_loop *loop = nir_cf_node_as_loop(node);
         visit_cf_list(b, &loop->body);
         visit_cf_list(b, &loop->continue_list);
         lower_loop_continue_block(b, loop);
         progress = true;
         break;
      }
      case nir_cf_node_function:
         unreachable("NIR divergence analysis: Unsupported cf_node type.");
      }
   }

   return progress;
}

bool
nir_lower_continue_blocks(nir_shader *shader)
{
   bool progress = false;
   nir_foreach_function(function, shader) {
      if (function->impl == NULL)
         continue;

      nir_builder b;
      nir_builder_init(&b, function->impl);
      progress |= visit_cf_list(&b, &function->impl->body);
      if (progress)
         nir_lower_regs_to_ssa_impl(function->impl);

   }
   if (progress)
      nir_repair_ssa(shader);

   return progress;
}

