/*
 * Copyright (C) 2021 Google, Inc.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nir.h"
#include "nir_builder.h"

static bool
texops(nir_builder *b, nir_instr *instr)
{
   nir_tex_instr *tex = nir_instr_as_tex(instr);
   nir_instr *src_parent[tex->num_srcs];

   for (unsigned i = 0; i < tex->num_srcs; i++) {
      if (!tex->src[i].src.is_ssa)
         return false;

      unsigned num_components = nir_tex_instr_src_size(tex, i);
      src_parent[i] = tex->src[i].src.ssa->parent_instr;

      switch (src_parent[i]->type) {
      case nir_instr_type_load_const: {
         nir_load_const_instr *load_const = nir_instr_as_load_const(src_parent[i]);
         if (false)
            return false;
         break;
      }
      case nir_instr_type_alu: {
         nir_alu_instr *alu_instr = nir_instr_as_alu(src_parent[i]);
         if (!alu_instr->src[0].src.is_ssa)
            return false;

         switch (alu_instr->op) {
         case nir_op_f2f32:
            if (alu_instr->src[0].src.ssa->bit_size != 16)
               return false;

            FALLTHROUGH;
         case nir_op_i2i32:
            if (alu_instr->src[0].src.ssa->num_components != num_components)
               return false;
            break;

         case nir_op_mov:
         case nir_op_vec2:
         case nir_op_vec3:
         case nir_op_vec4:
         case nir_op_vec5:
         case nir_op_vec8:
         case nir_op_vec16: {
            unsigned num_inputs = nir_op_infos[alu_instr->op].num_inputs;
            if (num_inputs != num_components)
               return false;

            for (int i = 0; i < num_inputs; i++) {
               if (!alu_instr->src[i].src.is_ssa)
                  return false;

               nir_ssa_def *ssa = alu_instr->src[i].src.ssa;
               if (ssa->bit_size != 32)
                  return false;

               nir_instr *parent = ssa->parent_instr;
               if (parent->type != nir_instr_type_alu)
                  return false;

               nir_alu_instr *alu = nir_instr_as_alu(parent);
               if (alu->op != nir_op_f2f32 ||
                   alu->src[0].src.ssa->bit_size != 16)
                  return false;
            }
            break;
         }
         default:
            return false;
         }
         break;
      }
      default:
         return false;
      }
   }

   for (unsigned i = 0; i < tex->num_srcs; i++) {
      nir_instr *src_instr = src_parent[i];
      nir_ssa_def *copy_def;

      b->cursor = nir_before_instr(&tex->instr);

      switch (src_instr->type) {
      case nir_instr_type_load_const: {
         nir_load_const_instr *load_const = nir_instr_as_load_const(src_instr);

         nir_ssa_def *conv = nir_f2f16(b, &load_const->def);
         nir_instr_rewrite_src(instr, &tex->src[i].src, nir_src_for_ssa(conv));
         break;
      }
      case nir_instr_type_alu: {
         nir_alu_instr *alu_instr = nir_instr_as_alu(src_instr);

         if (alu_instr->op == nir_op_i2f32) {
            copy_def = nir_i2f16(b, alu_instr->src[0].src.ssa);
         } else if (alu_instr->op == nir_op_f2f32) {
            copy_def = alu_instr->src[0].src.ssa;
         } else if (nir_op_is_vec(alu_instr->op)) {
            unsigned num_inputs = nir_op_infos[alu_instr->op].num_inputs;
            nir_ssa_def *srcs[num_inputs];

            for (int i = 0; i < num_inputs; i++) {
               nir_ssa_def *ssa = alu_instr->src[i].src.ssa;
               assert(ssa->bit_size == 32);

               nir_instr *parent = ssa->parent_instr;
               assert(parent->type == nir_instr_type_alu);

               nir_alu_instr *alu = nir_instr_as_alu(parent);
               assert(alu->op == nir_op_f2f32 &&
                      alu->src[0].src.ssa->bit_size == 16);

               srcs[i] = nir_channel(b, alu->src[0].src.ssa,
                                     alu_instr->src[i].swizzle[0]);
            }

            copy_def = nir_vec(b, srcs, num_inputs);
         } else {
            continue;
         }

         nir_instr_rewrite_src(instr, &tex->src[i].src, nir_src_for_ssa(copy_def));
         break;
      }
      default:
         break;
      }
   }

   return true;
}

static bool
opt_mediump_texops_impl(nir_function_impl *impl,
                        const struct nir_shader_compiler_options *options)
{
   nir_builder b;
   nir_builder_init(&b, impl);
   bool progress = false;

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         b.cursor = nir_before_instr(instr);

         switch (instr->type) {
         case nir_instr_type_tex:
            if (texops(&b, instr))
               progress = true;
            break;

         default:
            break;
         }
      }
   }

   return progress;
}

bool
nir_opt_mediump_texops(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (!function->impl)
         continue;

      if (opt_mediump_texops_impl(function->impl, shader->options)) {
         progress = true;
         nir_metadata_preserve(function->impl, nir_metadata_block_index |
                                               nir_metadata_dominance);
      } else {
         nir_metadata_preserve(function->impl, nir_metadata_all);
      }
   }

   return progress;
}
