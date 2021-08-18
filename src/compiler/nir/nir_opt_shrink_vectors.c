/*
 * Copyright © 2020 Google LLC
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
 * @file
 *
 * Trims off the unused trailing components of SSA defs.
 *
 * Due to various optimization passes (or frontend implementations,
 * particularly prog_to_nir), we may have instructions generating vectors
 * whose components don't get read by any instruction. As it can be tricky
 * to eliminate unused low components or channels in the middle of a writemask
 * (you might need to increment some offset from a load_uniform, for example),
 * it is trivial to just drop the trailing components. For vector ALU only used
 * by ALU, this pass eliminates arbitrary channels and reswizzles the uses.
 *
 * This pass is probably only of use to vector backends -- scalar backends
 * typically get unused def channel trimming by scalarizing and dead code
 * elimination.
 */

#include "nir.h"
#include "nir_builder.h"

static void
reswizzle_alu_uses(nir_ssa_def *def, uint8_t *reswizzle)
{
   nir_foreach_use(use_src, def) {
      assert(use_src->parent_instr->type == nir_instr_type_alu);
      nir_alu_src *alu_src = (nir_alu_src*)use_src;
      for (unsigned i = 0; i < NIR_MAX_VEC_COMPONENTS; i++)
         alu_src->swizzle[i] = reswizzle[alu_src->swizzle[i]];
   }
}

static bool
alu_writemask_cb(nir_src *src, void *defs_live)
{
   /* if the src instr is not ALU: just set pass_flags */
   if (src->ssa->parent_instr->type != nir_instr_type_alu) {
      src->ssa->parent_instr->pass_flags = 1;
      return true;
   }

   unsigned writemask = 0;
   /* if the current instr is not ALU: set src write_mask for all channels */
   if (src->parent_instr->type != nir_instr_type_alu) {
      writemask = BITFIELD_MASK(src->ssa->num_components);
      nir_instr_as_alu(src->ssa->parent_instr)->dest.write_mask |= writemask;
      src->ssa->parent_instr->pass_flags = 1;
      return true;
   }

   /* both instructions are ALU: set the write_mask for used channels */
   nir_alu_instr *instr = nir_instr_as_alu(src->parent_instr);
   nir_alu_src* alu_src = (nir_alu_src*)src;
   assert(instr->dest.write_mask);
   if (nir_op_infos[instr->op].output_size == 0) {
      for (unsigned i = 0; i < instr->dest.dest.ssa.num_components; i++) {
         if ((instr->dest.write_mask >> i) & 0x1)
            writemask |= (1 << alu_src->swizzle[i]);
      }
   } else if (nir_op_is_vec(instr->op)) {
      /* check if this component is live */
      unsigned src_index = alu_src - &instr->src[0];
      if ((instr->dest.write_mask >> src_index) & 0x1)
         writemask = (1 << alu_src->swizzle[0]);
      else
         return true;
   } else {
      writemask = BITFIELD_MASK(src->ssa->num_components);
   }
   assert(writemask);

   nir_instr_as_alu(src->ssa->parent_instr)->dest.write_mask |= writemask;
   src->ssa->parent_instr->pass_flags = 1;
   return true;
}

/* Re-initialize the write_mask of ALU instructions:
 * This function performs a per-component dead code analysis,
 * in order to mask out unused channels from ALU instructions.
 *
 * pass_flags - indicates whether some SSA is used at all
 * write_mask - indicates which ALU components are being used
 */
