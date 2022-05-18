/*
 * Copyright © 2022 Konstantin Seurer
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

/** @file nir_opt_ray_query_ranges.c
 *
 * Merge ray queries that are not used in parallel to reduce required scratch
 * memory and improve locality. 
 */

struct rq_range {
   nir_variable *variable;

   uint32_t first;
   uint32_t last;

   uint32_t new_index;
};

#define RQ_NEW_INDEX_NONE 0xFFFFFFFF

static bool
count_ranges(struct nir_builder *b, nir_instr *instr, void *data)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrinsic = nir_instr_as_intrinsic(instr);
   if (intrinsic->intrinsic == nir_intrinsic_rq_initialize)
      (*(uint32_t *) data)++;

   return false;
}

bool
nir_opt_ray_query_ranges(nir_shader *shader)
{
   bool progress = false;

   assert(exec_list_length(&shader->functions) == 1);

   struct nir_function *func =
      (struct nir_function *)exec_list_get_head_const(&shader->functions);
   assert(func->impl);

   nir_metadata_require(func->impl, nir_metadata_instr_index);

   nir_variable **ray_queries = calloc(shader->info.ray_queries, sizeof(nir_variable*));
   uint32_t ray_query_count = 0;

   nir_foreach_variable_in_shader(var, shader) {
      if (!var->data.ray_query)
         continue;
      
      ray_queries[ray_query_count] = var;
      ray_query_count++;
   }

   nir_foreach_function_temp_variable(var, func->impl) {
      if (!var->data.ray_query)
         continue;
      
      ray_queries[ray_query_count] = var;
      ray_query_count++;
   }

   uint32_t range_count = 0;
   nir_shader_instructions_pass(shader, count_ranges, nir_metadata_all, &range_count);

   struct rq_range *ranges = calloc(range_count, sizeof(struct rq_range));

   struct hash_table *range_indices = _mesa_pointer_hash_table_create(NULL);
   uint32_t target_index = 0;

   nir_foreach_block(block, func->impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intrinsic = nir_instr_as_intrinsic(instr);
         if (!nir_intrinsic_is_rq(intrinsic->intrinsic))
            continue;

         nir_deref_instr *ray_query_deref =
            nir_instr_as_deref(intrinsic->src[0].ssa->parent_instr);

         assert(ray_query_deref->deref_type == nir_deref_type_var);

         struct hash_entry *index_entry =
            _mesa_hash_table_search(range_indices, ray_query_deref->var);

         if (!index_entry || intrinsic->intrinsic == nir_intrinsic_rq_initialize) {
            assert(intrinsic->intrinsic == nir_intrinsic_rq_initialize);

            _mesa_hash_table_insert(range_indices, ray_query_deref->var,
                                    (void *)(uintptr_t)target_index);

            ranges[target_index].variable = ray_query_deref->var;
            ranges[target_index].first = instr->index;
            ranges[target_index].last = instr->index;
            ranges[target_index].new_index = RQ_NEW_INDEX_NONE;

            target_index++;

            continue;
         }

         ranges[(uintptr_t)index_entry->data].last = instr->index;

         if (intrinsic->intrinsic == nir_intrinsic_rq_terminate)
            _mesa_hash_table_remove(range_indices, index_entry);
      }
   }

   assert(target_index == range_count);

   /* Try to push ray query ranges 'down'. */
   for (uint32_t rq_index = 1; rq_index < shader->info.ray_queries; rq_index++) {
      for (uint32_t range_index = 0; range_index < range_count; range_index++) {
         if (ranges[range_index].variable != ray_queries[rq_index])
            continue;

         uint32_t lowest_index = RQ_NEW_INDEX_NONE;

         for (uint32_t dom_rq_index = 0; dom_rq_index < rq_index; dom_rq_index++) {
            bool collides = false;

            for (uint32_t dom_range_index = 0; dom_range_index < range_count; dom_range_index++) {
               if (ranges[dom_range_index].variable != ray_queries[dom_rq_index])
                  continue;

               if (!(ranges[dom_range_index].first > ranges[range_index].last ||
                     ranges[dom_range_index].last < ranges[range_index].first)) {
                  collides = true;
                  break;
               }
            }

            if (!collides) {
               lowest_index = dom_rq_index;
               break;
            }
         }

         ranges[range_index].new_index = lowest_index;
      }
   }

   nir_foreach_block(block, func->impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intrinsic = nir_instr_as_intrinsic(instr);
         if (!nir_intrinsic_is_rq(intrinsic->intrinsic))
            continue;

         nir_deref_instr *ray_query_deref =
            nir_instr_as_deref(intrinsic->src[0].ssa->parent_instr);

         for (uint32_t range_index = 0; range_index < range_count; range_index++) {
            if (ranges[range_index].variable != ray_query_deref->var ||
                ranges[range_index].first > instr->index ||
                ranges[range_index].last < instr->index)
               continue;
            
            if (ranges[range_index].new_index == RQ_NEW_INDEX_NONE)
               break;

            ray_query_deref->var = ray_queries[ranges[range_index].new_index];
         }
      }
   }

   ralloc_free(range_indices);
   free(ranges);
   free(ray_queries);

   /* Update the number of queries if some have been removed. */
   if (progress) {
      nir_remove_dead_variables(shader,
                                nir_var_shader_temp | nir_var_function_temp,
                                NULL);
      nir_shader_gather_info(shader, nir_shader_get_entrypoint(shader));

   }

   return progress;
}
