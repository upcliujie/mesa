/* -*- mesa-c++  -*-
 *
 * Copyright (c) 2020 Collabora LTD
 *
 * Author: Gert Wollny <gert.wollny@collabora.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "nir.h"
#include "nir_builder.h"


#include <map>
#include <vector>
#include <iostream>

namespace r600 {

bool
r600_nir_split_64bit_io_filter(const nir_instr *instr, const void *_options)
{
   switch (instr->type) {
   case  nir_instr_type_intrinsic: {
      auto intr = nir_instr_as_intrinsic(instr);

      switch (intr->intrinsic) {
      case nir_intrinsic_load_deref:
      case nir_intrinsic_load_uniform:
      case nir_intrinsic_load_input:
      case nir_intrinsic_load_ubo:
         if (nir_dest_bit_size(intr->dest) != 64)
            return false;
         return nir_dest_num_components(intr->dest) >= 3;
      case nir_intrinsic_store_output:
         if (nir_src_bit_size(intr->src[0]) != 64)
            return false;
         return nir_src_num_components(intr->src[0]) >= 3;
      default:
         return false;
      }
   }
   case  nir_instr_type_alu: {
      auto alu = nir_instr_as_alu(instr);
      switch (alu->op) {
      case nir_op_bany_fnequal3:
      case nir_op_bany_fnequal4:
      case nir_op_ball_fequal3:
      case nir_op_ball_fequal4:
      case nir_op_bany_inequal3:
      case nir_op_bany_inequal4:
      case nir_op_ball_iequal3:
      case nir_op_ball_iequal4:
      case nir_op_fdot3:
      case nir_op_fdot4:
         return nir_src_bit_size(alu->src[1].src) == 64;
      default:
         return false;
      }
   }
   default:
      return false;
   }
}

static nir_ssa_def *
r600_nir_split_double_load_deref(nir_builder *b, nir_intrinsic_instr *intr)
{
   auto deref1 = nir_instr_as_deref(intr->src[0].ssa->parent_instr);
   auto var = nir_intrinsic_get_var(intr, 0);
   auto var2 = nir_variable_clone(var, b->shader);
   ++var2->data.driver_location;
   ++var2->data.location;

   if (var->data.mode == nir_var_shader_in ||
       var->data.mode == nir_var_shader_out)
      nir_shader_add_variable(b->shader, var2);

   unsigned old_components = var->type->components();

   deref1->type = var->type = glsl_dvec_type(2);

   nir_intrinsic_instr *load1 = nir_intrinsic_instr_create(b->shader, nir_intrinsic_load_deref);
   load1->src[0] = nir_src_for_ssa(&deref1->dest.ssa);
   nir_ssa_dest_init(&load1->instr, &load1->dest, 2, 64, nullptr);
   nir_builder_instr_insert(b, &load1->instr);
   load1->num_components = 2;

   nir_deref_instr *deref2 = nir_build_deref_var(b, var2);
   deref2->type = var2->type = glsl_dvec_type(old_components - 2);

   nir_intrinsic_instr *load2 = nir_intrinsic_instr_create(b->shader, nir_intrinsic_load_deref);
   load2->src[0] = nir_src_for_ssa(&deref2->dest.ssa);
   load2->num_components = old_components - 2;
   var2->type = glsl_dvec_type(old_components - 2);
   nir_ssa_dest_init(&load2->instr, &load2->dest, old_components - 2, 64, nullptr);
   nir_builder_instr_insert(b, &load2->instr);

   if (old_components == 3)
      return nir_vec3(b, nir_channel(b, &load1->dest.ssa, 0),
                      nir_channel(b, &load1->dest.ssa, 1),
                      nir_channel(b, &load2->dest.ssa, 0));
   else
      return nir_vec4(b, nir_channel(b, &load1->dest.ssa, 0),
                      nir_channel(b, &load1->dest.ssa, 1),
                      nir_channel(b, &load2->dest.ssa, 0),
                      nir_channel(b, &load2->dest.ssa, 1));
}

static nir_ssa_def *
r600_nir_split_double_load(nir_builder *b, nir_intrinsic_instr *load1)
{
   unsigned old_components = nir_dest_num_components(load1->dest);
   auto load2 = nir_instr_as_intrinsic(nir_instr_clone(b->shader, &load1->instr));
   nir_io_semantics sem = nir_intrinsic_io_semantics(load1);

   load1->dest.ssa.num_components = 2;
   sem.num_slots = 1;
   nir_intrinsic_set_io_semantics(load1, sem);

   load2->dest.ssa.num_components = old_components - 2;
   sem.location += 1;
   nir_intrinsic_set_io_semantics(load2, sem);
   nir_intrinsic_set_base(load2, nir_intrinsic_base(load1) + 1);
   nir_builder_instr_insert(b, &load2->instr);


   if (old_components == 3)
      return nir_vec3(b, nir_channel(b, &load1->dest.ssa, 0),
                      nir_channel(b, &load1->dest.ssa, 1),
                      nir_channel(b, &load2->dest.ssa, 0));
   else
      return nir_vec4(b, nir_channel(b, &load1->dest.ssa, 0),
                      nir_channel(b, &load1->dest.ssa, 1),
                      nir_channel(b, &load2->dest.ssa, 0),
                      nir_channel(b, &load2->dest.ssa, 1));
}


static nir_ssa_def *
r600_nir_split_store_output(nir_builder *b, nir_intrinsic_instr *store1)
{
   auto src = store1->src[0];
   unsigned old_components = nir_src_num_components(src);
   nir_io_semantics sem = nir_intrinsic_io_semantics(store1);

   auto store2 = nir_instr_as_intrinsic(nir_instr_clone(b->shader, &store1->instr));
   auto src1 = nir_channels(b, src.ssa, 3);
   auto src2 = nir_channels(b, src.ssa, old_components == 3 ? 4 : 0xc);

   nir_instr_rewrite_src(&store1->instr, &src, nir_src_for_ssa(src1));
   nir_intrinsic_set_write_mask(store1, 3);

   nir_instr_rewrite_src(&store2->instr, &src, nir_src_for_ssa(src2));
   nir_intrinsic_set_write_mask(store1, old_components == 3 ? 1 : 3);

   sem.num_slots = 1;
   nir_intrinsic_set_io_semantics(store1, sem);

   sem.location += 1;
   nir_intrinsic_set_io_semantics(store2, sem);
   nir_intrinsic_set_base(store2, nir_intrinsic_base(store1));

   nir_builder_instr_insert(b, &store2->instr);
   return NIR_LOWER_INSTR_PROGRESS;
}


static nir_ssa_def *
r600_nir_split_double_load_uniform(nir_builder *b, nir_intrinsic_instr *intr)
{
   unsigned second_components = nir_dest_num_components(intr->dest) - 2;
   nir_intrinsic_instr *load2 = nir_intrinsic_instr_create(b->shader, nir_intrinsic_load_uniform);
   load2->src[0] = nir_src_for_ssa(nir_iadd_imm(b, intr->src[0].ssa, 1));
   nir_intrinsic_set_dest_type(load2, nir_intrinsic_dest_type(intr));
   nir_intrinsic_set_base(load2, nir_intrinsic_base(intr));
   nir_intrinsic_set_range(load2, nir_intrinsic_range(intr));
   load2->num_components = second_components;

   nir_ssa_dest_init(&load2->instr, &load2->dest, second_components, 64, nullptr);
   nir_builder_instr_insert(b, &load2->instr);

   intr->dest.ssa.num_components = intr->num_components = 2;

   if (second_components == 1)
      return nir_vec3(b, nir_channel(b, &intr->dest.ssa, 0),
                      nir_channel(b, &intr->dest.ssa, 1),
                      nir_channel(b, &load2->dest.ssa, 0));
   else
      return nir_vec4(b, nir_channel(b, &intr->dest.ssa, 0),
                      nir_channel(b, &intr->dest.ssa, 1),
                      nir_channel(b, &load2->dest.ssa, 0),
                      nir_channel(b, &load2->dest.ssa, 1));
}

static nir_ssa_def *
r600_nir_split_double_load_ubo(nir_builder *b, nir_intrinsic_instr *intr)
{
   unsigned second_components = nir_dest_num_components(intr->dest) - 2;
   nir_intrinsic_instr *load2 = nir_intrinsic_instr_create(b->shader, nir_intrinsic_load_ubo);
   load2->src[0] = intr->src[0];
   load2->src[1] = nir_src_for_ssa(nir_iadd_imm(b, intr->src[1].ssa, 16));
   nir_intrinsic_set_range_base(load2, nir_intrinsic_range_base(intr) + 16);
   nir_intrinsic_set_range(load2, nir_intrinsic_range(intr));
   nir_intrinsic_set_access(load2, nir_intrinsic_access(intr));
   nir_intrinsic_set_align_mul(load2, nir_intrinsic_align_mul(intr));
   nir_intrinsic_set_align_offset(load2, nir_intrinsic_align_offset(intr) + 16);

   load2->num_components = second_components;

   nir_ssa_dest_init(&load2->instr, &load2->dest, second_components, 64, nullptr);
   nir_builder_instr_insert(b, &load2->instr);

   intr->dest.ssa.num_components = intr->num_components = 2;

   if (second_components == 1)
      return nir_vec3(b, nir_channel(b, &intr->dest.ssa, 0),
                      nir_channel(b, &intr->dest.ssa, 1),
                      nir_channel(b, &load2->dest.ssa, 0));
   else
      return nir_vec4(b, nir_channel(b, &intr->dest.ssa, 0),
                      nir_channel(b, &intr->dest.ssa, 1),
                      nir_channel(b, &load2->dest.ssa, 0),
                      nir_channel(b, &load2->dest.ssa, 1));
}


static nir_ssa_def *
r600_nir_split_reduction(nir_builder *b, nir_ssa_def *src[2][2], nir_op op1, nir_op op2, nir_op reduction)
{
   auto cmp0 = nir_build_alu(b, op1, src[0][0], src[0][1], nullptr, nullptr);
   auto cmp1 = nir_build_alu(b, op2, src[1][0], src[1][1], nullptr, nullptr);
   return nir_build_alu(b, reduction, cmp0, cmp1, nullptr, nullptr);
}

static nir_ssa_def *
r600_nir_split_reduction3(nir_builder *b, nir_alu_instr *alu,
                          nir_op op1, nir_op op2, nir_op reduction)
{
   nir_ssa_def *src[2][2];

   src[0][0] = nir_channels(b, nir_ssa_for_src(b, alu->src[0].src, 2), 3);
   src[0][1] = nir_channels(b, nir_ssa_for_src(b, alu->src[1].src, 2), 3);

   src[1][0]  = nir_channel(b, nir_ssa_for_src(b, alu->src[0].src, 3), 2);
   src[1][1]  = nir_channel(b, nir_ssa_for_src(b, alu->src[1].src, 3), 2);

   return r600_nir_split_reduction(b, src, op1, op2, reduction);
}

static nir_ssa_def *
r600_nir_split_reduction4(nir_builder *b, nir_alu_instr *alu,
                          nir_op op1, nir_op op2, nir_op reduction)
{
   nir_ssa_def *src[2][2];

   src[0][0] = nir_channels(b, nir_ssa_for_src(b, alu->src[0].src, 2), 3);
   src[0][1] = nir_channels(b, nir_ssa_for_src(b, alu->src[1].src, 2), 3);

   src[1][0]  = nir_channels(b, nir_ssa_for_src(b, alu->src[0].src, 4), 0xc);
   src[1][1]  = nir_channels(b, nir_ssa_for_src(b, alu->src[1].src, 4), 0xc);

   return r600_nir_split_reduction(b, src, op1, op2, reduction);
}

static nir_ssa_def *
r600_nir_split_64bit_io_impl(nir_builder *b, nir_instr *instr, void *_options)
{
   switch (instr->type) {
   case nir_instr_type_intrinsic: {
      auto intr = nir_instr_as_intrinsic(instr);
      switch (intr->intrinsic) {
      case nir_intrinsic_load_deref:
         return r600_nir_split_double_load_deref(b, intr);
      case nir_intrinsic_load_uniform:
         return r600_nir_split_double_load_uniform(b, intr);
      case nir_intrinsic_load_ubo:
         return r600_nir_split_double_load_ubo(b, intr);
      case nir_intrinsic_load_input:
         return r600_nir_split_double_load(b, intr);
      case nir_intrinsic_store_output:
         return r600_nir_split_store_output(b, intr);
      default:
         assert(0);
      }
   }
   case  nir_instr_type_alu: {
      auto alu = nir_instr_as_alu(instr);
      nir_print_instr(instr, stderr);
      fprintf(stderr, "\n");
      switch (alu->op) {
      case nir_op_bany_fnequal3:
         return r600_nir_split_reduction3(b, alu, nir_op_bany_fnequal2, nir_op_fneu, nir_op_ior);
      case nir_op_ball_fequal3:
         return r600_nir_split_reduction3(b, alu, nir_op_ball_fequal2, nir_op_feq, nir_op_iand);
      case nir_op_bany_inequal3:
         return r600_nir_split_reduction3(b, alu, nir_op_bany_inequal2, nir_op_ine, nir_op_ior);
      case nir_op_ball_iequal3:
         return r600_nir_split_reduction3(b, alu, nir_op_ball_iequal2, nir_op_ieq, nir_op_iand);
      case nir_op_fdot3:
         return r600_nir_split_reduction3(b, alu, nir_op_fdot2, nir_op_fmul, nir_op_fadd);
      case nir_op_bany_fnequal4:
         return r600_nir_split_reduction4(b, alu, nir_op_bany_fnequal2, nir_op_bany_fnequal2, nir_op_ior);
      case nir_op_ball_fequal4:
         return r600_nir_split_reduction4(b, alu, nir_op_ball_fequal2, nir_op_ball_fequal2, nir_op_iand);
      case nir_op_bany_inequal4:
         return r600_nir_split_reduction4(b, alu, nir_op_bany_inequal2, nir_op_bany_inequal2, nir_op_ior);
      case nir_op_ball_iequal4:
         return r600_nir_split_reduction4(b, alu, nir_op_bany_fnequal2, nir_op_bany_fnequal2, nir_op_ior);
      case nir_op_fdot4:
         return r600_nir_split_reduction4(b, alu, nir_op_fdot2, nir_op_fdot2, nir_op_fadd);
      default:
         assert(0);
      }
   }
   default:
      assert(0);
   }
   return nullptr;
}


bool
r600_nir_split_64bit_io(nir_shader *sh)
{
   bool result = nir_shader_lower_instructions(sh,
                                               r600_nir_split_64bit_io_filter,
                                               r600_nir_split_64bit_io_impl,
                                               nullptr);
   return result;
}

bool
r600_nir_64_to_vec2_filter(const nir_instr *instr, const void *_options)
{
   switch (instr->type) {
   case nir_instr_type_intrinsic:  {
      auto intr = nir_instr_as_intrinsic(instr);

      switch (intr->intrinsic) {
      case nir_intrinsic_load_deref:
      case nir_intrinsic_load_input:
      case nir_intrinsic_load_uniform:
      case nir_intrinsic_load_ubo:
      case nir_intrinsic_load_ubo_vec4:
         return nir_dest_bit_size(intr->dest) == 64;
      case nir_intrinsic_store_output:
         return nir_src_bit_size(intr->src[0]) == 64;
      default:
         return false;
      }
   }
   case nir_instr_type_alu: {
      auto alu = nir_instr_as_alu(instr);
      return nir_dest_bit_size(alu->dest.dest) == 64;
   }
   case nir_instr_type_phi: {
      auto phi = nir_instr_as_phi(instr);
      return nir_dest_bit_size(phi->dest) == 64;
   }
   case nir_instr_type_load_const:  {
      auto lc = nir_instr_as_load_const(instr);
      return lc->def.bit_size == 64;
   }
   default:
      return false;
   }
}

static nir_ssa_def *
r600_nir_64_to_vec2_load(nir_builder *b, nir_intrinsic_instr *intr)
{
   auto deref = nir_instr_as_deref(intr->src[0].ssa->parent_instr);
   unsigned components = 4;
   if (deref->deref_type == nir_deref_type_var) {
      auto var = nir_intrinsic_get_var(intr, 0);
      components = 2 * var->type->components();
      deref->type = var->type = glsl_vec_type(components);
   } else {
      assert(0 && "Only lowring of var derefs supported\n");
   }
   intr->num_components = components;
   intr->dest.ssa.bit_size = 32;
   intr->dest.ssa.num_components = components;
   return NIR_LOWER_INSTR_PROGRESS;
}

static nir_ssa_def *
r600_nir_64_to_vec2_uniform(nir_builder *b, nir_intrinsic_instr *intr)
{
   intr->num_components *= 2;
   intr->dest.ssa.bit_size = 32;
   intr->dest.ssa.num_components *= 2;
   nir_intrinsic_set_dest_type(intr, nir_type_float32);
   return NIR_LOWER_INSTR_PROGRESS;
}

static nir_ssa_def *
r600_nir_load_64_to_vec2(nir_builder *b, nir_intrinsic_instr *intr)
{
   intr->num_components *= 2;
   intr->dest.ssa.bit_size = 32;
   intr->dest.ssa.num_components *= 2;
   nir_intrinsic_set_component(intr, nir_intrinsic_component(intr) * 2);
   return NIR_LOWER_INSTR_PROGRESS;
}

static nir_ssa_def *
r600_nir_store_64_to_vec2(nir_builder *b, nir_intrinsic_instr *intr)
{
   auto wm = nir_intrinsic_write_mask(intr);
   nir_intrinsic_set_write_mask(intr, (wm == 1) ? 3 : 0xf);
   return NIR_LOWER_INSTR_PROGRESS;
}

static nir_ssa_def *
r600_nir_64_to_vec2_impl(nir_builder *b, nir_instr *instr, void *_options)
{
   switch (instr->type) {
   case nir_instr_type_intrinsic:  {
      auto intr = nir_instr_as_intrinsic(instr);
      switch (intr->intrinsic) {
      case nir_intrinsic_load_deref:
         return r600_nir_64_to_vec2_load(b, intr);
      case nir_intrinsic_load_uniform:
         return r600_nir_64_to_vec2_uniform(b, intr);
      case nir_intrinsic_load_input:
      case nir_intrinsic_load_ubo:
      case nir_intrinsic_load_ubo_vec4:
         return r600_nir_load_64_to_vec2(b, intr);
      case nir_intrinsic_store_output:
         return r600_nir_store_64_to_vec2(b, intr);
      default:
         return nullptr;
      }
   }
   case nir_instr_type_alu: {
      auto alu = nir_instr_as_alu(instr);
      alu->dest.dest.ssa.bit_size = 32;
      alu->dest.dest.ssa.num_components = 2;
      alu->dest.write_mask = 3;
      if (alu->op == nir_op_pack_64_2x32_split)
         alu->op = nir_op_vec2;
      if (alu->op == nir_op_pack_64_2x32)
         alu->op = nir_op_mov;
      return NIR_LOWER_INSTR_PROGRESS;
   }
   case nir_instr_type_phi: {
      auto phi = nir_instr_as_phi(instr);
      phi->dest.ssa.bit_size = 32;
      phi->dest.ssa.num_components = 2;
      return NIR_LOWER_INSTR_PROGRESS;
   }
   case nir_instr_type_load_const:  {
      auto lc = nir_instr_as_load_const(instr);
      nir_const_value val[2];
      uint64_t v = lc->value->u64;

      val[0].u32 = v & 0xffffffff;
      val[1].u32 = (v >> 32) & 0xffffffff;
      return nir_build_imm(b, 2, 32, val);
   }
   default:
      return nullptr;
   }

}

static bool store_64bit_intr(nir_src *src, void *state)
{
   bool *s = (bool *)state;
   *s = nir_src_bit_size(*src) == 64;
   return !*s;
}

static bool double2vec2(nir_src *src, void *state)
{
   if (nir_src_bit_size(*src) != 64)
      return true;

   assert(src->is_ssa);
   src->ssa->bit_size = 32;
   src->ssa->num_components *= 2;
   return true;
}

bool
r600_nir_64_to_vec2(nir_shader *sh)
{
   std::vector<nir_instr*> intr64bit;

   nir_foreach_function(function, sh) {
      if (function->impl) {
         nir_builder b;
         nir_builder_init(&b, function->impl);

         nir_foreach_block(block, function->impl) {
            nir_foreach_instr_safe(instr, block) {
               if (instr->type != nir_instr_type_alu)
                  continue;

               bool success = false;
               nir_foreach_src(instr, store_64bit_intr, &success);
               if (success)
                  intr64bit.push_back(instr);
            }
         }
      }
   }

   bool result = nir_shader_lower_instructions(sh,
                                               r600_nir_64_to_vec2_filter,
                                               r600_nir_64_to_vec2_impl,
                                               nullptr);

   if (result || !intr64bit.empty()) {

      for(auto&& instr: intr64bit) {
         if (instr->type == nir_instr_type_alu) {
            auto alu = nir_instr_as_alu(instr);
            auto alu_info = nir_op_infos[alu->op];
            for (unsigned i = 0; i < alu_info.num_inputs; ++i) {
               int swizzle[NIR_MAX_VEC_COMPONENTS] = {0};
               for (unsigned k = 0; k < NIR_MAX_VEC_COMPONENTS / 2; k++) {
                  if (!nir_alu_instr_channel_used(alu, i, k)) {
                     continue;
                  }

                  switch (alu->op) {
                  case nir_op_unpack_64_2x32_split_x:
                     swizzle[2 * k] = alu->src[i].swizzle[k] * 2;
                     alu->op = nir_op_mov;
                     break;
                  case nir_op_unpack_64_2x32_split_y:
                     swizzle[2 * k] = alu->src[i].swizzle[k] * 2 + 1;
                     alu->op = nir_op_mov;
                     break;
                  case nir_op_unpack_64_2x32:
                     alu->op = nir_op_mov;
                     break;
                  case nir_op_bcsel:
                     if (i == 0) {
                        swizzle[2 * k] = swizzle[2 * k + 1] = alu->src[i].swizzle[k] * 2;
                        break;
                     }
                     /* fallthrough */
                  default:
                     swizzle[2 * k] = alu->src[i].swizzle[k] * 2;
                     swizzle[2 * k + 1] = alu->src[i].swizzle[k] * 2 + 1;
                  }
               }
               for (unsigned k = 0; k < NIR_MAX_VEC_COMPONENTS; ++k) {
                  alu->src[i].swizzle[k] = swizzle[k];
               }
            }
         } else
            nir_foreach_src(instr, double2vec2, nullptr);
      }
      result = true;
   }

   return result;
}

