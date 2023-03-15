/*
 * Copyright © 2023 Google LLC
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

#include "main/mtypes.h"
#include "standalone_scaffolding.h"
#include "ir.h"
#include "ir_optimization.h"
#include "nir.h"
#include "builtin_functions.h"
#include "nir.h"
#include "glsl_to_nir.h"
#include "nir_builder.h"
#include "program.h"

namespace
{
   class gl_nir_lower_mediump_test : public ::testing::Test
   {
   protected:
      gl_nir_lower_mediump_test();
      ~gl_nir_lower_mediump_test();

      struct gl_shader *compile_shader(GLenum type, const char *source);
      void compile(const char *source);

      struct gl_context local_ctx;
      struct gl_context *ctx;

      nir_alu_instr *find_op(nir_op op)
      {
         if (!nir)
            return NULL;

         nir_foreach_function(func, nir)
         {
            nir_foreach_block(block, func->impl)
            {
               nir_foreach_instr(instr, block)
               {
                  if (instr->type == nir_instr_type_alu)
                  {
                     nir_alu_instr *alu = nir_instr_as_alu(instr);
                     if (alu->op == op)
                        return alu;
                  }
               }
            }
         }
         return NULL;
      }

      uint32_t op_dest_bits(nir_op op)
      {
         nir_alu_instr *alu = find_op(op);
         EXPECT_TRUE(alu != NULL);
         return alu->dest.dest.ssa.bit_size;
      }

      /* Returns the common bit size of all src operands (failing if not matching). */
      uint32_t op_src_bits(nir_op op)
      {
         nir_alu_instr *alu = find_op(op);
         EXPECT_TRUE(alu != NULL);

         for (int i = 0; i < nir_op_infos[op].num_inputs; i++) {
            EXPECT_EQ(alu->src[i].src.ssa->bit_size, alu->src[0].src.ssa->bit_size);
         }
         return alu->src[0].src.ssa->bit_size;
      }

      nir_shader *nir;
      struct gl_shader_program *whole_program;
      const char *source;
   };

   gl_nir_lower_mediump_test::gl_nir_lower_mediump_test()
   {
      glsl_type_singleton_init_or_ref();

      nir = NULL;
      source = NULL;
   }

   gl_nir_lower_mediump_test::~gl_nir_lower_mediump_test()
   {
      if (HasFailure())
      {
         printf("\nSource for the failed test:\n%s\n", source);
         if (nir) {
            printf("\nNIR from the failed test:\n\n");
            nir_print_shader(nir, stdout);
         }
      }

      ralloc_free(nir);

      glsl_type_singleton_decref();
   }

   struct gl_shader *
   gl_nir_lower_mediump_test::compile_shader(GLenum type, const char *source)
   {
      struct gl_shader *shader = standalone_add_shader_source(ctx, whole_program, type, source);

      _mesa_glsl_compile_shader(ctx, shader, false, false, true);

      return shader;
   }

   void
   gl_nir_lower_mediump_test::compile(const char *source)
   {
      ctx = &local_ctx;

      /* Get better variable names from GLSL IR for debugging. */
      ir_variable::temporaries_allocate_names = true;

      initialize_context_to_defaults(ctx, API_OPENGLES2);
      ctx->Version = 31;
      _mesa_glsl_builtin_functions_init_or_ref();

      whole_program = standalone_create_shader_program();
      whole_program->IsES = true;

      const char *vs_source = R"(#version 310 es
      void main() {
         gl_Position = vec4(0.0);
      })";
      compile_shader(GL_VERTEX_SHADER, vs_source);

      compile_shader(GL_FRAGMENT_SHADER, source);

      for (unsigned i = 0; i < whole_program->NumShaders; i++)
      {
         struct gl_shader *shader = whole_program->Shaders[i];
         if (shader->CompileStatus != COMPILE_SUCCESS)
            fprintf(stderr, "Compiler error: %s", shader->InfoLog);
         ASSERT_EQ(shader->CompileStatus, COMPILE_SUCCESS);
      }

      link_shaders(ctx, whole_program);
      if (whole_program->data->LinkStatus != LINKING_SUCCESS)
         fprintf(stderr, "Linker error: %s", whole_program->data->InfoLog);
      EXPECT_EQ(whole_program->data->LinkStatus, LINKING_SUCCESS);

      for (unsigned i = 0; i < ARRAY_SIZE(whole_program->_LinkedShaders); i++) {
         struct gl_linked_shader *sh = whole_program->_LinkedShaders[i];
         if (!sh)
            continue;

         do_mat_op_to_vec(sh->ir);
      }


      /* glsl_to_nir frees the GLSL IR, so if you need to look at it to debug a
       * test, do it here.
       */
      if (false) {
         printf("GLSL IR for linked FS:\n");
         _mesa_print_ir(stdout, whole_program->_LinkedShaders[MESA_SHADER_FRAGMENT]->ir, NULL);
      }

      static const struct nir_shader_compiler_options compiler_options = {
          .support_16bit_alu = true,
      };

      nir = glsl_to_nir(&ctx->Const, whole_program, MESA_SHADER_FRAGMENT, &compiler_options);

      /* nir_lower_mediump_vars happens after copy deref lowering. */
      NIR_PASS_V(nir, nir_split_var_copies);
      NIR_PASS_V(nir, nir_lower_var_copies);

      /* Make the vars and i/o mediump like we'd expect, so people debugging aren't confused. */
      NIR_PASS_V(nir, nir_lower_mediump_vars, nir_var_uniform | nir_var_function_temp | nir_var_shader_temp);
      NIR_PASS_V(nir, nir_lower_mediump_io, nir_var_shader_out, ~0, false);

      /* Clean up f2fmp(f2f32(x)) noise. */
      NIR_PASS_V(nir, nir_opt_algebraic);
      NIR_PASS_V(nir, nir_opt_algebraic_late);
      NIR_PASS_V(nir, nir_copy_prop);
      NIR_PASS_V(nir, nir_opt_dce);

      /* Store the source for printing from later assertions. */
      this->source = source;
   }

} // namespace