static void
init_alu_writemask(nir_function_impl *impl)
{
   /* initialize pass flags and write_masks */
   nir_foreach_block(block, impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type == nir_instr_type_intrinsic) {
            nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
            const nir_intrinsic_info *info = &nir_intrinsic_infos[intrin->intrinsic];
            instr->pass_flags = !(info->flags & NIR_INTRINSIC_CAN_ELIMINATE);
         } else if (instr->type == nir_instr_type_alu) {
            instr->pass_flags = 0;
            nir_instr_as_alu(instr)->dest.write_mask = 0;
         } else {
            instr->pass_flags = 0;
         }
      }
   }

   /* iterate backwards: set pass_flags for used ssa-defs and
    * the write_mask for used components
    */
   nir_block *block = nir_impl_last_block(impl);
   while (block) {
      bool repeat_loop = false;
      nir_block *loop_preheader = NULL;

      /* check if we are at a loop header */
      if (nir_cf_node_is_first(&block->cf_node) &&
          block->cf_node.parent->type == nir_cf_node_loop)
         loop_preheader = nir_block_cf_tree_prev(block);

      /* mark IF-conditions as live */
      nir_if *nif = nir_block_get_following_if(block);
      if (nif) {
         nir_instr *parent = nif->condition.ssa->parent_instr;
         parent->pass_flags = 1;
         if (parent->type == nir_instr_type_alu)
            nir_instr_as_alu(parent)->dest.write_mask = 1;
      }

      nir_foreach_instr_reverse(instr, block) {
         if (instr->pass_flags) {
            /* repeat the loop if loop header phi sources changed */
            if (loop_preheader && instr->type == nir_instr_type_phi) {
               nir_foreach_phi_src(src, nir_instr_as_phi(instr)) {
                  repeat_loop |= src->pred != loop_preheader &&
                                 src->src.ssa->parent_instr->pass_flags == 0;
               }
            }
            nir_foreach_src(instr, alu_writemask_cb, NULL);
         }
      }

      if (repeat_loop)
         block = nir_loop_last_block(nir_cf_node_as_loop(block->cf_node.parent));
      else
         block = nir_block_cf_tree_prev(block);
   }
}

static bool
shrink_dest_to_read_mask(nir_ssa_def *def)
{
   /* early out if there's nothing to do. */
   if (def->num_components == 1)
      return false;

   /* don't remove any channels if used by an intrinsic */
   nir_foreach_use(use_src, def) {
      if (use_src->parent_instr->type == nir_instr_type_intrinsic)
         return false;
   }

   unsigned mask = nir_ssa_def_components_read(def);
   int last_bit = util_last_bit(mask);

   /* If nothing was read, leave it up to DCE. */
   if (!mask)
      return false;

   if (def->num_components > last_bit) {
      def->num_components = last_bit;
      return true;
   }

   return false;
}

static bool
opt_shrink_vector(nir_builder *b, nir_alu_instr *instr)
{
   nir_ssa_def *def = &instr->dest.dest.ssa;

   /* don't remove any channels if used by non-ALU */
   nir_foreach_use(use_src, def) {
      if (use_src->parent_instr->type != nir_instr_type_alu)
         return false;
   }

   unsigned mask = instr->dest.write_mask;
   uint8_t reswizzle[NIR_MAX_VEC_COMPONENTS] = { 0 };
   nir_ssa_def *srcs[NIR_MAX_VEC_COMPONENTS] = { 0 };
   unsigned num_components = 0;
   for (unsigned i = 0; i < def->num_components; i++) {
      if (!((mask >> i) & 0x1))
         continue;

      /* Try reuse a component with the same value */
      unsigned j;
      for (j = 0; j < num_components; j++) {
         if (nir_alu_srcs_equal(instr, instr, i, j)) {
            reswizzle[i] = j;
            break;
         }
      }

      /* Otherwise, just append the value */
      if (j == num_components) {
         srcs[num_components] = nir_ssa_for_alu_src(b, instr, i);
         reswizzle[i] = num_components++;
      }
   }

   if (num_components == def->num_components)
      return false;

   nir_ssa_def *new_vec = nir_vec(b, srcs, num_components);

   /* update uses */
   nir_ssa_def_rewrite_uses(def, new_vec);
   reswizzle_alu_uses(new_vec, reswizzle);

   return true;
}

