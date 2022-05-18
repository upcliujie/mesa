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
   nir_variable *new_variable;

   uint32_t first;
   uint32_t last;

   nir_instr *init;
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

static nir_block *
get_parent_loop_body(const nir_cf_node *node)
{
   nir_block *candidate_block = NULL;
   nir_block *result = NULL;

   while (node) {
      if (node->type == nir_cf_node_block)
         candidate_block = nir_cf_node_as_block(node);
      else if (node->type == nir_cf_node_loop)
         result = candidate_block;

      node = node->parent;
   }

   return result;
}

bool
nir_opt_ray_query_ranges(nir_shader *shader)
{
   assert(exec_list_length(&shader->functions) == 1);

   struct nir_function *func =
      (struct nir_function *)exec_list_get_head_const(&shader->functions);
   assert(func->impl);

   nir_metadata_require(func->impl, nir_metadata_instr_index | nir_metadata_dominance);

   nir_variable **ray_queries = ralloc_array(NULL, nir_variable*, shader->info.ray_queries);
   uint32_t ray_query_count = 0;

   nir_foreach_variable_in_shader(var, shader) {
      if (!var->data.ray_query || glsl_type_is_array(var->type))
         continue;
      
      ray_queries[ray_query_count] = var;
      ray_query_count++;
   }

   nir_foreach_function_temp_variable(var, func->impl) {
      if (!var->data.ray_query || glsl_type_is_array(var->type))
         continue;
      
      ray_queries[ray_query_count] = var;
      ray_query_count++;
   }

   uint32_t range_count = 0;
   nir_shader_instructions_pass(shader, count_ranges, nir_metadata_all, &range_count);

   struct rq_range *ranges = ralloc_array(ray_queries, struct rq_range, range_count);

   struct hash_table *range_indices = _mesa_pointer_hash_table_create(ray_queries);
   uint32_t target_index = 0;

   nir_foreach_block(block, func->impl) {
      /* Compute loop information on demand and reuse it for other
       * ray query intrinsics in the same block.
       */
      bool has_loop_info = false;
      bool is_inside_loop = false;
      uint32_t loop_first = UINT32_MAX;
      uint32_t loop_last = 0;

      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intrinsic = nir_instr_as_intrinsic(instr);
         if (!nir_intrinsic_is_ray_query(intrinsic->intrinsic))
            continue;

         nir_deref_instr *ray_query_deref =
            nir_instr_as_deref(intrinsic->src[0].ssa->parent_instr);

         if (ray_query_deref->deref_type != nir_deref_type_var)
            continue;

         if (intrinsic->intrinsic == nir_intrinsic_rq_initialize) {
            _mesa_hash_table_insert(range_indices, ray_query_deref->var,
                                    (void *)(uintptr_t)target_index);

            ranges[target_index].variable = ray_query_deref->var;
            ranges[target_index].new_variable = NULL;
            ranges[target_index].first = instr->index;
            ranges[target_index].last = instr->index;
            ranges[target_index].init = instr;

            target_index++;
         }

         struct hash_entry *index_entry =
            _mesa_hash_table_search(range_indices, ray_query_deref->var);

         if (!index_entry)
            continue;

         struct rq_range *range = &ranges[(uintptr_t)index_entry->data];
         
         if (intrinsic->intrinsic != nir_intrinsic_rq_initialize) {
            /* If the initialize instruction does not dominate every other
             * instruction in the range, we have to reject the enire query
             * since we can not be certain about the ranges:
             * 
             * rayQuery rq;
             * if (i == 0)
             *    init(rq);
             * ...             <-- Another ray query that would get merged.
             * if (i == 1)
             *    init(rq);    <----+
             * if (i == 0)          |
             *    proceed(rq); <-- Not dominated by init!
             * if (i == 1)
             *    proceed(rq);
             */

            if (!nir_block_dominates(range->init->block, instr->block)) {
               _mesa_hash_table_remove(range_indices, index_entry);

               for (uint32_t i = 0; i < shader->info.ray_queries; i++) {
                  if (ray_queries[i] == ray_query_deref->var) {
                     ray_queries[i] = NULL;
                     break;
                  }
               }

               continue;
            }
         }

         range->last = MAX2(range->last, instr->index);

         if (!has_loop_info) {
            has_loop_info = true;

            nir_block *loop_block = get_parent_loop_body(&block->cf_node);
            if (loop_block) {
               is_inside_loop = true;

               nir_foreach_instr(loop_instr, loop_block) {
                  loop_first = MIN2(loop_first, loop_instr->index);
                  loop_last = MAX2(loop_last, loop_instr->index);
               }
            }
         }

         if (is_inside_loop) {
            range->first = MIN2(range->first, loop_first);
            range->last = MAX2(range->last, loop_last);
         }
      }
   }

   assert(target_index == range_count);

   bool progress = false;

   /* Try to push ray query ranges 'down'. */
   for (uint32_t rq_index = 1; rq_index < shader->info.ray_queries; rq_index++) {
      if (!ray_queries[rq_index])
         continue;

      for (uint32_t dom_rq_index = 0; dom_rq_index < rq_index; dom_rq_index++) {
         if (!ray_queries[dom_rq_index])
            continue;

         bool collides = false;

         for (uint32_t range_index = 0; range_index < range_count; range_index++) {
            if (ranges[range_index].variable != ray_queries[rq_index] &&
                ranges[range_index].new_variable != ray_queries[rq_index])
               continue;

            for (uint32_t dom_range_index = 0; dom_range_index < range_count; dom_range_index++) {
               if (ranges[dom_range_index].variable != ray_queries[dom_rq_index] &&
                   ranges[dom_range_index].new_variable != ray_queries[dom_rq_index])
                  continue;

               if (!(ranges[dom_range_index].first > ranges[range_index].last ||
                     ranges[dom_range_index].last < ranges[range_index].first)) {
                  collides = true;
                  break;
               }
            }

            if (collides)
               break;
         }

         if (collides)
            continue;

         for (uint32_t range_index = 0; range_index < range_count; range_index++) {
            if (ranges[range_index].variable != ray_queries[rq_index] &&
                ranges[range_index].new_variable != ray_queries[rq_index])
               continue;

            ranges[range_index].new_variable = ray_queries[dom_rq_index];
         }
      }
   }

   /* Remap the ray query derefs to the new variables. */
   nir_foreach_block(block, func->impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intrinsic = nir_instr_as_intrinsic(instr);
         if (!nir_intrinsic_is_ray_query(intrinsic->intrinsic))
            continue;

         nir_deref_instr *ray_query_deref =
            nir_instr_as_deref(intrinsic->src[0].ssa->parent_instr);

         if (ray_query_deref->deref_type != nir_deref_type_var)
            continue;

         for (uint32_t range_index = 0; range_index < range_count; range_index++) {
            if (ranges[range_index].variable != ray_query_deref->var ||
                ranges[range_index].first > instr->index ||
                ranges[range_index].last < instr->index)
               continue;
            
            if (ranges[range_index].new_variable)
               ray_query_deref->var = ranges[range_index].new_variable;
         }
      }
   }

   nir_metadata_preserve(func->impl, nir_metadata_all);

   /* Update the number of queries if some have been removed. */
   if (progress) {
      nir_remove_dead_variables(shader, nir_var_shader_temp | nir_var_function_temp,
                                NULL);
   }

   ralloc_free(ray_queries);

   return progress;
}
