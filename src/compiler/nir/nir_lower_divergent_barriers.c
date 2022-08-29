/*
 * Copyright © 2022 Intel Corporation
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

/**
 * Lower workgroup barriers in divergent code paths into non divergent code
 * paths.
 *
 * This pass reuses the lower_shader_calls pass to build a series a
 * continuations that are put into a loop with a series of switch/case for
 * each divergent continuation. The barriers are put outside the divergent
 * switch/case, inside the loop. Using a local shared atomic, the loop exits
 * only when all threads have completed execution.
 */

#include "nir.h"
#include "nir_builder.h"

static void
write_return_value(nir_builder *b, unsigned cont_id)
{
   nir_deref_instr *ret_deref =
      nir_build_deref_cast(b, nir_load_param(b, 0),
                           nir_var_function_temp, glsl_int_type(), 0);
   nir_store_deref(b, ret_deref, nir_imm_int(b, cont_id), 0x1);
}

static struct exec_list *
get_cf_node_parent_child_list(nir_cf_node *node)
{
   nir_cf_node *cf_parent = node->parent;
   switch (cf_parent->type) {
   case nir_cf_node_if: {
      nir_if *_if = nir_cf_node_as_if(cf_parent);
      foreach_list_typed_safe(nir_cf_node, child, node, &_if->then_list) {
         if (child == node) {
            return &_if->then_list;
         }
      }
      foreach_list_typed_safe(nir_cf_node, child, node, &_if->else_list) {
         if (child == node) {
            return &_if->else_list;
         }
      }
      unreachable("Cannot find node in if/else children");
      break;
   }

   case nir_cf_node_loop: {
      nir_loop *loop = nir_cf_node_as_loop(cf_parent);
      return &loop->body;
   }

   case nir_cf_node_function: {
      nir_function_impl *func = nir_cf_node_as_function(cf_parent);
      return &func->body;
   }

   case nir_cf_node_block:
   default:
      unreachable("invalid cf node");
   }
}

static void
replace_call_with_return(nir_function_impl *impl)
{
   nir_builder b;
   nir_builder_init(&b, impl);

   /* Iterate blocks backward as we'll remove entire chunks of control flow
    * from one instruction down the the end of the current control flow
    * list.
    */
   nir_foreach_block_reverse_safe(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *call = nir_instr_as_intrinsic(instr);
         if (call->intrinsic != nir_intrinsic_rt_execute_callable)
            continue;

         struct exec_list *child_list =
            get_cf_node_parent_child_list(&block->cf_node);

         /* Remove anything after a call, as we should return the uniform part
          * of the function.
          */
         nir_cf_list cf_list;
         nir_cf_extract(&cf_list, nir_after_instr(instr),
                                  nir_after_cf_list(child_list));
         nir_cf_delete(&cf_list);

         b.cursor = nir_instr_remove(instr);
         write_return_value(&b, nir_intrinsic_call_idx(call) + 1);
         nir_jump(&b, nir_jump_return);
         break;
      }
   }
}

static void
remove_resume_instrs(nir_shader *shader)
{
   nir_foreach_function(function, shader) {
      if (!function->impl)
         continue;

      nir_builder b;
      nir_builder_init(&b, function->impl);

      nir_foreach_block_safe(block, function->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *call = nir_instr_as_intrinsic(instr);
            if (call->intrinsic != nir_intrinsic_rt_resume)
               continue;

            nir_instr_remove(instr);
         }
      }
   }
}

static bool
instr_is_divergent_barrier(nir_instr *instr)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   bool is_workgroup_barrier =
      intrin->intrinsic == nir_intrinsic_scoped_barrier &&
      nir_intrinsic_execution_scope(intrin) == NIR_SCOPE_WORKGROUP;
   if (!is_workgroup_barrier)
      return false;

   bool divergent = false;
   nir_cf_node *node = instr->block->cf_node.parent;
   while (node && !divergent) {
      switch (node->type) {
      case nir_cf_node_if: {
         nir_if *_if = nir_cf_node_as_if(node);
         if (nir_src_is_divergent(_if->condition))
            divergent = true;
         break;
      }

      case nir_cf_node_loop: {
         nir_loop *loop = nir_cf_node_as_loop(node);
         if (loop->divergent)
            divergent = true;
         break;
      }

      case nir_cf_node_function:
         break;

      default:
         unreachable("Invalid cf type");
      }

      node = node->parent;
   }

   return divergent;
}