TEST_F(gl_nir_lower_mediump_test, float_simple_mul)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         uniform mediump float a, b;
         out mediump float result;

         void main()
         {
            result = a * b;
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
}

TEST_F(gl_nir_lower_mediump_test, int_simple_mul)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         precision mediump int;
         uniform mediump int a, b;
         out mediump int result;

         void main()
         {
            result = a * b;
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_imul), 16);
}

TEST_F(gl_nir_lower_mediump_test, int_default_precision_med)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         precision mediump int;
         uniform int a, b;
         out int result;

         void main()
         {
            result = a * b;
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_imul), 16);
}

TEST_F(gl_nir_lower_mediump_test, int_default_precision_high)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision mediump float;
         precision highp int;
         uniform int a, b;
         out int result;

         void main()
         {
            result = a * b;
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_imul), 32);
}

/* Test that a builtin with mediump args does mediump computation. */
TEST_F(gl_nir_lower_mediump_test, dot_builtin)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         precision highp int;
         uniform mediump vec4 a, b;
         out float result;

         void main()
         {
            result = dot(a, b);
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fdot4), 16);
}

/* Test that a constant-index array deref is mediump */
TEST_F(gl_nir_lower_mediump_test, array_const_index)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         precision highp int;
         uniform mediump float a, b[2];
         out float result;

         void main()
         {
            result = a * b[1];
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
}

/* Test that a variable-index array deref is mediump, even if the array index is highp */
TEST_F(gl_nir_lower_mediump_test, array_var_index)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         uniform mediump float a, b[2];
         uniform highp int i;
         out float result;

         void main()
         {
            result = a * b[i];
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
}

TEST_F(gl_nir_lower_mediump_test, func_return)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float; /* Make sure that default highp temps in function handling don't break our mediump return. */
         uniform mediump float a;
         uniform highp float b;
         out float result;

         mediump float func()
         {
            return b; /* Returning highp b here, but it should be the mediump return value qualifier that matters */
         }

         void main()
         {
            /* "If a function returns a value, then a call to that function may
             *  be used as an expression, whose type will be the type that was
             *  used to declare or define the function."
             */
            result = a * func();
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
}

TEST_F(gl_nir_lower_mediump_test, func_args_in_mediump)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float; /* Make sure that default highp temps in function handling don't break our mediump return. */
         uniform highp float a, b;
         out float result;

         highp float func(mediump float x, mediump float y)
         {
            return x * y; /* should be mediump due to x and y, but propagating qualifiers from a,b by inlining could trick it. */
         }

         void main()
         {
            result = func(a, b);
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
}

TEST_F(gl_nir_lower_mediump_test, func_args_inout_mediump)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float; /* Make sure that default highp temps in function handling don't break our mediump inout. */
         uniform highp float a, b;
         out float result;

         void func(inout mediump float x, mediump float y)
         {
            x = x * y; /* should be mediump due to x and y, but propagating qualifiers from a,b by inlining could trick it. */
         }

         void main()
         {
            /* The spec says "function input and output is done through copies,
             * and therefore qualifiers do not have to match."  So we use a
             * highp here for our mediump inout.
             */
            highp float x = a;
            func(x, b);
            result = x;
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
}

TEST_F(gl_nir_lower_mediump_test, func_args_inout_highp)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision mediump float; /* Make sure that default mediump temps in function handling don't break our highp inout. */
         uniform mediump float a, b;
         out float result;

         void func(inout highp float x, highp float y)
         {
            x = x * y; /* should be highp due to x and y, but propagating qualifiers from a,b by inlining could trick it. */
         }

         void main()
         {
            mediump float x = a;
            func(x, b);
            result = x;
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 32);
}

