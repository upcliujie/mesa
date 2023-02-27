/*
 * Copyright © 2022 Google LLC
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
#include "gl_nir.h"

/** @file
 *
 * Lowers ALU operations from 32-bit to 16-bit according to mediump/lowp
 * qualifiers on variables in ES shaders.
 *
 * Lowering of the values at rest in temporary variables is separately handled
 * by nir_lower_mediump_vars (shared with vulkan, which has similar
 * RelaxedPrecision decorations for variables and texture operations, but has
 * separate, simpler rules for ALU operations).
 */

static bool
nir_lower_mediump_alu_type_supported(nir_alu_type type, const gl_nir_lower_mediump_alu_options *options)
{
   switch (nir_alu_type_get_base_type(type)) {
   case nir_type_float:
      return options->fp16;
   case nir_type_int:
   case nir_type_uint:
      return options->int16;
   default:
      /* Return true for bools -- we want to allow ops with bools to be mediump, since they don't have a precision */
      return true;
   }
}

/* Returns the highest non-NONE precision of the two precision qualifiers.
 */
static uint32_t
merge_precision(uint32_t a, uint32_t b)
{
   if (a == GLSL_PRECISION_NONE)
      return b;
   if (b == GLSL_PRECISION_NONE)
      return a;

   /* Note: The ordering of the precisions is opposite what you might expect. */
   STATIC_ASSERT(GLSL_PRECISION_HIGH < GLSL_PRECISION_LOW);
   return MIN2(a, b);
}

static int
nir_alu_op_precision_num_inputs(nir_op op)
{
   switch (op) {
   case nir_op_ibitfield_extract:
   case nir_op_ubitfield_extract:
      /* "The precision qualification of the value returned from
       *  bitfieldExtract() matches the precision qualification of the call's
       *  input argument “value”."
       */
      return 1;

   case nir_op_bitfield_insert:
      /* "The precision qualification of the value returned from bitfieldInsert
       * matches the highest precision qualification of the call's input
       * arguments “base” and “insert”."
       */
      return 2;

   default:
      return nir_op_infos[op].num_inputs;
   }
}

static int
nir_intrinsic_precision_num_inputs(nir_intrinsic_op intr)
{
   switch (intr) {
   case nir_intrinsic_interp_deref_at_offset:
   case nir_intrinsic_interp_deref_at_sample:
      /* "For the interpolateAt* functions, the call will return a precision
       *  qualification matching the precision of the interpolant argument to the
       *  function call."
       */
      return 1;

   default:
      return nir_intrinsic_infos[intr].num_srcs;
   }
}