static bool
opt_shrink_vectors_alu(nir_builder *b, nir_alu_instr *instr)
{
   nir_ssa_def *def = &instr->dest.dest.ssa;
   unsigned mask = instr->dest.write_mask;

   if (mask == 0) {
      /* leave it to DCE and restore the write_mask */
      instr->dest.write_mask = BITFIELD_MASK(def->num_components);
      return false;
   }

   /* Nothing to shrink */
   if (def->num_components == 1)
      return false;

   switch (instr->op) {
      /* don't use nir_op_is_vec() as not all vector sizes are supported. */
      case nir_op_vec4:
      case nir_op_vec3:
      case nir_op_vec2:
         return opt_shrink_vector(b, instr);
      default:
         if (nir_op_infos[instr->op].output_size != 0)
            return false;
         break;
   }

   unsigned num_components = util_bitcount(mask);
   if (num_components == def->num_components)
      return false;

   uint8_t reswizzle[NIR_MAX_VEC_COMPONENTS] = { 0 };
   unsigned index = 0;
   for (unsigned i = 0; i < util_last_bit(mask); i++) {
      if (!((mask >> i) & 0x1))
         continue;

      for (int k = 0; k < nir_op_infos[instr->op].num_inputs; k++) {
         instr->src[k].swizzle[index] = instr->src[k].swizzle[i];
         reswizzle[i] = index;
      }
      index++;
   }

   def->num_components = num_components;
   instr->dest.write_mask = BITFIELD_MASK(num_components);
   reswizzle_alu_uses(def, reswizzle);

   return true;
}

static bool
opt_shrink_vectors_image_store(nir_builder *b, nir_intrinsic_instr *instr)
{
   enum pipe_format format;
   if (instr->intrinsic == nir_intrinsic_image_deref_store) {
      nir_deref_instr *deref = nir_src_as_deref(instr->src[0]);
      format = nir_deref_instr_get_variable(deref)->data.image.format;
   } else {
      format = nir_intrinsic_format(instr);
   }
   if (format == PIPE_FORMAT_NONE)
      return false;

   unsigned components = util_format_get_nr_components(format);
   if (components >= instr->num_components)
      return false;

   nir_ssa_def *data = nir_channels(b, instr->src[3].ssa, BITSET_MASK(components));
   nir_instr_rewrite_src(&instr->instr, &instr->src[3], nir_src_for_ssa(data));
   instr->num_components = components;

   return true;
}

static bool
opt_shrink_vectors_intrinsic(nir_builder *b, nir_intrinsic_instr *instr, bool shrink_image_store)
{
   switch (instr->intrinsic) {
   case nir_intrinsic_load_uniform:
   case nir_intrinsic_load_ubo:
   case nir_intrinsic_load_input:
   case nir_intrinsic_load_input_vertex:
   case nir_intrinsic_load_per_vertex_input:
   case nir_intrinsic_load_interpolated_input:
   case nir_intrinsic_load_ssbo:
   case nir_intrinsic_load_push_constant:
   case nir_intrinsic_load_constant:
   case nir_intrinsic_load_shared:
   case nir_intrinsic_load_global:
   case nir_intrinsic_load_global_constant:
   case nir_intrinsic_load_kernel_input:
   case nir_intrinsic_load_scratch:
   case nir_intrinsic_store_output:
   case nir_intrinsic_store_per_vertex_output:
   case nir_intrinsic_store_ssbo:
   case nir_intrinsic_store_shared:
   case nir_intrinsic_store_global:
   case nir_intrinsic_store_scratch:
      break;
   case nir_intrinsic_bindless_image_store:
   case nir_intrinsic_image_deref_store:
   case nir_intrinsic_image_store:
      return shrink_image_store && opt_shrink_vectors_image_store(b, instr);
   default:
      return false;
   }

   /* Must be a vectorized intrinsic that we can resize. */
   assert(instr->num_components != 0);

   if (nir_intrinsic_infos[instr->intrinsic].has_dest) {
      /* loads: Trim the dest to the used channels */

      if (shrink_dest_to_read_mask(&instr->dest.ssa)) {
         instr->num_components = instr->dest.ssa.num_components;
         return true;
      }
   } else {
      /* Stores: trim the num_components stored according to the write
       * mask.
       */
      unsigned write_mask = nir_intrinsic_write_mask(instr);
      unsigned last_bit = util_last_bit(write_mask);
      if (last_bit < instr->num_components && instr->src[0].is_ssa) {
         nir_ssa_def *def = nir_channels(b, instr->src[0].ssa,
                                         BITSET_MASK(last_bit));
         nir_instr_rewrite_src(&instr->instr,
                               &instr->src[0],
                               nir_src_for_ssa(def));
         instr->num_components = last_bit;

         return true;
      }
   }

   return false;
}

