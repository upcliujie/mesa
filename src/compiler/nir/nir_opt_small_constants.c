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
#include "nir_deref.h"

#include "util/u_math.h"

struct small_arr {
   unsigned bits;
   unsigned bits_per_elem;
   uint64_t data;
};

static struct hash_table *
build_small_arr_table(const nir_shader *shader,
                      unsigned max_bit_size)
{
   assert(util_is_power_of_two_nonzero(max_bit_size));
   assert(max_bit_size <= 64);

   struct hash_table *table = _mesa_pointer_hash_table_create(NULL);

   nir_foreach_variable_with_modes(var, shader, nir_var_mem_constant) {
      if (!glsl_type_is_array(var->type))
         continue;

      const struct glsl_type *elem_type = glsl_get_array_element(var->type);
      if (!glsl_type_is_scalar(elem_type))
         continue;

      unsigned array_len = glsl_get_length(var->type);
      unsigned bit_size = glsl_get_bit_size(elem_type);

      /* If our array is large, don't even bother */
      if (array_len > max_bit_size)
         continue;

      unsigned used_bits = 0;
      const nir_constant *val = var->constant_initializer;
      for (unsigned i = 0; i < array_len; i++) {
         nir_const_value elem = val->elements[i]->values[0];
         uint64_t elem_u64 = nir_const_value_as_uint(elem, bit_size);
         if (elem_u64 == 0)
            continue;
         unsigned elem_bits = util_logbase2_64(elem_u64) + 1;
         used_bits = MAX2(used_bits, elem_bits);
      }

      /* Only use power-of-two numbers of bits so we end up with a shift
       * instead of a multiply on our index.
       */
      used_bits = util_next_power_of_two(used_bits);

      if (used_bits * array_len > max_bit_size)
         continue;

      struct small_arr *sa = rzalloc(table, struct small_arr);
      sa->bits_per_elem = used_bits;
      sa->bits = util_next_power_of_two(used_bits * array_len);
      sa->bits = MAX2(sa->bits, 8);

      for (unsigned i = 0; i < array_len; i++) {
         nir_const_value elem = val->elements[i]->values[0];
         uint64_t elem_u64 = nir_const_value_as_uint(elem, bit_size);
         sa->data |= elem_u64 << (i * used_bits);
      }

      _mesa_hash_table_insert(table, var, sa);
   }

   if (table->entries == 0) {
      ralloc_free(table);
      return NULL;
   } else {
      return table;
   }
}

static bool
opt_small_constants_instr(nir_builder *b, nir_instr *instr, void *_table)
{
   struct hash_table *table = _table;

   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *load = nir_instr_as_intrinsic(instr);
   if (load->intrinsic != nir_intrinsic_load_deref)
      return false;

   nir_deref_instr *deref = nir_src_as_deref(load->src[0]);
   if (!nir_deref_mode_is(deref, nir_var_mem_constant))
      return false;

   nir_deref_path path;
   nir_deref_path_init(&path, deref, table);

   nir_deref_instr *var_deref = path.path[0];
   if (var_deref->deref_type != nir_deref_type_var)
      return false;

   nir_deref_instr *arr_deref = path.path[1];
   if (arr_deref == NULL || arr_deref->deref_type != nir_deref_type_array)
      return false;

   if (path.path[2] != NULL)
      return false;

   struct hash_entry *entry = _mesa_hash_table_search(table, var_deref->var);
   if (entry == NULL)
      return false;

   const struct small_arr *sa = entry->data;

   assert(arr_deref->arr.index.is_ssa);
   b->cursor = nir_before_instr(instr);
   nir_ssa_def *imm = nir_imm_intN_t(b, sa->data, sa->bits);
   nir_ssa_def *shift = nir_imul_imm(b, arr_deref->arr.index.ssa,
                                        sa->bits_per_elem);
   nir_ssa_def *val = nir_ushr(b, imm, nir_u2u32(b, shift));
   val = nir_iand_imm(b, val, BITFIELD64_MASK(sa->bits_per_elem));
   val = nir_u2u(b, val, load->dest.ssa.bit_size);

   nir_ssa_def_rewrite_uses(&load->dest.ssa, nir_src_for_ssa(val));
   nir_instr_remove(&load->instr);

   return true;
}

/* Converts small constant arrays into shifts
 *
 * Say we have a little array like this:
 *
 *    __constant const uint a[8] = {1, 0, 3, 2, 5, 4, 7, 6};
 *
 * Then we can turn this:
 *
 *    uint e = a[i];
 *
 * into this:
 *
 *    uint e = (0x67452301 >> (i << 2h)) & 0xf;
 */
bool
nir_opt_small_constants(nir_shader *shader, unsigned max_bit_size)
{
   struct hash_table *table = build_small_arr_table(shader, max_bit_size);
   if (table == NULL)
      return false;

   bool progress =
      nir_shader_instructions_pass(shader, opt_small_constants_instr,
                                   nir_metadata_block_index |
                                   nir_metadata_dominance,
                                   table);
   ralloc_free(table);
   return progress;
}