static uint32_t
nir_instr_operand_precision(nir_instr *instr, const gl_nir_lower_mediump_alu_options *options,
                            struct set *unqualified_temps)
{
   switch (instr->type) {

   case nir_instr_type_deref: {
      nir_deref_instr *deref = nir_instr_as_deref(instr);

      switch (deref->deref_type) {
      case nir_deref_type_var:
         return deref->var->data.precision;

      case nir_deref_type_array:
      case nir_deref_type_array_wildcard:
         return deref->parent.parent_instr->pass_flags;

      case nir_deref_type_struct:
         /* Precision qualifiers can only appear on float/int types, which
          * structures are not.  And structure members can't have explicit
          * precision qualifiers.  So, they're definitely unqualified.  (XXX:
          * What about a sampler in a struct?)
          */
         return GLSL_PRECISION_NONE;

      default:
         unreachable("unsupported deref type");
      }

      break;
   }

   /* "The precision used to internally evaluate an operation, and the precision
    *  qualification subsequently associated with any resulting intermediate
    *  values, must be at least as high as the highest precision qualification
    *  of the operands consumed by the operation."
    */
   case nir_instr_type_alu: {
      nir_alu_instr *alu = nir_instr_as_alu(instr);

      uint32_t precision = GLSL_PRECISION_NONE;
      nir_alu_type dest_type = nir_alu_type_get_base_type(nir_op_infos[alu->op].output_type);

      /* XXX: We could also do this type support checking at lowering time, if
       * lack of support for one op in a large expression tree should not keep
       * us from lowering the rest of the tree.
       */
      if (!nir_lower_mediump_alu_type_supported(dest_type, options))
         return GLSL_PRECISION_HIGH;

      for (int i = 0; i < nir_alu_op_precision_num_inputs(alu->op); i++) {
         nir_alu_type src_type = nir_alu_type_get_base_type(nir_op_infos[alu->op].input_types[i]);

         assert(alu->src[i].src.is_ssa);
         if (!nir_lower_mediump_alu_type_supported(src_type, options))
            return GLSL_PRECISION_HIGH;

         precision = merge_precision(precision, alu->src[i].src.ssa->parent_instr->pass_flags);
      }

      return precision;
   }

   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
      switch (intr->intrinsic) {
      case nir_intrinsic_load_deref:{
         nir_deref_instr *deref = nir_src_as_deref(intr->src[0]);
         return deref->instr.pass_flags;
      }

      /* GLSL IR generates unqualified temporaries for various purposes (vector
       * constructor temporaries, builtin function intermediate values), and we
       * need to infer the precision of the stores to them as if they were part
       * of the expression tree that generated those temps.  Merge in the
       * precisions of each store to the temp onto the var, and we'll propagate
       * the lvalue's precision back onto that tree later.
       *
       * Any function temps that the user declared should have received a
       * precision qualifier at AST-to-HIR time.
       */
      case nir_intrinsic_store_deref: {
         nir_deref_instr *deref = nir_src_as_deref(intr->src[0]);
         nir_instr *src = intr->src[1].ssa->parent_instr;

         nir_variable *var = nir_deref_instr_get_variable(deref);
         if (var && _mesa_set_search(unqualified_temps, var)) {
            var->data.precision = merge_precision(var->data.precision, src->pass_flags);
         }
         return src->pass_flags;
      }

      /* XXX: GLSL lower_precision reduces the highp qualifier on the builtin to
       * mediump based on the image's type.  Should we port that?  Is it really
       * valid to infer the precision of consuming operations differently from
       * the builtin's function signature, or should we just lower imageLoad's
       * bitsize with nir_fold_16bit_tex_image() later?
       */

      default: {
         uint32_t precision = GLSL_PRECISION_NONE;
         for (int i = 0; i < nir_intrinsic_precision_num_inputs(intr->intrinsic); i++) {
            /* XXX: type support checking */
            precision = merge_precision(precision, intr->src[i].ssa->parent_instr->pass_flags);
         }
         return precision;
      }
      }
   }

   /* XXX: inference for tex ops */

   case nir_instr_type_load_const:
      /* ES3.0 spec:
       *
       * "Literal constants do not have precision qualifiers. Neither do Boolean
       *  variables. Neither do floating point constructors nor integer
       *  constructors when none of the constructor arguments have precision
       *  qualifiers. For this paragraph, “operation” includes operators,
       *  built-in functions, and constructors, and “operand” includes function
       *  arguments and constructor arguments. The precision used to internally
       *  evaluate an operation, and the precision qualification subsequently
       *  associated with any resulting intermediate values, must be at least as
       *  high as the highest precision qualification of the operands consumed
       *  by the operation.
       *
       *  For constant expressions and sub-expressions, where the precision is
       *  not defined, the evaluation is performed at or above the highest
       *  supported precision of the target (either mediump or highp). The
       *  evaluation of constant expressions must be invariant and will usually
       *  be performed at compile time."
       *
       * but also:
       *
       * "Where the precision of a constant integral or constant floating point
       *  expression is not specified, evaluation is performed at highp. This
       *  rule does not affect the precision qualification of the expression."
       *
       * So, assuming that GLSL IR hasn't done any constant folding other than
       * constant expression evaluation (nor has any been done on NIR yet), then
       * we can treat constants here as unqualified.
       */
      return GLSL_PRECISION_NONE;

   default:
      return GLSL_PRECISION_HIGH;
   }
}

