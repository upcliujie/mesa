/*
 * Copyright © 2016 Intel Corporation
 * Copyright © 2020 SUSE LLC
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

#include <gtest/gtest.h>
#include "brw_fs.h"
#include "brw_cfg.h"
#include "program/program.h"

using namespace brw;

class register_coalesce_test : public ::testing::Test {
   virtual void SetUp();

public:
   struct brw_compiler *compiler;
   struct gen_device_info *devinfo;
   struct gl_context *ctx;
   struct brw_wm_prog_data *prog_data;
   struct gl_shader_program *shader_prog;
   fs_visitor *v;
};

class register_coalesce_fs_visitor : public fs_visitor
{
public:
	register_coalesce_fs_visitor(struct brw_compiler *compiler,
			             struct brw_wm_prog_data *prog_data,
			             nir_shader *shader)
           : fs_visitor(compiler, NULL, NULL, NULL,
	            &prog_data->base, shader, 8, -1) {}
};

void register_coalesce_test::SetUp()
{
   ctx = (struct gl_context *)calloc(1, sizeof(*ctx));
   compiler = (struct brw_compiler *)calloc(1, sizeof(*compiler));
   devinfo = (struct gen_device_info *)calloc(1, sizeof(*devinfo));
   compiler->devinfo = devinfo;

   prog_data = ralloc(NULL, struct brw_wm_prog_data);
   nir_shader *shader =
      nir_shader_create(NULL, MESA_SHADER_FRAGMENT, NULL, NULL);

   v = new register_coalesce_fs_visitor(compiler, prog_data, shader);

   devinfo->gen = 4;
}

static fs_inst *
instruction(bblock_t *block, int num)
{
   fs_inst *inst = (fs_inst *)block->start();
   for (int i = 0; i < num; i++) {
      inst = (fs_inst *)inst->next;
   }
   return inst;
}


static bool
register_coalesce(fs_visitor *v)
{
   const bool print = getenv("TEST_DEBUG");

   if (print) {
      fprintf(stderr, "= Before =\n");
      v->cfg->dump(v);
   }

   bool ret = v->register_coalesce();

   if (print) {
      fprintf(stderr, "\n= After =\n");
      v->cfg->dump(v);
   }

   return ret;
}

TEST_F(register_coalesce_test, basic)
{
   const fs_builder &bld = v->bld;

   /*
    * add vgrf2:F, vgrf0:F, vgrf1:F
    * mov vgrf3:F, vgrf2:F
    * mul vgrf4:F, vgrf4:F, vgrf3:F
    *
    * becomes:
    *
    * add vgrf3:F, vgrf0:F, vgrf1:F
    * mul vgrf4:F, vgrf4:F, vgrf3:F
    *
    */

   fs_reg vgrf0 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf1 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf2 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf3 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf4 = v->vgrf(glsl_type::float_type);

   bld.ADD(vgrf2, vgrf0, vgrf1);
   bld.MOV(vgrf3, vgrf2);
   bld.MUL(vgrf4, vgrf4, vgrf3);

   v->calculate_cfg();
   bblock_t *block0 = v->cfg->blocks[0];

   EXPECT_EQ(0, block0->start_ip);
   EXPECT_EQ(2, block0->end_ip);

   EXPECT_TRUE(register_coalesce(v));
   EXPECT_EQ(0, block0->start_ip);
   EXPECT_EQ(1, block0->end_ip);

   fs_inst *add = instruction(block0, 0);
   EXPECT_EQ(BRW_OPCODE_ADD, add->opcode);
   EXPECT_TRUE(add->dst.equals(vgrf3));
   EXPECT_TRUE(add->src[0].equals(vgrf0));
   EXPECT_TRUE(add->src[1].equals(vgrf1));

   fs_inst *mul = instruction(block0, 1);
   EXPECT_EQ(BRW_OPCODE_MUL, mul->opcode);
   EXPECT_TRUE(mul->dst.equals(vgrf4));
   EXPECT_TRUE(mul->src[0].equals(vgrf4));
   EXPECT_TRUE(mul->src[1].equals(vgrf3));
}


TEST_F(register_coalesce_test, cmod)
{
   const fs_builder &bld = v->bld;

   /*
    * add    vgrf2:F, vgrf0:F, vgrf1:F
    * mov.nz vgrf3:F, vgrf2:F
    * mul    vgrf4:F, vgrf4:F, vgrf3:F
    *
    * Here the MOV contains a cmod and it should
    * should not be deleted (see commit e581ddee).
    * The expected output is:
    *
    * add    vgrf3:F, vgrf0:F, vgrf1:F
    * mov.nz null,    vgrf3:F
    * mul    vgrf4:F, vgrf4:F, vgrf3:F
    *
    */

   fs_reg vgrf0 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf1 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf2 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf3 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf4 = v->vgrf(glsl_type::float_type);

   bld.ADD(vgrf2, vgrf0, vgrf1);
   fs_inst *mov = bld.MOV(vgrf3, vgrf2);
   set_condmod(BRW_CONDITIONAL_NZ, mov);
   bld.MUL(vgrf4, vgrf4, vgrf3);

   v->calculate_cfg();
   bblock_t *block0 = v->cfg->blocks[0];

   EXPECT_EQ(0, block0->start_ip);
   EXPECT_EQ(2, block0->end_ip);

   EXPECT_TRUE(register_coalesce(v));
   EXPECT_EQ(0, block0->start_ip);
   EXPECT_EQ(2, block0->end_ip);

   fs_inst *add = instruction(block0, 0);
   EXPECT_EQ(BRW_OPCODE_ADD, add->opcode);
   EXPECT_TRUE(add->dst.equals(vgrf3));
   EXPECT_TRUE(add->src[0].equals(vgrf0));
   EXPECT_TRUE(add->src[1].equals(vgrf1));

   mov = instruction(block0, 1);
   EXPECT_EQ(BRW_OPCODE_MOV, mov->opcode);
   EXPECT_EQ(BRW_CONDITIONAL_NZ, mov->conditional_mod);
   EXPECT_TRUE(mov->dst.equals(bld.null_reg_f()));
   EXPECT_TRUE(mov->src[0].equals(vgrf3));

   fs_inst *mul = instruction(block0, 2);
   EXPECT_EQ(BRW_OPCODE_MUL, mul->opcode);
   EXPECT_TRUE(mul->dst.equals(vgrf4));
   EXPECT_TRUE(mul->src[0].equals(vgrf4));
   EXPECT_TRUE(mul->src[1].equals(vgrf3));
}
