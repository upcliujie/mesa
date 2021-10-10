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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * Lower constant arrays to uniform arrays.
 *
 * Some driver backends (such as i965 and nouveau) don't handle constant arrays
 * gracefully, instead treating them as ordinary writable temporary arrays.
 * Since arrays can be large, this often means spilling them to scratch memory,
 * which usually involves a large number of instructions.
 *
 * This must be called prior to gl_nir_set_uniform_initializers(); we need the
 * linker to process our new uniform's constant initializer.
 *
 * This should be called after optimizations, since those can result in
 * splitting and removing arrays that are indexed by constant expressions.
 */
#include "nir.h"
#include "nir_builder.h"
#include "nir_deref.h"

static bool
lower_const_array_to_uniform(nir_shader *shader, nir_variable *var,
                             struct hash_table *const_array_vars,
                             unsigned *free_uni_components,
                             unsigned *const_count, bool *progress)
{
   if (!var->data.read_only || !var->constant_initializer ||
       !glsl_type_is_array(var->type))
      return true;

   /* How many uniform component slots are required? */
   unsigned component_slots = glsl_get_component_slots(var->type);

   /* We would utilize more than is available, bail out. */
   if (component_slots > *free_uni_components)
      return false;

   *free_uni_components -= component_slots;

   /* In the very unlikely event of 4294967295 constant arrays in a single
    * shader, don't promote this to a uniform.
    */
   unsigned limit = ~0;
   if (*const_count == limit)
      return false;

   nir_variable *uni = rzalloc(shader, nir_variable);
   uni->constant_initializer = nir_constant_clone(var->constant_initializer, uni);
   uni->data.how_declared = nir_var_hidden;
   uni->data.read_only = true;
   uni->data.mode = nir_var_uniform;
   uni->type = var->type;
   uni->name = ralloc_asprintf(uni, "constarray_%x_%u",
                               *const_count, shader->info.stage);

   nir_shader_add_variable(shader, uni);

   *const_count = *const_count + 1;

   _mesa_hash_table_insert(const_array_vars, var, uni);

   *progress = true;

   return true;
}

static unsigned
count_uniforms(nir_shader *shader)
{
   unsigned total = 0;

   nir_foreach_variable_with_modes(var, shader, nir_var_uniform) {
      total += glsl_get_component_slots(var->type);
   }

   return total;
}

bool
nir_lower_const_arrays_to_uniforms(nir_shader *shader,
                                   unsigned max_uniform_components)
{
   bool progress = false;
   unsigned uniform_components = count_uniforms(shader);
   unsigned free_uni_components = max_uniform_components - uniform_components;
   unsigned const_count = 0;

   struct hash_table *const_array_vars =
      _mesa_hash_table_create(NULL, _mesa_hash_pointer, _mesa_key_pointer_equal);

   nir_foreach_function(function, shader) {
      if (function->impl) {
         nir_foreach_function_temp_variable(var, function->impl) {
            if (!lower_const_array_to_uniform(shader, var, const_array_vars,
                                              &free_uni_components, &const_count,
                                              &progress))
               break;
         }
      }
   }

   nir_foreach_variable_in_shader(var, shader) {
      if (var->data.mode != nir_var_shader_temp)
         continue;

      if (!lower_const_array_to_uniform(shader, var, const_array_vars,
                                        &free_uni_components, &const_count,
                                        &progress))
         break;

   }

   nir_foreach_function(function, shader) {
      if (function->impl) {
         nir_builder b;
         nir_builder_init(&b, function->impl);
         nir_foreach_block(block, function->impl) {
            nir_foreach_instr_safe(instr, block) {

               if (instr->type != nir_instr_type_intrinsic)
                  continue;

               nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
               if (intrin->intrinsic != nir_intrinsic_load_deref)
                  continue;

               nir_deref_instr *deref = nir_src_as_deref(intrin->src[0]);
               nir_variable *var = nir_deref_instr_get_variable(deref);

               struct hash_entry *entry =
                  _mesa_hash_table_search(const_array_vars, var);
               if (!entry)
                  continue;

               b.cursor= nir_before_instr(instr);

               nir_variable *uni = (nir_variable *) entry->data;
               nir_deref_instr *new_deref_instr = nir_build_deref_var(&b, uni);

               nir_deref_path path;
               nir_deref_path_init(&path, deref, NULL);
               assert(path.path[0]->deref_type == nir_deref_type_var);

               nir_deref_instr **p = &path.path[1];
               for (; *p; p++) {
                  if ((*p)->deref_type == nir_deref_type_array) {
                     new_deref_instr = nir_build_deref_array(&b, new_deref_instr,
                                                             (*p)->arr.index.ssa);
                  } else if ((*p)->deref_type == nir_deref_type_struct) {
                     new_deref_instr = nir_build_deref_struct(&b, new_deref_instr,
                                                              (*p)->strct.index);
                  } else {
                     unreachable("Unsupported deref type");
                 }
              }

               nir_deref_path_finish(&path);

               nir_ssa_def *new_def = nir_load_deref(&b, new_deref_instr);

               nir_ssa_def_rewrite_uses(&intrin->dest.ssa, new_def);
               nir_instr_remove(&intrin->instr);
            }
         }
      }
   }

   _mesa_hash_table_destroy(const_array_vars, NULL);

   return progress;
}