static void
nir_instr_update_uses_precision(nir_instr *instr)
{
   switch (instr->type) {
   /* "In other cases where operands do not have a precision qualifier, the
    *  precision qualification will come from the other operands. If no operands
    *  have a precision qualifier, then the precision qualifications of the
    *  operands of the next consuming operation in the expression will be used.
    *  This rule can be applied recursively until a precision qualified operand
    *  is found. If necessary, it will also include the precision qualification
    *  of l-values for assignments, of the declared variable for initializers,
    *  of formal parameters for function call arguments, or of function return
    *  types for function return values."
    *
    * Note that "the next consuming operation" section!  If (a + b) * c, and a
    * and b are unqualified, but c is highp, then a + b is highp.  So, we do the
    * default precision roots-to-leaves propagation after the leaves-to-roots
    * operand propagation, so that c's highp can get propagated to a + b.
    *
    * The deref instructions will have had their precision set in the forward
    * pass, and the language's default precision was applied by AST-to-HIR on
    * variables already.
    */
   case nir_instr_type_alu:
      if (instr->pass_flags == GLSL_PRECISION_NONE) {
         nir_alu_instr *alu = nir_instr_as_alu(instr);
         /* Note that the forward walk would have marked the instruction as
          * highp if mediump wasn't supported on its operand or dest types.
          */
         nir_foreach_use(src, &alu->dest.dest.ssa) {
            /* XXX: Apply nir_alu_op_precision_num_inputs() logic to
             * backwards prop, too.
             */
            instr->pass_flags = merge_precision(instr->pass_flags, src->parent_instr->pass_flags);
         }
      }
      break;

   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
      switch (intr->intrinsic) {
      /* Normally the lvalue's precision will have been set on the store_deref
       * instruction according to the rvalue of that particular store.  But, if
       * it was to an unqualified temporary, and we didn't have an inferred
       * qualifier on this particular rvalue, then the store is still at NONE
       * and we need to propagate backwards from what qualifier we settled on
       * based on other stores to that temp.
       */
      case nir_intrinsic_store_deref:
         if (instr->pass_flags == GLSL_PRECISION_NONE) {
            nir_deref_instr *deref = nir_src_as_deref(intr->src[0]);
            nir_variable *var = nir_deref_instr_get_variable(deref);
            if (var)
               instr->pass_flags = var->data.precision;
         }
         break;

      default:
            /* XXX: Apply nir_intrinsic_precision_num_inputs() logic to
             * backwards prop, too.
             */
         break;
      }
      break;
   }

   default:
      break;
   }
}

static bool
nir_lower_mediump_alu_instr(nir_builder *b, nir_instr *instr)
{
   if (instr->pass_flags != GLSL_PRECISION_MEDIUM && instr->pass_flags != GLSL_PRECISION_LOW)
      return false;

   if (instr->type == nir_instr_type_alu) {
      nir_alu_instr *alu = nir_instr_as_alu(instr);

      /* Some ops have a fixed dest size.  This doesn't affect GLSL's expression
       * tree precision inference rules, but it does affect whether we can lower
       * the op.
       */
      nir_alu_type dest_type = nir_op_infos[alu->op].output_type;
      if (nir_alu_type_get_type_size(dest_type) == 32)
         return false;

      /* XXX: driver-dependent lowering of derivatives. */

      switch (alu->op) {
      case nir_op_mov:
         /* Don't wrap a mov in down/upcasts, it won't help anything and just
          * makes for noise in the shader.
          */
         return false;
      default:
         break;
      }

      b->cursor = nir_before_instr(instr);
      /* Downcast our operands to 16 bits. */
      for (int i = 0; i < nir_op_infos[alu->op].num_inputs; i++) {
         switch (nir_alu_type_get_base_type(nir_op_infos[alu->op].input_types[i])) {
         case nir_type_float:
            nir_instr_rewrite_src_ssa(instr, &alu->src[i].src, nir_f2fmp(b, alu->src[i].src.ssa));
            break;
         case nir_type_int:
         case nir_type_uint:
            nir_instr_rewrite_src_ssa(instr, &alu->src[i].src, nir_i2imp(b, alu->src[i].src.ssa));
            break;
         case nir_type_bool:
            /* bools don't have lower precision. */
            break;
         default:
            break;
         }
      }

      /* Upcast our result to 32.  If we end up getting downcast to 16 again by
       * a consuming expr, nir_opt_algebraic will just eat that and eliminate
       * the casts.
       */
      if (nir_alu_type_get_base_type(dest_type) != nir_type_bool) {
         assert(alu->dest.dest.is_ssa);
         b->cursor = nir_after_instr(instr);
         nir_ssa_def *def = &alu->dest.dest.ssa;

         /* Update the bit size before we do the upconvert, or the upconvert
          * builder will skip it.
          */
         def->bit_size = 16;

         switch (nir_alu_type_get_base_type(dest_type)) {
         case nir_type_float:
            def = nir_f2f32(b, def);
            break;
         case nir_type_int:
            def = nir_i2i32(b, def);
            break;
         case nir_type_uint:
            def = nir_u2u32(b, def);
            break;
         default:
            unreachable("");
         }

         /* Rename our op if it was a 32-bit conversion before. */
         switch (alu->op) {
         case nir_op_u2f32:
            alu->op = nir_op_u2f16;
            break;
         case nir_op_i2f32:
            alu->op = nir_op_i2f16;
            break;
         case nir_op_f2i32:
            alu->op = nir_op_f2i16;
            break;
         case nir_op_f2u32:
            alu->op = nir_op_f2u16;
            break;
         default:
            break;
         }

         nir_ssa_def_rewrite_uses_after(&alu->dest.dest.ssa, def, def->parent_instr);
      }
   }

   return true;
}