static void
instr_rewrite_divergent_barrier(struct nir_builder *b,
                                nir_instr *instr,
                                unsigned call_idx,
                                unsigned offset,
                                void *data)
{
   nir_ssa_def *zero = nir_imm_int(b, 0);
   nir_rt_execute_callable(b, zero, zero,
                           .call_idx = call_idx, .stack_size = offset);
   nir_rt_resume(b, .call_idx = call_idx, .stack_size = offset);
   nir_instr_remove(instr);
}

struct divergent_state {
   struct nir_builder b;
   struct hash_table *block_to_cont;
   nir_variable      *continuation_var;
};

static void
set_continuation_function_params(nir_function *func)
{
   func->num_params = 1;
   func->params = ralloc_array(func->shader, nir_parameter, func->num_params);
   func->params[0].num_components = 1;
   func->params[0].bit_size = 32;
}

static nir_function_impl *
create_continuation_function(nir_shader *shader, nir_function_impl *old_impl, unsigned continuation)
{
   nir_function_impl *new_impl = nir_function_impl_clone(shader, old_impl);
   char *name = ralloc_asprintf(shader, "continuation_%u", continuation);
   nir_function *new_func = nir_function_create(shader, name);
   set_continuation_function_params(new_func);
   new_func->impl = new_impl;
   new_impl->function = new_func;

   nir_foreach_block(block, new_impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_deref)
            continue;

         nir_deref_instr *deref = nir_instr_as_deref(instr);
         if (deref->deref_type != nir_deref_type_var)
            continue;

         nir_variable *old_var = deref->var;
         if (old_var->data.mode & nir_var_function_temp)
            continue;

         nir_variable *new_var = nir_variable_clone(old_var, shader);
         nir_shader_add_variable(shader, new_var);
         deref->var = new_var;
      }
   }

   return new_impl;
}

static inline nir_ssa_def *
build_deref_var(nir_builder *b, nir_variable *var)
{
   nir_deref_instr *deref = nir_build_deref_var(b, var);
   return &deref->dest.ssa;
}

static nir_function_impl *
shader_get_function(nir_shader *shader)
{
   return exec_node_data(nir_function,
                         exec_list_get_head(&shader->functions), node)->impl;

}

static void
opt_shader(nir_shader *shader)
{
   bool progress;

#define OPT(pass, ...) ({                                  \
   bool this_progress = pass(shader, ##__VA_ARGS__);       \
   if (this_progress)                                      \
      progress = true;                                     \
   this_progress;                                          \
})

   do {
      progress = false;
      OPT(nir_opt_remove_phis);
      OPT(nir_opt_dead_cf);
      OPT(nir_opt_if, false);
      OPT(nir_copy_prop);
      OPT(nir_opt_dce);
      OPT(nir_opt_cse);
      OPT(nir_opt_algebraic);
      OPT(nir_lower_constant_convert_alu_types);
      OPT(nir_opt_constant_folding);
      OPT(nir_opt_gcm, false);
      OPT(nir_opt_undef);
      OPT(nir_opt_dead_write_vars);
   } while (progress);
#undef OPT
}

