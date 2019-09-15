/*
 * Copyright © 2018 Intel Corporation
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

static bool
assert_ssa_def_is_not_1bit(nir_ssa_def *def, UNUSED void *unused)
{
   assert(def->bit_size > 1);
   return true;
}

static bool
rewrite_1bit_ssa_def_to_Nbit(nir_ssa_def *def, void *bit_size)
{
   if (def->bit_size == 1) {
      def->bit_size = (size_t) bit_size;
      return false;
   }
   return true;
}

static bool
lower_alu_instr(nir_builder *b, nir_alu_instr *alu, unsigned bit_size)
{
   const nir_op_info *op_info = &nir_op_infos[alu->op];

   b->cursor = nir_before_instr(&alu->instr);

   /* Replacement SSA value */
   nir_ssa_def *rep = NULL;
   switch (alu->op) {
   case nir_op_mov:
   case nir_op_vec2:
   case nir_op_vec3:
   case nir_op_vec4:
      /* These we expect to have booleans but the opcode doesn't change */
      break;

   case nir_op_b2f32:
      alu->op = bit_size == 16 ? nir_op_f2f32 : nir_op_mov;
      break;
   case nir_op_b2f16:
      alu->op = bit_size == 16 ? nir_op_mov : nir_op_f2f16;
      break;
   case nir_op_f2b1:
      rep = nir_sne(b, nir_ssa_for_alu_src(b, alu, 0),
                    nir_imm_floatN_t(b, 0.0, alu->src[0].src.ssa->bit_size));
      rep = nir_f2f(b, rep, bit_size);
      break;

   case nir_op_flt: alu->op = nir_op_slt; break;
   case nir_op_fge: alu->op = nir_op_sge; break;
   case nir_op_feq: alu->op = nir_op_seq; break;
   case nir_op_fne: alu->op = nir_op_sne; break;
   case nir_op_ieq: alu->op = nir_op_seq; break;
   case nir_op_ine: alu->op = nir_op_sne; break;

   case nir_op_ball_fequal2:  alu->op = nir_op_fall_equal2; break;
   case nir_op_ball_fequal3:  alu->op = nir_op_fall_equal3; break;
   case nir_op_ball_fequal4:  alu->op = nir_op_fall_equal4; break;
   case nir_op_bany_fnequal2: alu->op = nir_op_fany_nequal2; break;
   case nir_op_bany_fnequal3: alu->op = nir_op_fany_nequal3; break;
   case nir_op_bany_fnequal4: alu->op = nir_op_fany_nequal4; break;
   case nir_op_ball_iequal2:  alu->op = nir_op_fall_equal2; break;
   case nir_op_ball_iequal3:  alu->op = nir_op_fall_equal3; break;
   case nir_op_ball_iequal4:  alu->op = nir_op_fall_equal4; break;
   case nir_op_bany_inequal2: alu->op = nir_op_fany_nequal2; break;
   case nir_op_bany_inequal3: alu->op = nir_op_fany_nequal3; break;
   case nir_op_bany_inequal4: alu->op = nir_op_fany_nequal4; break;

   case nir_op_bcsel: {
      unsigned src_size = alu->src[1].src.ssa->bit_size;
      alu->op = nir_op_fcsel;
      /* convert the bool precision to same precision as other sources */
      if (src_size != bit_size) {
         nir_ssa_def *x = nir_f2f(b, nir_ssa_for_alu_src(b, alu, 0), src_size);
         for (unsigned i = 0; i < x->num_components; i++)
            alu->src[0].swizzle[i] = i;
         nir_instr_rewrite_src(&alu->instr, &alu->src[0].src,
                               nir_src_for_ssa(x));
      }
   } break;

   case nir_op_iand: alu->op = nir_op_fmul; break;
   case nir_op_ixor: alu->op = nir_op_sne; break;
   case nir_op_ior: alu->op = nir_op_fmax; break;

   case nir_op_inot:
      rep = nir_seq(b, nir_ssa_for_alu_src(b, alu, 0),
                       nir_imm_floatN_t(b, 0.0, bit_size));
      break;

   default:
      assert(alu->dest.dest.ssa.bit_size > 1);
      for (unsigned i = 0; i < op_info->num_inputs; i++)
         assert(alu->src[i].src.ssa->bit_size > 1);
      return false;
   }

   if (rep) {
      /* We've emitted a replacement instruction */
      nir_ssa_def_rewrite_uses(&alu->dest.dest.ssa, nir_src_for_ssa(rep));
      nir_instr_remove(&alu->instr);
   } else if (alu->dest.dest.ssa.bit_size == 1) {
      unsigned src_size = alu->src[0].src.ssa->bit_size;
      alu->dest.dest.ssa.bit_size = src_size;
      /* convert result to bool bit_size if necessary
       * this happens if comparing 32-bit floats with 16-bit bools for example
       */
      if (src_size != bit_size) {
         b->cursor = nir_after_instr(&alu->instr);
         rep = nir_f2f(b, &alu->dest.dest.ssa, bit_size);
         nir_ssa_def_rewrite_uses_after(&alu->dest.dest.ssa,
                                        nir_src_for_ssa(rep),
                                        rep->parent_instr);
      }
   }

   return true;
}

static bool
nir_lower_bool_to_float_impl(nir_function_impl *impl, unsigned bit_size)
{
   bool progress = false;

   nir_builder b;
   nir_builder_init(&b, impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         switch (instr->type) {
         case nir_instr_type_alu:
            progress |= lower_alu_instr(&b, nir_instr_as_alu(instr), bit_size);
            break;

         case nir_instr_type_load_const: {
            nir_load_const_instr *load = nir_instr_as_load_const(instr);
            if (load->def.bit_size == 1) {
               nir_const_value *value = load->value;
               for (unsigned i = 0; i < load->def.num_components; i++) {
                  float val = value[i].b ? 1.0 : 0.0;
                  if (bit_size == 16)
                     load->value[i].u16 = _mesa_float_to_half(val);
                  else
                     load->value[i].f32 = val;
               }
               load->def.bit_size = bit_size;
               progress = true;
            }
            break;
         }

         case nir_instr_type_intrinsic:
         case nir_instr_type_ssa_undef:
         case nir_instr_type_phi:
         case nir_instr_type_tex:
            /* these have <=1 dest so return value can be used for progress */
            if (!nir_foreach_ssa_def(instr, rewrite_1bit_ssa_def_to_Nbit,
                                     (void*) (size_t) bit_size))
               progress = true;
            break;

         default:
            nir_foreach_ssa_def(instr, assert_ssa_def_is_not_1bit, NULL);
         }
      }
   }

   if (progress) {
      nir_metadata_preserve(impl, nir_metadata_block_index |
                                  nir_metadata_dominance);
   }

   return progress;
}

bool
nir_lower_bool_to_float(nir_shader *shader, unsigned bit_size)
{
   bool progress = false;

   assert(bit_size == 16 || bit_size == 32);

   nir_foreach_function(function, shader) {
      if (function->impl && nir_lower_bool_to_float_impl(function->impl,
                                                         bit_size))
         progress = true;
   }

   return progress;
}