static void
dump_instr_precision(nir_function_impl *impl, const char *step)
{
   /* comment out this return to dump mediump lowering state */
   return;

   fprintf(stderr, "Precisions for instructions after %s step:\n", step);
   nir_foreach_block(block, impl)
   {
      nir_foreach_instr(instr, block) {
         const char *precisions[] = {
             [GLSL_PRECISION_NONE] = "none",
             [GLSL_PRECISION_LOW] = "low",
             [GLSL_PRECISION_MEDIUM] = "med",
             [GLSL_PRECISION_HIGH] = "high",
         };
         fprintf(stderr, "  %5s ", precisions[instr->pass_flags]);
         nir_print_instr(instr, stderr);
         fprintf(stderr, "\n");
      }
   }
}

static bool
gl_nir_lower_mediump_alu_impl(nir_function_impl *impl, const gl_nir_lower_mediump_alu_options *options)
{
   bool progress = false;

   nir_builder b;
   nir_builder_init(&b, impl);

   struct set *unqualified_temps = _mesa_pointer_set_create(NULL);
   nir_foreach_function_temp_variable(var, impl) {
      if (var->data.precision == GLSL_PRECISION_NONE)
         _mesa_set_add(unqualified_temps, var);
   }

   /* First do a forwards walk (expression tree leaves to roots) where the
    * operands of instructions are examined for their precision qualifiers and
    * propagate that precision toward stores.
    */
   nir_foreach_block(block, impl) {
      nir_foreach_instr(instr, block) {
         instr->pass_flags = nir_instr_operand_precision(instr, options, unqualified_temps);
      }
   }

   dump_instr_precision(impl, "forward");

   ralloc_free(unqualified_temps);

   /* Then, walk backwards from roots to leaves propagating default precision
    * qualifiers into expression subtrees that didn't have their own precision
    * dictated by their operands.
    */
   nir_foreach_block_reverse(block, impl) {
      nir_foreach_instr_reverse(instr, block) {
         nir_instr_update_uses_precision(instr);
      }
   }

   dump_instr_precision(impl, "backward");

   /* Now that we've decided on the precisions of instructions, go through and
    * lower the ALU ops accordigly.
    */
   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         progress = nir_lower_mediump_alu_instr(&b, instr);
      }
   }

   if (progress) {
      nir_metadata_preserve(impl, nir_metadata_dominance | nir_metadata_block_index);
      progress = true;
   } else {
      nir_metadata_preserve(impl, nir_metadata_all);
   }

   return progress;
}

/* Lowers ALU operations to 16 bit according to GLSL source rules. For SPIR-V,
 * this is done in spirv-to-nir.c according to the RelaxedPrecision decorations,
 * instead.
 */
bool
gl_nir_lower_mediump_alu(nir_shader *s, const gl_nir_lower_mediump_alu_options *options)
{
   bool progress = false;

   nir_foreach_function(function, s) {
      if (!function->impl)
         continue;

      progress = gl_nir_lower_mediump_alu_impl(function->impl, options) || progress;
   }

   return progress;
}
