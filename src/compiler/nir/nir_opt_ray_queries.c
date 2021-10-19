/*
 * Copyright © 2021 Intel Corporation
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
#include "nir_builder.h"

#include "util/hash_table.h"
#include "util/macros.h"

/** @file nir_opt_ray_queries.c
 *
 * Remove ray queries that the shader is not using the result of.
 */

enum query_state
{
   QUERY_STATE_INITIALIZED = BITFIELD_BIT(0),
   QUERY_STATE_TERMINATED  = BITFIELD_BIT(1),
   QUERY_STATE_PROCEEDED   = BITFIELD_BIT(2),
   QUERY_STATE_READ        = BITFIELD_BIT(3),
   QUERY_STATE_WRITTEN     = BITFIELD_BIT(4),
};

static void
mark_query(struct hash_table *queries,
           nir_intrinsic_instr *intrin,
           enum query_state new_state)
{
   nir_ssa_def *rq_def = intrin->src[0].ssa;

   nir_variable *query;
   if (rq_def->parent_instr->type == nir_instr_type_intrinsic) {
      nir_intrinsic_instr *load_deref =
         nir_instr_as_intrinsic(rq_def->parent_instr);
      assert(load_deref->intrinsic == nir_intrinsic_load_deref);

      query = nir_intrinsic_get_var(load_deref, 0);
   } else if (rq_def->parent_instr->type == nir_instr_type_deref) {
      query = nir_deref_instr_get_variable(
         nir_instr_as_deref(rq_def->parent_instr));
   } else {
      return;
   }
   assert(query);

   struct hash_entry *entry =
      _mesa_hash_table_search(queries, query);

   if (!entry)
      entry = _mesa_hash_table_insert(queries, query, NULL);

   entry->data = (void *)((uintptr_t) entry->data | (uintptr_t) new_state);
}

static struct hash_table *
nir_find_ray_queries(nir_shader *shader)
{
   struct hash_table *queries =
      _mesa_pointer_hash_table_create(NULL);

   nir_foreach_function(function, shader) {
      nir_function_impl *impl = function->impl;

      if (!impl)
         continue;

      nir_foreach_block(block, impl) {
         nir_foreach_instr(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
            switch (intrin->intrinsic) {
            case nir_intrinsic_rq_initialize:
               mark_query(queries, intrin, QUERY_STATE_INITIALIZED);
               break;
            case nir_intrinsic_rq_proceed:
               if (list_length(&intrin->dest.ssa.uses) > 0 ||
                   list_length(&intrin->dest.ssa.if_uses) > 0)
                  mark_query(queries, intrin, QUERY_STATE_READ);
               mark_query(queries, intrin, QUERY_STATE_PROCEEDED);
               break;
            case nir_intrinsic_rq_terminate:
               mark_query(queries, intrin, QUERY_STATE_TERMINATED);
               break;
            case nir_intrinsic_rq_load:
               mark_query(queries, intrin, QUERY_STATE_READ);
               break;
            case nir_intrinsic_rq_generate_intersection:
            case nir_intrinsic_rq_confirm_intersection:
               mark_query(queries, intrin, QUERY_STATE_WRITTEN);
               break;
            default:
               break;
            }
         }
      }
   }

   return queries;
}

static bool
nir_replace_unproceed_queries(nir_builder *b, nir_instr *instr, void *data)
{
   struct hash_table *queries = data;

   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   bool has_dest = false;
   switch (intrin->intrinsic) {
   case nir_intrinsic_rq_initialize:
   case nir_intrinsic_rq_terminate:
   case nir_intrinsic_rq_generate_intersection:
   case nir_intrinsic_rq_confirm_intersection:
      break;
   case nir_intrinsic_rq_load:
      has_dest = true;
      break;
   case nir_intrinsic_rq_proceed:
      break;
   default:
      return false;
   }

   nir_variable *query = nir_intrinsic_get_var(intrin, 0);
   assert(query);

   struct hash_entry *entry =
      _mesa_hash_table_search(queries, query);
   assert(entry);

   enum query_state state = (enum query_state) (uintptr_t) entry->data;
   if (state & QUERY_STATE_READ)
      return false;

   assert(!has_dest);

   if (intrin->intrinsic == nir_intrinsic_rq_load) {
      assert(list_is_empty(&intrin->dest.ssa.uses));
      assert(list_is_empty(&intrin->dest.ssa.if_uses));
   }

   nir_instr_remove(instr);

   return true;
}

bool
nir_opt_ray_queries(nir_shader *shader)
{
   struct hash_table *queries =
      nir_find_ray_queries(shader);
   bool progress = false;

   if (_mesa_hash_table_num_entries(queries) > 0) {
      progress =
         nir_shader_instructions_pass(shader,
                                      nir_replace_unproceed_queries,
                                      nir_metadata_block_index |
                                      nir_metadata_dominance,
                                      queries);

      /* Update the number of queries if some have been removed. */
      if (progress) {
         nir_remove_dead_derefs(shader);
         nir_remove_dead_variables(shader,
                                   nir_var_shader_temp | nir_var_function_temp,
                                   NULL);
         nir_shader_gather_info(shader, nir_shader_get_entrypoint(shader));
      }
   }

   _mesa_hash_table_destroy(queries, NULL);

   return progress;
}