bool
nir_lower_divergent_barrier(nir_shader *shader,
                            nir_address_format address_format,
                            unsigned stack_alignment)
{
   unsigned num_calls;

   assert(exec_list_length(&shader->functions) == 1);

   nir_lower_global_vars_to_local(shader);

   if (!nir_lower_shader_split(shader,
                               instr_is_divergent_barrier,
                               instr_rewrite_divergent_barrier,
                               NULL /* rewrite_call_data */,
                               address_format,
                               stack_alignment,
                               &num_calls))
      return false;

   void *mem_ctx = ralloc_context(NULL);

   /* Set a return value on the main function */
   nir_function_impl *old_main_impl = nir_shader_get_entrypoint(shader);
   set_continuation_function_params(old_main_impl->function);

   {
      nir_builder b;
      nir_builder_init(&b, old_main_impl);
      b.cursor = nir_after_block(nir_impl_last_block(old_main_impl));
      write_return_value(&b, -1);
      nir_jump(&b, nir_jump_return);
   }

   /* Make N copies of our shader */
   nir_shader **resume_shaders = ralloc_array(mem_ctx, nir_shader *, num_calls);
   for (unsigned i = 0; i < num_calls; i++)
      resume_shaders[i] = nir_shader_clone(mem_ctx, shader);

   /* Put the old main function into another function called continuation_0
    * and all the resume shaders' main function into their own continuation_X
    * function.
    */
   unsigned num_continuations = num_calls + 1;
   nir_function_impl **continuations =
      ralloc_array(mem_ctx, nir_function_impl *, num_continuations);
   continuations[0] = old_main_impl;
   continuations[0]->function->name = ralloc_strdup(shader, "continuation_0");
   for (unsigned i = 1; i < num_continuations; i++) {
      nir_function_impl *cont_impl = shader_get_function(resume_shaders[i - 1]);
      nir_shader_call_lower_resume(cont_impl, i - 1);
      replace_call_with_return(cont_impl);
      continuations[i] =
         create_continuation_function(shader,
                                      shader_get_function(resume_shaders[i - 1]),
                                      i);
   }

   /* Also replace the calls in the first continuation */
   replace_call_with_return(shader_get_function(shader));

   /* Try to optimize the whole thing so it's nicer to look at (in particular
    * all the dead SSA).
    */
   opt_shader(shader);

   /* Create a new main function */
   nir_function *main_func =
      nir_function_create(shader, "main");
   nir_function_impl *new_main_impl = nir_function_impl_create(main_func);
   new_main_impl->function->is_entrypoint = true;
   old_main_impl->function->is_entrypoint = false;

   /* In the new main function, do a loop like this :
    *
    *   void main() {
    *      uint next = 0, done = 0;
    *      shared uint threads_done = 0;
    *      barrier(); // Important after initializing threads_done
    *      while (true) {
    *         switch (next) {
    *         case 0:
    *            next = continuation_0();
    *            break;
    *         case 1:
    *            next = continuation_1();
    *            break;
    *         ....
    *         }
    *         if (next == -1 && !done) {
    *            done = true;
    *            atomicAdd(threads_done, 1);
    *         }
    *         barrier();
    *         if (threads_done == workgroupSize)
    *            break;
    *         barrier();
    *      }
    *   }
    */
   nir_variable *continuation_var =
      nir_local_variable_create(new_main_impl, glsl_int_type(),
                                "continuation_id");
   nir_variable *thread_done_var =
      nir_local_variable_create(new_main_impl, glsl_bool_type(),
                                "thread_done");
   nir_variable *threads_ended_var =
      nir_variable_create(shader, nir_var_mem_shared,
                          glsl_int_type(), "threads_ended");

   nir_builder b;
   nir_builder_init(&b, new_main_impl);
   b.cursor = nir_before_block(nir_start_block(new_main_impl));
   nir_store_var(&b,  continuation_var,
                 nir_imm_int(&b, 0), 0x1 /* write_mask */);
   nir_store_var(&b,  thread_done_var,
                 nir_imm_bool(&b, false), 0x1 /* write_mask */);
   nir_store_var(&b,  threads_ended_var,
                 nir_imm_int(&b, 0x0), 0x1 /* write_mask */);

   nir_scoped_barrier(&b,
                      .execution_scope = NIR_SCOPE_WORKGROUP,
                      .memory_scope = NIR_SCOPE_WORKGROUP,
                      .memory_semantics = NIR_MEMORY_ACQ_REL,
                      .memory_modes = nir_var_mem_shared);

   nir_push_loop(&b);
   {
      nir_ssa_def *continuation_id = nir_load_var(&b, continuation_var);

      /* Push each continuation in a if block. */
      for (unsigned i = 0; i < num_continuations; i++) {
         nir_push_if(&b,  nir_ieq_imm(&b, continuation_id, i));
         {
            nir_call_instr *call_instr =
               nir_call_instr_create(shader, continuations[i]->function);
            nir_deref_instr *ret_deref =
               nir_build_deref_var(&b, continuation_var);
            call_instr->params[0] = nir_src_for_ssa(&ret_deref->dest.ssa);
            nir_instr_insert(b.cursor, &call_instr->instr);
         }
         nir_pop_if(&b, NULL);
      }

      continuation_id = nir_load_var(&b, continuation_var);
      nir_ssa_def *thread_done = nir_load_var(&b, thread_done_var);

      nir_push_if(&b, nir_iand(&b,
                               nir_ieq_imm(&b, continuation_id, -1),
                               nir_ieq_imm(&b, thread_done, false)));
      {
         nir_store_var(&b, thread_done_var,
                       nir_imm_bool(&b, true), 0x1 /* write_mask */);

         nir_intrinsic_instr *intrin =
            nir_intrinsic_instr_create(shader, nir_intrinsic_deref_atomic_add);
         intrin->src[0] = nir_src_for_ssa(build_deref_var(&b, threads_ended_var));
         intrin->src[1] = nir_src_for_ssa(nir_imm_int(&b, 1));
         nir_ssa_dest_init(&intrin->instr, &intrin->dest, 1, 32, NULL);
         nir_builder_instr_insert(&b, &intrin->instr);
      }
      nir_pop_if(&b, NULL);

      nir_scoped_barrier(&b,
                         .execution_scope = NIR_SCOPE_WORKGROUP,
                         .memory_scope = NIR_SCOPE_WORKGROUP,
                         .memory_semantics = NIR_MEMORY_ACQ_REL,
                         .memory_modes = nir_var_mem_shared);

      /* If all instances have reached the end of the shader, break end exit
       * the shader.
       */
      nir_intrinsic_instr *intrin =
         nir_intrinsic_instr_create(shader, nir_intrinsic_deref_atomic_add);
      intrin->src[0] = nir_src_for_ssa(build_deref_var(&b, threads_ended_var));
      intrin->src[1] = nir_src_for_ssa(nir_imm_int(&b, 0));
      nir_ssa_dest_init(&intrin->instr, &intrin->dest, 1, 32, NULL);
      nir_builder_instr_insert(&b, &intrin->instr);

      unsigned local_workgroup_size = shader->info.workgroup_size[0] *
                                      shader->info.workgroup_size[1] *
                                      shader->info.workgroup_size[2];
      nir_ssa_def *all_finished =
         nir_ball(&b, nir_ieq_imm(&b, &intrin->dest.ssa, local_workgroup_size));
      nir_push_if(&b, all_finished);
      {
         nir_jump(&b, nir_jump_break);
      }
      nir_pop_if(&b, NULL);

      nir_scoped_barrier(&b,
                         .execution_scope = NIR_SCOPE_WORKGROUP,
                         .memory_scope = NIR_SCOPE_WORKGROUP,
                         .memory_semantics = NIR_MEMORY_ACQ_REL,
                         .memory_modes = nir_var_mem_shared);

   }
   nir_pop_loop(&b, NULL);

   /* Remove the resume instructions which are not useful for this pass. */
   remove_resume_instrs(shader);

   /* Inline the whole thing */
   nir_lower_returns(shader);
   nir_inline_functions(shader);

   /* Pick off the single entrypoint that we want */
   foreach_list_typed_safe(nir_function, func, node, &shader->functions) {
      if (!func->is_entrypoint)
         exec_node_remove(&func->node);
   }
   assert(exec_list_length(&shader->functions) == 1);

   ralloc_free(mem_ctx);

   nir_metadata_preserve(new_main_impl, nir_metadata_none);

   return true;
}

static bool
remove_workgroup_barriers_impl(struct nir_builder *b,
                               nir_instr *instr,
                               UNUSED void *data)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   if (intrin->intrinsic != nir_intrinsic_scoped_barrier)
      return false;

   if (nir_intrinsic_execution_scope(intrin) != NIR_SCOPE_WORKGROUP)
      return false;

   nir_instr_remove(instr);
   return true;
}

bool
nir_has_divergent_barriers(nir_shader *shader)
{
   nir_foreach_function(function, shader) {
      if (!function->impl)
         continue;

      nir_foreach_block(block, function->impl) {
         nir_foreach_instr(instr, block) {
            if (instr_is_divergent_barrier(instr))
               return true;
         }
      }
   }

   return false;
}

bool
nir_remove_workgroup_barriers(nir_shader *shader)
{
   if (shader->info.stage != MESA_SHADER_COMPUTE &&
       shader->info.stage != MESA_SHADER_KERNEL)
      return false;

   return nir_shader_instructions_pass(shader,
                                       remove_workgroup_barriers_impl,
                                       nir_metadata_block_index | nir_metadata_dominance,
                                       NULL);


}
