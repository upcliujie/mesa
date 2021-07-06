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

/*
 * This pass tries to create more shared atomic operations.
 */

#include "nir/nir.h"
#include "nir/nir_builder.h"

static nir_intrinsic_op
get_shared_atomic_op_from_alu(nir_alu_instr *src_alu)
{
#define OP(alu, atomic) \
   case nir_op_##alu: return nir_intrinsic_shared_atomic_##atomic
   switch (src_alu->op) {
   OP(iadd, add);
   OP(imin, imin);
   OP(umin, umin);
   OP(imax, imax);
   OP(umax, umax);
   OP(iand, and);
   OP(ior, or);
   OP(ixor, xor);
   OP(fadd, fadd);
   OP(fmin, fmin);
   OP(fmax, fmax);
   default:
      unreachable("Invalid ALU opcode");
   }
#undef OP
}

static bool
can_use_shared_atomic(nir_alu_instr *src_alu,
                      const nir_opt_shared_atomics_options *options)
{
   uint8_t bit_size = src_alu->dest.dest.ssa.bit_size;

   if ((bit_size == 32 || bit_size == 64) &&
       (src_alu->op == nir_op_iadd ||
        src_alu->op == nir_op_imin ||
        src_alu->op == nir_op_umin ||
        src_alu->op == nir_op_imax ||
        src_alu->op == nir_op_umax ||
        src_alu->op == nir_op_iand ||
        src_alu->op == nir_op_ior ||
        src_alu->op == nir_op_ixor))
      return true;

   if (options->has_float32_atomic_add &&
       bit_size == 32 && src_alu->op == nir_op_fadd)
      return true;

   if (((options->has_float32_atomic_min_max && bit_size == 32) ||
        (options->has_float64_atomic_min_max && bit_size == 64)) &&
       (src_alu->op == nir_op_fmin || src_alu->op == nir_op_fmax))
      return true;

   return false;
}

static bool
opt_shared_atomics(nir_function_impl *impl,
                   const nir_opt_shared_atomics_options *options)
{
   bool progress = false;
   nir_builder b;
   nir_builder_init(&b, impl);

   nir_foreach_block_safe(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *store_shared = nir_instr_as_intrinsic(instr);
         if (store_shared->intrinsic != nir_intrinsic_store_shared)
            continue;

         /* Only scalars are supported for now. */
         if (nir_intrinsic_write_mask(store_shared) != 0x1)
            continue;

         nir_ssa_def *data = store_shared->src[0].ssa;
         if (data->parent_instr->type != nir_instr_type_alu)
            continue;

         /* Determine if the ALU operation could be replaced with an atomic. */
         nir_alu_instr *src_alu = nir_instr_as_alu(data->parent_instr);
         if (!can_use_shared_atomic(src_alu, options))
            continue;

         nir_ssa_def *alu_src0 = src_alu->src[0].src.ssa;
         nir_ssa_def *alu_src1 = src_alu->src[1].src.ssa;

         /* Both ALU sources must come from nir_intrinsic_load_shared. */
         if (alu_src0->parent_instr->type != nir_instr_type_intrinsic ||
             nir_instr_as_intrinsic(alu_src0->parent_instr)->intrinsic != nir_intrinsic_load_shared ||
             alu_src1->parent_instr->type != nir_instr_type_intrinsic ||
             nir_instr_as_intrinsic(alu_src1->parent_instr)->intrinsic != nir_intrinsic_load_shared)
            continue;

         nir_intrinsic_instr *load_shared_src0 = nir_instr_as_intrinsic(alu_src0->parent_instr);
         nir_intrinsic_instr *load_shared_src1 = nir_instr_as_intrinsic(alu_src1->parent_instr);

         if (load_shared_src0->num_components != 1 ||
             load_shared_src1->num_components != 1)
            continue;

         nir_ssa_def *found_alu_src = NULL;

         if (nir_srcs_equal(store_shared->src[1], load_shared_src0->src[0]) &&
             nir_intrinsic_base(store_shared) == nir_intrinsic_base(load_shared_src0)) {
             found_alu_src = alu_src1;
         } else if (nir_srcs_equal(store_shared->src[1], load_shared_src1->src[0]) &&
                    nir_intrinsic_base(store_shared) == nir_intrinsic_base(load_shared_src1)) {
            found_alu_src = alu_src0;
         }

         if (!found_alu_src)
            continue;

         b.cursor = nir_before_instr(&store_shared->instr);

         /* Replace the shared store by an atomic shared operation. */
         uint8_t bit_size = store_shared->src[0].ssa->bit_size;
         nir_intrinsic_op atomic_op = get_shared_atomic_op_from_alu(src_alu);

         nir_intrinsic_instr *atomic_shared = nir_intrinsic_instr_create(b.shader, atomic_op);
         nir_ssa_dest_init(&atomic_shared->instr, &atomic_shared->dest, 1, bit_size, NULL);
         atomic_shared->src[0] = nir_src_for_ssa(store_shared->src[1].ssa);
         atomic_shared->src[1] = nir_src_for_ssa(found_alu_src);
         nir_intrinsic_set_base(atomic_shared, nir_intrinsic_base(store_shared));
         nir_builder_instr_insert(&b, &atomic_shared->instr);

         nir_instr_remove(&store_shared->instr);

         progress = true;
      }
   }

   return progress;
}

bool
nir_opt_shared_atomics(nir_shader *shader,
                       const nir_opt_shared_atomics_options *options)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (!function->impl)
         continue;

      if (opt_shared_atomics(function->impl, options)) {
         progress = true;
         nir_metadata_preserve(function->impl, 0);
      } else {
         nir_metadata_preserve(function->impl, nir_metadata_all);
      }
   }

   return progress;
}
