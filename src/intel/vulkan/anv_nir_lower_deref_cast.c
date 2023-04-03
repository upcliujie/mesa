/*
 * Copyright © 2023 Intel Corporation
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

#include "anv_nir.h"
#include "nir_builder.h"

typedef struct load_srcs {
   nir_instr* instr;
   nir_src** srcs;
   unsigned num_srcs;
} load_srcs;

static bool
are_all_uses_load(nir_ssa_def* def)
{
   bool result = true;

   if (!list_is_empty(&def->if_uses))
      return false;

   nir_foreach_use(use_src, def) {
      result &= use_src->parent_instr->type == nir_instr_type_intrinsic &&
                nir_instr_as_intrinsic(use_src->parent_instr)->intrinsic ==
                nir_intrinsic_load_deref;
   }

   return result;
}

static bool
find_load_deref_srcs(nir_ssa_def* load_deref,
                     nir_src* deref_src,
                     struct load_srcs* load_srcs)
{
   bool success = true;

   switch(deref_src->parent_instr->type) {
   case nir_instr_type_alu: {
      nir_alu_instr* alu = nir_instr_as_alu(deref_src->parent_instr);
      load_srcs->instr = &alu->instr;
      load_srcs->srcs = rzalloc_array(NULL, nir_src *,
                                      nir_op_infos[alu->op].num_inputs);

      for(unsigned i = 0; i < nir_op_infos[alu->op].num_inputs; i++) {
         if (alu->src[i].src.ssa->index == load_deref->index) {
            load_srcs->srcs[load_srcs->num_srcs] = &alu->src[i].src;
            load_srcs->num_srcs++;
         }
      }
      break;
   }
   case nir_instr_type_phi: {
      nir_phi_instr* phi = nir_instr_as_phi(deref_src->parent_instr);
      load_srcs->instr = &phi->instr;
      load_srcs->srcs = rzalloc_array(NULL, nir_src *,
                                      exec_list_length(&phi->srcs));

      nir_foreach_phi_src(phi_src, phi) {
         if (phi_src->src.ssa->index == load_deref->index) {
            load_srcs->srcs[load_srcs->num_srcs] = &phi_src->src;
            load_srcs->num_srcs++;
         }
      }
      break;
   }
   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr* intrin =
         nir_instr_as_intrinsic(deref_src->parent_instr);

      switch(intrin->intrinsic) {
      case nir_intrinsic_store_deref: {
         load_srcs->instr = &intrin->instr;
         load_srcs->srcs = rzalloc(NULL, nir_src *);
         load_srcs->srcs[0] = &intrin->src[1];
         load_srcs->num_srcs++;
         break;
      }
      default:
         success = false;
      }
      break;
   }
   default:
      success = false;
   }

   return success;
}

static bool
find_bit_size(const struct set* deref_srcs, uint32_t *ret)
{
   set_foreach(deref_srcs, entry) {
      struct load_srcs* srcs = (struct load_srcs *) entry->key;

      for(unsigned j = 0; j < srcs->num_srcs; j++) {
         unsigned temp_size = srcs->srcs[j]->ssa->bit_size;

         if(*ret == 0)
            *ret = temp_size;

         if (*ret != temp_size)
            return false;
      }
   }

   return true;
}

static nir_deref_instr *
find_deref_cast(nir_instr* instr)
{
   nir_deref_instr *ret = NULL;

   if (instr->type != nir_instr_type_deref)
      return NULL;

   ret = nir_instr_as_deref(instr);

   if (ret->deref_type != nir_deref_type_cast)
      return NULL;

   assert(ret->dest.is_ssa);

   if (!are_all_uses_load(&ret->dest.ssa))
      return NULL;

   return ret;
}

static nir_intrinsic_instr *
find_load_param(nir_deref_instr *deref_cast)
{
    nir_intrinsic_instr *ret = NULL;

   if (deref_cast->parent.ssa->parent_instr->type !=
       nir_instr_type_intrinsic)
      return NULL;

   ret =
      nir_instr_as_intrinsic(deref_cast->parent.ssa->parent_instr);

   if(ret->intrinsic != nir_intrinsic_load_param)
      return NULL;

   return ret;
}

static bool
lower_deref_cast(nir_builder *b, nir_instr *instr, UNUSED void *_data)
{
   nir_function *func = b->impl->function;
   nir_deref_instr *deref_cast = NULL;
   nir_intrinsic_instr *load_param = NULL;
   uint32_t bit_size = 0;

   deref_cast = find_deref_cast(instr);

   if (!deref_cast)
      return false;

   load_param = find_load_param(deref_cast);

   if (!load_param)
      return false;

   struct set *load_params =
      _mesa_set_create(NULL, _mesa_hash_pointer,
                                                 _mesa_key_pointer_equal);
   struct set *load_derefs =
      _mesa_set_create(NULL, _mesa_hash_pointer,
                                _mesa_key_pointer_equal);
   struct set *deref_srcs =
      _mesa_set_create(NULL, _mesa_hash_pointer,
                                _mesa_key_pointer_equal);

   nir_foreach_use(use_src, &deref_cast->dest.ssa) {
      nir_instr *use_instr = use_src->parent_instr;

      if (use_instr->type != nir_instr_type_intrinsic)
         continue;

      nir_intrinsic_instr *load_deref =
      nir_instr_as_intrinsic(use_instr);

      nir_foreach_use(use_src2, &load_deref->dest.ssa) {
         struct load_srcs* srcs =
            rzalloc(NULL, struct load_srcs);

         if (find_load_deref_srcs(&load_deref->dest.ssa,
                                  use_src2, srcs)) {
            _mesa_set_add(deref_srcs, srcs);
         }
      }

      _mesa_set_add(load_derefs, &load_deref->instr);
   }

   if (!find_bit_size(deref_srcs, &bit_size)) {
      _mesa_set_destroy(load_params, NULL);
      _mesa_set_destroy(deref_srcs, NULL);
      _mesa_set_destroy(load_derefs, NULL);
      return false;
   }

   /* Change bit_size of the shader param and make new load_param instruction */
   func->params[nir_intrinsic_param_idx(load_param)].bit_size = bit_size;

   b->cursor = nir_before_instr(&load_param->instr);
   nir_ssa_def* new_load_param =
      nir_build_load_param(b,
                           load_param->num_components,
                           bit_size,
                           nir_intrinsic_param_idx(load_param));

   _mesa_set_add(load_params, &load_param->instr);

   /* Rewrite load_deref sources */
   set_foreach(deref_srcs, entry) {
      struct load_srcs* srcs = (struct load_srcs *) entry->key;

      for(unsigned j = 0; j < srcs->num_srcs; j++) {
         nir_instr_rewrite_src(srcs->instr, srcs->srcs[j],
                               nir_src_for_ssa(new_load_param));
      }
      ralloc_free(srcs->srcs);
   }

   /* Remove load_deref instructions */
   set_foreach(load_derefs, entry) {
      nir_instr_remove((nir_instr *) entry->key);
   }

   _mesa_set_destroy(deref_srcs, NULL);
   _mesa_set_destroy(load_derefs, NULL);

   /* Remove deref_cast instructions */
   nir_instr_remove(&deref_cast->instr);

   set_foreach(load_params, entry) {
      nir_instr_remove((nir_instr *) entry->key);
   }

   _mesa_set_destroy(load_params, NULL);

   return true;
}