TEST_F(gl_nir_lower_mediump_test, if_mediump)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         uniform mediump float a, b, c;
         out float result;

         void main()
         {
            if (a * b < c)
               result = 1.0;
            else
               result = 0.0;
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
   EXPECT_EQ(op_src_bits(nir_op_flt), 16);
}

TEST_F(gl_nir_lower_mediump_test, mat_mul_mediump)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         uniform mediump mat2 a;
         uniform mediump vec2 b;
         out highp vec2 result;

         void main()
         {
            result = a * b;
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
}

TEST_F(gl_nir_lower_mediump_test, struct_default_precision_lvalue)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         precision mediump int;
         struct S {
            float x, y;
            int z, w;
         };
         uniform S a;
         out mediump vec2 result;

         void main()
         {
            /* I believe that structure members don't have a precision
             * qualifier, so we expect the precision of these operations to come
             * from the lvalue (which is higher precedence than the default
             * precision).
             */
            mediump float resultf = a.x * a.y;
            highp int resulti = a.z * a.w;
            result = vec2(resultf, float(resulti));
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
   EXPECT_EQ(op_dest_bits(nir_op_imul), 32);
}

TEST_F(gl_nir_lower_mediump_test, float_constructor)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision mediump float;
         uniform highp uint a;
         uniform mediump float b;
         out mediump float result;

         void main()
         {
            /* It's tricky to reconcile these two bits of spec: "Literal
             * constants do not have precision qualifiers. Neither do Boolean
             * variables. Neither do constructors."
             *
             * and
             *
             * "For this paragraph, “operation” includes operators, built-in
             * functions, and constructors, and “operand” includes function
             * arguments and constructor arguments."
             *
             * I take this to mean that the language doesn't let you put a
             * precision qualifier on a constructor (or literal), but the
             * constructor operation gets precision qualification inference
             * based on its args like normal.
             */
            result = float(a) * b;
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 32);
}

TEST_F(gl_nir_lower_mediump_test, vec2_constructor)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision mediump float;
         uniform highp float a, b;
         uniform mediump float c;
         out mediump vec2 result;

         void main()
         {
            result = c * vec2(a, b);
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 32);
}
TEST_F(gl_nir_lower_mediump_test, vec4_of_float_constructor)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision mediump float;
         uniform highp float a;
         uniform mediump float b;
         out mediump vec4 result;

         void main()
         {
            result = b * vec4(a);
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 32);
}

TEST_F(gl_nir_lower_mediump_test, vec4_of_vec2_constructor)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision mediump float;
         uniform highp vec2 a, b;
         uniform mediump vec4 c;
         out mediump vec4 result;

         void main()
         {
            /* GLSL IR has to either have a temp for a*b, or clone the
             * expression and let it get CSEed later.  If it chooses temp, that
             * may confuse us.
             */
            result = c + vec4(a * b, 0.0, 0.0);
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 32);
   EXPECT_EQ(op_dest_bits(nir_op_fadd), 32);
}

TEST_F(gl_nir_lower_mediump_test, float_literal_mediump)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         uniform mediump float a;
         out highp float result;

         void main()
         {
            /* The literal is unqualified, so it shouldn't promote the expression to highp. */
            result = a * 2.0;
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
}

TEST_F(gl_nir_lower_mediump_test, float_const_highp)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         uniform mediump float a;
         out highp float result;

         void main()
         {
            highp float two = 2.0;
            /* The constant is highp, so even with constant propagation the expression should be highp. */
            result = a * two;
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 32);
}

TEST_F(gl_nir_lower_mediump_test, float_const_expr_mediump)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         uniform mediump float a;
         out highp float result;

         void main()
         {
            /* "Where the precision of a constant integral or constant floating
             * point expression is not specified, evaluation is performed at
             * highp. This rule does not affect the precision qualification of the
             * expression."
             * So the 5.0 is calculated at highp, but a * 5.0 is calculated at mediump.
             */
            result = a * (2.0 + 3.0);
         }
    )"));

   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
}

TEST_F(gl_nir_lower_mediump_test, unpackUnorm4x8)
{
   ASSERT_NO_FATAL_FAILURE(compile(
       R"(#version 310 es
         precision highp float;
         uniform highp uint a;
         uniform mediump float b;
         out highp float result;

         void main()
         {
            result = unpackUnorm4x8(a).x * b;
         }
    )"));

   /* XXX: NIR insists that unorm_4x8 returns 32 bits per channel. */
   EXPECT_EQ(op_dest_bits(nir_op_unpack_unorm_4x8), 32);
   EXPECT_EQ(op_dest_bits(nir_op_fmul), 16);
}

/* XXX: Add unit tests getting at precision of temporaries inside builtin function impls. */
/* XXX: Add unit tests getting at precision of any other temps internally generated by the compiler */
/* XXX: Add unit tests checking for default precision on user-declared function temps*/