using std::map;
using std::vector;
using std::pair;

class StoreMerger {
public:
   StoreMerger(nir_shader *shader);
   void collect_stores();
   bool combine();
   void combine_one_slot(vector<nir_intrinsic_instr*>& stores);

   using StoreCombos = map<unsigned, vector<nir_intrinsic_instr*>>;

   StoreCombos m_stores;
   nir_shader *sh;
   nir_builder b;
};

StoreMerger::StoreMerger(nir_shader *shader):
   sh(shader)
{
}


void StoreMerger::collect_stores()
{
   unsigned vertex = 0;
   nir_foreach_function(function, sh) {
      if (function->impl) {
         nir_foreach_block(block, function->impl) {
            nir_foreach_instr_safe(instr, block) {
               if (instr->type != nir_instr_type_intrinsic)
                  continue;

               auto ir = nir_instr_as_intrinsic(instr);
               if (ir->intrinsic == nir_intrinsic_emit_vertex ||
                   ir->intrinsic == nir_intrinsic_emit_vertex_with_counter) {
                  ++vertex;
                  continue;
               }
               if (ir->intrinsic != nir_intrinsic_store_output)
                  continue;
               m_stores[64 * vertex + nir_intrinsic_base(ir)].push_back(ir);

            }
         }
      }
   }
}

