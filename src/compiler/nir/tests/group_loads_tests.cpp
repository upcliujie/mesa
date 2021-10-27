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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <gtest/gtest.h>

#include "nir.h"
#include "nir_builder.h"

namespace {

class nir_group_loads_test : public ::testing::Test {
protected:
   nir_group_loads_test();
   ~nir_group_loads_test();

   nir_builder *b, _b;
};

nir_group_loads_test::nir_group_loads_test()
{
   glsl_type_singleton_init_or_ref();

   static const nir_shader_compiler_options options = { };
   _b = nir_builder_init_simple_shader(MESA_SHADER_COMPUTE, &options, "group_loads test");
   b = &_b;
}

nir_group_loads_test::~nir_group_loads_test()
{
   if (HasFailure()) {
      printf("\nShader from the failed test:\n\n");
      nir_print_shader(b->shader, stdout);
   }

   ralloc_free(b->shader);

   glsl_type_singleton_decref();
}

} // namespace

TEST_F(nir_group_loads_test, group_same_images)
{
   nir_ssa_def *deref[4];
   nir_ssa_def *zero = nir_imm_zero(b, 1, 32);

   for (unsigned i = 0; i < ARRAY_SIZE(deref); i++) {
      nir_variable *var =
            nir_variable_create(b->shader, nir_var_image,
                                glsl_image_type(GLSL_SAMPLER_DIM_1D, false,
                                                GLSL_TYPE_INT), "");
      deref[i] = nir_instr_ssa_def(&nir_build_deref_var(b, var)->instr);
   }

   /* Build 4 groups of 4 indirections each. nir_group_loads should interleave
    * them to get 4 indirections in total.
    */
   for (unsigned n = 0; n < ARRAY_SIZE(deref); n++) {
      nir_ssa_def *load = NULL;

      /* Build a string of load indirections. */
      for (unsigned i = 0; i < ARRAY_SIZE(deref); i++) {
         load = nir_build_image_deref_load(b, 1, 32, deref[i],
                                           i ? load : zero,
                                           zero, zero,
                                           GLSL_SAMPLER_DIM_1D, false,
                                           PIPE_FORMAT_R32_SINT,
                                           ACCESS_CAN_REORDER,
                                           nir_type_int32);
      }
   }

   NIR_PASS_V(b->shader, nir_group_loads, nir_group_same_resource_only, 1000);

   unsigned counter = 0;
   nir_foreach_block(block, b->impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type == nir_instr_type_intrinsic) {
            nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

            if (intr->intrinsic != nir_intrinsic_image_deref_load)
               continue;

            /* Check whether derefs are interleaved. */
            ASSERT_EQ(intr->src[0].ssa, deref[counter / ARRAY_SIZE(deref)]);
            counter++;
         }
      }
   }
}

TEST_F(nir_group_loads_test, group_same_textures)
{
   nir_ssa_def *deref[4];
   nir_ssa_def *zero = nir_imm_float(b, 1);

   for (unsigned i = 0; i < ARRAY_SIZE(deref); i++) {
      nir_variable *var =
            nir_variable_create(b->shader, nir_var_uniform,
                                glsl_sampler_type(GLSL_SAMPLER_DIM_1D, false,
                                                  false, GLSL_TYPE_FLOAT), "");
      deref[i] = nir_instr_ssa_def(&nir_build_deref_var(b, var)->instr);
   }

   /* Build 4 groups of 4 indirections each. nir_group_loads should interleave
    * them to get 4 indirections in total.
    */
   for (unsigned n = 0; n < ARRAY_SIZE(deref); n++) {
      nir_ssa_def *last = NULL;

      /* Build a string of texture indirections. */
      for (unsigned i = 0; i < ARRAY_SIZE(deref); i++) {
         nir_tex_instr *tex = nir_tex_instr_create(b->shader, 3);
         tex->sampler_dim = GLSL_SAMPLER_DIM_1D;
         tex->op = nir_texop_tex;
         tex->src[0].src_type = nir_tex_src_texture_deref;
         tex->src[0].src = nir_src_for_ssa(deref[i]);
         tex->src[1].src_type = nir_tex_src_sampler_deref;
         tex->src[1].src = nir_src_for_ssa(deref[i]);
         tex->src[2].src_type = nir_tex_src_coord;
         tex->src[2].src = last ? nir_src_for_ssa(last) : nir_src_for_ssa(zero);
         tex->dest_type = nir_type_float32;
         tex->coord_components = 1;

         nir_ssa_dest_init(&tex->instr, &tex->dest, 1, 32, "");
         nir_builder_instr_insert(b, &tex->instr);
         last = nir_instr_ssa_def(&tex->instr);
      }
   }

   NIR_PASS_V(b->shader, nir_group_loads, nir_group_same_resource_only, 1000);

   unsigned counter = 0;
   nir_foreach_block(block, b->impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type == nir_instr_type_tex) {
            nir_tex_instr *tex = nir_instr_as_tex(instr);

            /* Check whether derefs are interleaved. */
            ASSERT_EQ(tex->src[0].src.ssa, deref[counter / ARRAY_SIZE(deref)]);
            counter++;
         }
      }
   }
}