static bool
opt_shrink_vectors_load_const(nir_load_const_instr *instr)
{
   nir_ssa_def *def = &instr->def;

   /* early out if there's nothing to do. */
   if (def->num_components == 1)
      return false;

   /* don't remove any channels if used by non-ALU */
   nir_foreach_use(use_src, def) {
      if (use_src->parent_instr->type != nir_instr_type_alu)
         return false;
   }

   unsigned mask = nir_ssa_def_components_read(def);

   /* If nothing was read, leave it up to DCE. */
   if (!mask)
      return false;

   uint8_t reswizzle[NIR_MAX_VEC_COMPONENTS] = { 0 };
   unsigned num_components = 0;
   for (unsigned i = 0; i < def->num_components; i++) {
      if (!((mask >> i) & 0x1))
         continue;

      /* Try reuse a component with the same constant */
      unsigned j;
      for (j = 0; j < num_components; j++) {
         if (instr->value[i].u64 == instr->value[j].u64) {
            reswizzle[i] = j;
            break;
         }
      }

      /* Otherwise, just append the value */
      if (j == num_components) {
         instr->value[num_components] = instr->value[i];
         reswizzle[i] = num_components++;
      }
   }

   if (num_components == def->num_components)
      return false;

   def->num_components = num_components;
   reswizzle_alu_uses(def, reswizzle);

   return true;
}

static bool
opt_shrink_vectors_ssa_undef(nir_ssa_undef_instr *instr)
{
   return shrink_dest_to_read_mask(&instr->def);
}

static bool
opt_shrink_vectors_instr(nir_builder *b, nir_instr *instr, bool shrink_image_store)
{
   b->cursor = nir_before_instr(instr);

   switch (instr->type) {
   case nir_instr_type_alu:
      return opt_shrink_vectors_alu(b, nir_instr_as_alu(instr));

   case nir_instr_type_intrinsic:
      return opt_shrink_vectors_intrinsic(b, nir_instr_as_intrinsic(instr), shrink_image_store);

   case nir_instr_type_load_const:
      return opt_shrink_vectors_load_const(nir_instr_as_load_const(instr));

   case nir_instr_type_ssa_undef:
      return opt_shrink_vectors_ssa_undef(nir_instr_as_ssa_undef(instr));

   default:
      return false;
   }

   return true;
}

bool
nir_opt_shrink_vectors(nir_shader *shader, bool shrink_image_store)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (!function->impl)
         continue;

      init_alu_writemask(function->impl);

      nir_builder b;
      nir_builder_init(&b, function->impl);

      nir_foreach_block_reverse(block, function->impl) {
         nir_foreach_instr_reverse(instr, block) {
            progress |= opt_shrink_vectors_instr(&b, instr, shrink_image_store);
         }
      }

      if (progress) {
         nir_metadata_preserve(function->impl,
                               nir_metadata_block_index |
                               nir_metadata_dominance);
      } else {
         nir_metadata_preserve(function->impl, nir_metadata_all);
      }
   }

   return progress;
}
