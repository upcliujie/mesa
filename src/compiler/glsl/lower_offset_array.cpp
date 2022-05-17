/*
 * Copyright © 2013 Intel Corporation
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

/**
 * \file lower_offset_array.cpp
 *
 * IR lower pass to decompose ir_texture ir_tg4 with an array of offsets
 * into four ir_tg4s with a single ivec2 offset, select the .w component of each,
 * and return those four values packed into a gvec4.
 *
 * \author Chris Forbes <chrisf@ijw.co.nz>
 */

#include "compiler/glsl_types.h"
#include "ir.h"
#include "ir_builder.h"
#include "ir_optimization.h"
#include "ir_rvalue_visitor.h"
#include "glsl_symbol_table.h"

using namespace ir_builder;

class lower_offset_array_visitor : public ir_rvalue_visitor {
public:
   lower_offset_array_visitor(glsl_symbol_table *symbols)
      : mem_ctx(NULL), intrin(NULL), progress(false)
   {
      intrin = symbols->get_function("__intrinsic_sparse_residency_code_and");
   }

   ir_dereference_record *
   record_ref(ir_variable *var, const char *field)
   {
      return new(mem_ctx) ir_dereference_record(var, field);
   }

   ir_dereference_variable *var_ref(ir_variable *var)
   {
      return new(mem_ctx) ir_dereference_variable(var);
   }

   ir_call *
   call(ir_function *f, ir_variable *ret, exec_list params)
   {
      exec_list actual_params;

      foreach_in_list_safe(ir_instruction, ir, &params) {
         ir_dereference_variable *d = ir->as_dereference_variable();
         if (d != NULL) {
            d->remove();
            actual_params.push_tail(d);
         } else {
            ir_variable *var = ir->as_variable();
            assert(var != NULL);
            actual_params.push_tail(var_ref(var));
         }
      }

      ir_function_signature *sig =
         f->exact_matching_signature(NULL, &actual_params);
      if (!sig)
         return NULL;

      ir_dereference_variable *deref =
         (sig->return_type->is_void() ? NULL : var_ref(ret));

      return new(mem_ctx) ir_call(sig, deref, &actual_params);
   }

   void handle_rvalue(ir_rvalue **rv);

   void *mem_ctx;
   ir_function *intrin;
   bool progress;
};

void
lower_offset_array_visitor::handle_rvalue(ir_rvalue **rv)
{
   if (*rv == NULL || (*rv)->ir_type != ir_type_texture)
      return;

   ir_texture *ir = (ir_texture *) *rv;
   if (ir->op != ir_tg4 || !ir->offset || !ir->offset->type->is_array())
      return;

   void *mem_ctx = ralloc_parent(ir);

   this->mem_ctx = mem_ctx;

   ir_variable *var =
      new (mem_ctx) ir_variable(ir->type, "result", ir_var_temporary);
   base_ir->insert_before(var);

   if (!ir->is_sparse) {
      for (int i = 0; i < 4; i++) {
         ir_texture *tex = ir->clone(mem_ctx, NULL);
         tex->offset = new (mem_ctx) ir_dereference_array(tex->offset,
                                                          new (mem_ctx) ir_constant(i));

         base_ir->insert_before(assign(var, swizzle_w(tex), 1 << i));
      }
   } else {
      ir_variable *tmp_var =
         new (mem_ctx) ir_variable(ir->type, "tmp_var", ir_var_temporary);
      ir_variable *tmp_code =
         new (mem_ctx) ir_variable(glsl_type::int_type, "tmp_code", ir_var_temporary);

      base_ir->insert_before(tmp_var);

      for (int i = 0; i < 4; i++) {
         ir_texture *tex = ir->clone(mem_ctx, NULL);
         tex->offset = new (mem_ctx) ir_dereference_array(tex->offset,
                                                          new (mem_ctx) ir_constant(i));

         base_ir->insert_before(assign(tmp_var, tex));

         if (i == 0) {
            base_ir->insert_before(assign(record_ref(var, "code"),
                                          record_ref(tmp_var, "code")));
         } else {
            exec_list parameters;

            parameters.push_tail(record_ref(var, "code"));
            parameters.push_tail(record_ref(tmp_var, "code"));

            base_ir->insert_before(call(intrin, tmp_code, parameters));

            base_ir->insert_before(assign(record_ref(var, "code"), tmp_code));
         }

         base_ir->insert_before(assign(record_ref(var, "texel"),
                                       swizzle_w(record_ref(tmp_var, "texel")),
                                       1 << i));
      }
   }

   *rv = new (mem_ctx) ir_dereference_variable(var);

   progress = true;
}

bool
lower_offset_arrays(exec_list *instructions, glsl_symbol_table *symbols)
{
   lower_offset_array_visitor v(symbols);

   visit_list_elements(&v, instructions);

   return v.progress;
}