bool StoreMerger::combine()
{
   bool progress = false;
   for(auto&& i : m_stores) {
      if (i.second.size() < 2)
         continue;

      combine_one_slot(i.second);
      progress = true;
   }
   return progress;
}

void StoreMerger::combine_one_slot(vector<nir_intrinsic_instr*>& stores)
{
   /* We assume that we lower_io_to_vector did most of the hard work, and we have
    * to deal only with wiredness from the double lowering here, hence we combine
    * two vec 2 writes */
   assert(stores.size() == 2);

   auto store1 = stores[0];
   auto store2 = stores[1];

   assert(nir_intrinsic_component(store1) == 0);
   assert(nir_intrinsic_component(store2) == 2);
   assert(nir_intrinsic_write_mask(store1) == 3);
   assert(nir_intrinsic_write_mask(store2) == 3);

   nir_intrinsic_set_component(store2, 0);
   nir_intrinsic_set_write_mask(store2, 0xf);

   nir_builder b;
   nir_builder_init(&b, nir_shader_get_entrypoint(sh));
   b.cursor = nir_before_instr(&store2->instr);

   auto x = nir_channel(&b, store1->src[0].ssa, 0);
   auto y = nir_channel(&b, store1->src[0].ssa, 1);
   auto z = nir_channel(&b, store2->src[0].ssa, 0);
   auto w = nir_channel(&b, store2->src[0].ssa, 1);

   auto new_src = nir_vec4(&b, x,y,z,w);

   nir_instr_rewrite_src(&store2->instr, &store2->src[0], nir_src_for_ssa(new_src));
   store2->num_components = 4;
   nir_instr_remove(&store1->instr);
}

bool r600_merge_vec2_stores(nir_shader *shader)
{
   r600::StoreMerger merger(shader);
   merger.collect_stores();
   return merger.combine();
}

} // end namespace r600