/* This pass analyzes deref_cast(function_temp) instructions and converts
 * them into simple pair of load_param and unpack_* instructions.
 *
 * 1) The pass analyzes the SSA variable produced by the load_param
 *    intrinsic to identify whether it is used as a source for deref_cast
 *    and is cast into a different type, such as uint64_t*, and then cast
 *    back to the original type (e.g., uint32_t).
 *
 * 2) If such a sequence is found, the bit_size and destination
 *    of the load_param instruction will be rewritten to match
 *    the bit_size required for the sources of the unpack_* instruction.
 *
 * Bellow is an example of how this pass works.
 *
 * vec1 32 ssa_0 = intrinsic load_param () (param_idx=1) (bit_size=32)
 * vec1 32 ssa_1 = deref_cast (uint64_t *)ssa_0 (function_temp uint64_t)
 * vec1 64 ssa_2 = intrinsic load_deref (ssa_1) (access=0)
 * vec2 32 ssa_3 = unpack_64_2x32 ssa_2
 *
 * into:
 *
 * vec1 64 ssa_0 = intrinsic load_param () (param_idx=1) (bit_size=64)
 * vec2 32 ssa_1 = unpack_64_2x32 ssa_0
 */

bool
anv_nir_lower_deref_cast(nir_shader* shader) {
   return nir_shader_instructions_pass(shader, lower_deref_cast,
                                       nir_metadata_none,
                                       NULL);
}
