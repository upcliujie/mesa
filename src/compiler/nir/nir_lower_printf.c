/*
 * Copyright © 2020 Microsoft Corporation
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
#include "nir_builder_opcodes.h"

#include "util/u_math.h"

static const struct glsl_type *
get_format_string_id_type(int bit_size)
{
   switch (bit_size) {
   case 8: return glsl_int8_t_type();
   case 16: return glsl_int16_t_type();
   case 32: return glsl_int_type();
   case 64: return glsl_int64_t_type();
   default: unreachable("Unexpected bit size");
   }
}

static void
lower_printf_impl(nir_builder *b, nir_intrinsic_instr *instr, const nir_lower_printf_options *options)
{
   /* Atomic add a buffer size counter to determine where to write.
    * If overflowed, return -1, otherwise, store the arguments and return 0.
    */
   b->cursor = nir_before_instr(&instr->instr);
   nir_deref_instr *base_deref = nir_build_deref_cast(b,
                                                      nir_load_printf_buffer_address(b, nir_get_ptr_bitsize(b->shader)),
                                                      nir_var_mem_global,
                                                      glsl_array_type(glsl_uint_type(), 0, 4),
                                                      sizeof(int));
   nir_deref_instr *counter_deref = nir_build_deref_array_imm(b, base_deref, 0);
   nir_deref_instr *struct_deref = nir_instr_as_deref(instr->src[1].ssa->parent_instr);
   nir_variable *struct_var = nir_deref_instr_get_variable(struct_deref);
   const struct glsl_type *struct_type = struct_var->type;

   /* Align the struct size to 4 */
   int struct_size = align(glsl_get_cl_size(struct_type), 4);
   int format_string_id_size = 4;

   /* Increment the counter at the beginning of the buffer */
   nir_intrinsic_instr *atomic = nir_intrinsic_instr_create(b->shader, nir_intrinsic_deref_atomic_add);
   nir_ssa_dest_init(&atomic->instr, &atomic->dest, 1, 32, NULL);
   atomic->src[0] = nir_src_for_ssa(&counter_deref->dest.ssa);
   atomic->src[1] = nir_src_for_ssa(nir_imm_int(b, struct_size + format_string_id_size));
   nir_builder_instr_insert(b, &atomic->instr);

   /* Check if we're still in-bounds */
   const unsigned default_buffer_size = 1024 * 1024;
   unsigned buffer_size = (options && options->max_buffer_size) ? options->max_buffer_size : default_buffer_size;
   int max_valid_offset =
      buffer_size -
      struct_size -
      format_string_id_size -
      sizeof(int); /* the first int in the buffer is for the counter */
   nir_push_if(b, nir_ilt(b, &atomic->dest.ssa, nir_imm_int(b, max_valid_offset)));
   nir_ssa_def *printf_succ_val = nir_imm_int(b, 0);

   /* Write the format string ID */
   nir_ssa_def *start_offset = nir_u2u64(b, nir_iadd(b, &atomic->dest.ssa, nir_imm_int(b, sizeof(int))));
   nir_deref_instr *as_byte_array = nir_build_deref_cast(b, &base_deref->dest.ssa, nir_var_mem_global, glsl_uint8_t_type(), 1);
   nir_deref_instr *as_offset_byte_array = nir_build_deref_ptr_as_array(b, as_byte_array, start_offset);
   nir_deref_instr *format_string_write_deref =
      nir_build_deref_cast(b, &as_offset_byte_array->dest.ssa, nir_var_mem_global,
                           get_format_string_id_type(instr->src[0].ssa->bit_size), format_string_id_size);
   format_string_write_deref->cast.align_mul = sizeof(int);
   nir_store_deref(b, format_string_write_deref, instr->src[0].ssa, ~0);

   /* Write the format args */
   for (unsigned i = 0; i < glsl_get_length(struct_type); ++i) {
      nir_ssa_def *field_offset_from_start = nir_imm_int64(b, glsl_get_struct_field_offset(struct_type, i) + sizeof(int));
      nir_ssa_def *field_offset = nir_iadd(b, start_offset, field_offset_from_start);

      const struct glsl_type *field_type = glsl_get_struct_field(struct_type, i);
      nir_deref_instr *field_read_deref = nir_build_deref_struct(b, struct_deref, i);
      nir_ssa_def *field_value = nir_load_deref(b, field_read_deref);

      /* Clang does promotion of arguments to their "native" size. That means that any floats
       * have been converted to doubles for the call to printf. Since doubles are optional,
       * some drivers might not support them. For those drivers, convert them back to float
       * before writing. Copy prop and other optimizations should remove all hints of doubles.
       */
      if (glsl_get_base_type(field_type) == GLSL_TYPE_DOUBLE &&
          options && options->treat_doubles_as_floats) {
         field_value = nir_f2f32(b, field_value);
         field_type = glsl_float_type();
      }

      as_offset_byte_array = nir_build_deref_ptr_as_array(b, as_byte_array, field_offset);
      nir_deref_instr *field_write_deref =
         nir_build_deref_cast(b, &as_offset_byte_array->dest.ssa, nir_var_mem_global, field_type, glsl_get_cl_size(field_type));
      field_write_deref->cast.align_mul = sizeof(int);
      field_write_deref->cast.align_offset = glsl_get_struct_field_offset(struct_type, i) % sizeof(int);

      nir_store_deref(b, field_write_deref, field_value, ~0);
   }

   nir_push_else(b, NULL);
   nir_ssa_def *printf_fail_val = nir_imm_int(b, -1);
   nir_pop_if(b, NULL);

   nir_ssa_def *return_value = nir_if_phi(b, printf_succ_val, printf_fail_val);
   nir_ssa_def_rewrite_uses(&instr->dest.ssa, nir_src_for_ssa(return_value));
   nir_instr_remove(&instr->instr);
}

bool
nir_lower_printf(nir_shader *nir, const nir_lower_printf_options *options)
{
   bool progress = false;

   nir_foreach_function(func, nir) {
      if (!func->impl)
         continue;

      nir_foreach_block(block, func->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *intrinsic = nir_instr_as_intrinsic(instr);
            if (intrinsic->intrinsic != nir_intrinsic_printf)
               continue;

            nir_builder b;
            nir_builder_init(&b, func->impl);
            lower_printf_impl(&b, intrinsic, options);
            progress = true;
         }
      }

      if (progress) {
         nir_metadata_preserve(func->impl, nir_metadata_dominance);
      }
   }

   return progress;
}
