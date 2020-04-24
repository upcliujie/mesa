/*
 * Copyright © 2020 Intel Corporation
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

#include "brw_nir_ubo_gather.h"
#include "brw_compiler.h"
#include "nir_builder.h"
#include "util/u_dynarray.h"

static void
store_global(nir_builder *b, nir_ssa_def *addr, nir_ssa_def *value)
{
   nir_intrinsic_instr *store =
      nir_intrinsic_instr_create(b->shader, nir_intrinsic_store_global);
   store->num_components = value->num_components;
   store->src[0] = nir_src_for_ssa(value);
   store->src[1] = nir_src_for_ssa(addr);
   nir_intrinsic_set_align(store, 4, 0);
   nir_intrinsic_set_write_mask(store, (1 << value->num_components) - 1);
   nir_builder_instr_insert(b, &store->instr);
}

/** Emits NIR code to do a gather op */
static void
build_gather_op(nir_builder *b,
                nir_ssa_def *dst_addr_in,
                nir_ssa_def *src_addr_in,
                nir_ssa_def *dwords_in)
{
   nir_ssa_def *src_is_null = nir_ieq(b, src_addr_in, nir_imm_int64(b, 0));

   /* Temporary loop variables. */
   nir_variable *src_var =
      nir_local_variable_create(b->impl, glsl_uint64_t_type(), "src");
   nir_variable *dst_var =
      nir_local_variable_create(b->impl, glsl_uint64_t_type(), "dst");
   nir_variable *dwords_var =
      nir_local_variable_create(b->impl, glsl_uint_type(), "dwords");
   nir_variable *count_var =
      nir_local_variable_create(b->impl, glsl_uint_type(), "count");

   /* Initialize loop variables */
   nir_store_var(b, src_var, src_addr_in, 1);
   nir_store_var(b, dst_var, dst_addr_in, 1);
   nir_store_var(b, dwords_var, dwords_in, 1);
   nir_store_var(b, count_var, nir_bit_count(b, dwords_in), 1);

   nir_push_loop(b);
   {
      nir_ssa_def *zero = nir_imm_int(b, 0);
      /* We effectively have two loops here.  In the case where our source is
       * not null, we iterate over the set bits in dwords.  In the case where
       * our source is null, we iterate over the number of bits.  However, we
       * keep the two avoid subgroup divergence.  If we did the loop inside
       * the if, we would end up executing the loop twice if the src_is_null
       * is a divergent value.
       */
      nir_ssa_def *dwords_bits, *count;
      nir_ssa_def *data[4];
      nir_push_if(b, nir_inot(b, src_is_null));
      {
         nir_ssa_def *dwords = nir_load_var(b, dwords_var);

         nir_push_if(b, nir_ieq(b, dwords, nir_imm_int(b, 0)));
         nir_jump(b, nir_jump_break);
         nir_pop_if(b, NULL);

         dwords_bits = nir_bit_count(b, dwords);

         nir_ssa_def *src = nir_load_var(b, src_var);
         /* We loop on the CPU to ensure this gets unrolled */
         for (unsigned i = 0; i < 4; i++) {
            nir_ssa_def *next = nir_find_lsb(b, dwords);
            src = nir_iadd(b, src, nir_u2u64(b, nir_imul_imm(b, next, 4)));

            nir_intrinsic_op load_op =
               (i == 0) ? nir_intrinsic_load_global :
                          nir_intrinsic_load_global_predicated;
            nir_intrinsic_instr *load =
               nir_intrinsic_instr_create(b->shader, load_op);
            load->num_components = 1;
            load->src[0] = nir_src_for_ssa(src);
            nir_intrinsic_set_align(load, 4, 0);
            if (load_op == nir_intrinsic_load_global_predicated) {
               load->src[1] = nir_src_for_ssa(nir_ine(b, dwords, nir_imm_int(b, 0)));
               load->src[2] = nir_src_for_ssa(nir_ssa_undef(b, 1, 32));
            }
            nir_ssa_dest_init(&load->instr, &load->dest, 1, 32, NULL);
            nir_builder_instr_insert(b, &load->instr);
            data[i] = &load->dest.ssa;

            dwords = nir_iand(b, nir_ushr(b, dwords, next), nir_imm_int(b, ~1));
         }
         nir_store_var(b, src_var, src, 1);
         nir_store_var(b, dwords_var, dwords, 1);
      }
      nir_push_else(b, NULL);
      {
         count = nir_load_var(b, count_var);

         nir_push_if(b, nir_ige(b, nir_imm_int(b, 0), count));
         nir_jump(b, nir_jump_break);
         nir_pop_if(b, NULL);

         nir_store_var(b, count_var, nir_iadd_imm(b, count, -(int64_t)4), 1);
      }
      nir_pop_if(b, NULL);
      nir_ssa_def *num_dw = nir_if_phi(b, dwords_bits, count);
      for (unsigned i = 0; i < 4; i++)
         data[i] = nir_if_phi(b, data[i], zero);

      nir_ssa_def *dst = nir_load_var(b, dst_var);

      /* Store 1-4 components based on how many bits were in the dwords mask */
      nir_push_if(b, nir_uge(b, num_dw, nir_imm_int(b, 4)));
      store_global(b, dst, nir_vec(b, data, 4));
      nir_push_else(b, NULL);
      nir_push_if(b, nir_ieq(b, num_dw, nir_imm_int(b, 3)));
      store_global(b, dst, nir_vec(b, data, 3));
      nir_push_else(b, NULL);
      nir_push_if(b, nir_ieq(b, num_dw, nir_imm_int(b, 2)));
      store_global(b, dst, nir_vec(b, data, 2));
      nir_push_else(b, NULL);
      store_global(b, dst, data[0]);
      nir_pop_if(b, NULL);
      nir_pop_if(b, NULL);
      nir_pop_if(b, NULL);

      /* If we wrote less than 4 components, we're done so we can add a
       * constant 16B offset to dst here.
       */
      dst = nir_iadd_imm(b, dst, 16);
      nir_store_var(b, dst_var, dst, 1);
   }
   nir_pop_loop(b, NULL);
}

/** Constructs a vertex shader which does a UBO gather
 *
 * The resulting shader consumes a stream of uvec4s as vertex input data.
 * Each gather work item copies up to 32 dwords of data (or zeros) from the
 * source UBOs to the gather buffer.  The work items can be constructed using
 * the brw_nir_pack_gather_vs_entry helper.  In GLSL, this shader would look
 * something like this:
 */
nir_shader *
brw_nir_create_gather_vs(const struct brw_compiler *compiler, void *mem_ctx)
{
   const nir_shader_compiler_options *nir_options =
      compiler->glsl_compiler_options[MESA_SHADER_VERTEX].NirOptions;

   nir_builder build;
   nir_builder_init_simple_shader(&build, mem_ctx, MESA_SHADER_VERTEX,
                                  nir_options);
   nir_builder *b = &build;

   b->shader->info.name = ralloc_strdup(b->shader, "ANV Constant Gather");

   /* Fetch the addresses and mask from the input */
   nir_variable *input_var =
      nir_variable_create(b->shader, nir_var_shader_in,
                          glsl_uvec4_type(), "v_gather");
   input_var->data.location = VERT_ATTRIB_GENERIC0;
   nir_ssa_def *input = nir_load_var(b, input_var);

   nir_ssa_def *dst_low = nir_channel(b, input, 0);
   nir_ssa_def *dst_hi = nir_channel(b, input, 1);
   dst_hi = nir_ishr(b, nir_ishl(b, dst_hi, nir_imm_int(b, 16)),
                        nir_imm_int(b, 16));
   nir_ssa_def *dst_addr = nir_pack_64_2x32_split(b, dst_low, dst_hi);

   nir_ssa_def *src_low = nir_channel(b, input, 2);
   nir_ssa_def *src_hi = nir_channel(b, input, 3);
   src_hi = nir_ishr(b, nir_ishl(b, src_hi, nir_imm_int(b, 16)),
                        nir_imm_int(b, 16));
   nir_ssa_def *src_addr = nir_pack_64_2x32_split(b, src_low, src_hi);

   nir_ssa_def *dwords =
      nir_ior(b, nir_ushr(b, nir_channel(b, input, 1), nir_imm_int(b, 16)),
                 nir_iand(b, nir_channel(b, input, 3),
                             nir_imm_int(b, 0xffff0000)));

   build_gather_op(b, dst_addr, src_addr, dwords);

   return b->shader;
}

struct ubo_load {
   uint8_t block;
   uint16_t uses;
   uint32_t offset_dw;
};

static int
cmp_ubo_load_by_block_offset(const void *_a, const void *_b)
{
   const struct ubo_load *a = _a, *b = _b;

   if (a->block != b->block)
      return (int)a->block - (int)b->block;

   assert(a->offset_dw < INT_MAX);
   assert(b->offset_dw < INT_MAX);
   return (int)a->offset_dw - (int)b->offset_dw;
}

static int
cmp_ubo_load_by_uses_inv(const void *_a, const void *_b)
{
   const struct ubo_load *a = _a, *b = _b;

   /* Inverse comparison so it sorts in descending order */
   return (int)b->uses - (int)a->uses;
}

/** Analyze a shader and try to "gather" the UBO loads
 *
 * This pass analyzes a shader and looks at every constant-offset UBO load and
 * tries to pack as many of them as possible into a single contiguous range.
 * Returned by this pass is a list of brw_ubo_gather structs each of which
 * specifies one or more dwords worth of data which needs to be packed into
 * the gather buffer.
 *
 * This pass is only an analysis pass and does not touch the NIR shader.  To
 * lower UBO loads from gathered memory to nir_intrinsic_load_push_constant,
 * call brw_nir_lower_gathered_ubo_loads().
 */
struct brw_ubo_gather *
brw_nir_gather_ubo_loads(nir_shader *nir, unsigned max_gather_size,
                         unsigned *gather_count, void *mem_ctx)
{
   struct util_dynarray load_arr;
   util_dynarray_init(&load_arr, mem_ctx);

   bool has_64bit_load = false;
   nir_foreach_function(function, nir) {
      if (!function->impl)
         continue;

      nir_foreach_block(block, function->impl) {
         unsigned loop_factor = 1;
         for (nir_cf_node *node = &block->cf_node; node; node = node->parent) {
            if (node->type == nir_cf_node_loop)
               loop_factor *= 10;
         }

         nir_foreach_instr(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *load = nir_instr_as_intrinsic(instr);
            if (load->intrinsic != nir_intrinsic_load_ubo)
               continue;

            if (!nir_src_is_const(load->src[0]) ||
                !nir_src_is_const(load->src[1]))
               continue;

            assert(nir_src_as_uint(load->src[0]) < BRW_MAX_BINDING_TABLE_SIZE);
            assert(nir_src_as_uint(load->src[1]) <= UINT32_MAX);
            uint8_t block = nir_src_as_uint(load->src[0]);
            uint32_t offset = nir_src_as_uint(load->src[1]);

            assert(load->dest.ssa.bit_size >= 8);
            uint32_t offset_end =
               offset + load->num_components * load->dest.ssa.bit_size / 8;
            assert(offset_end == 0 || offset_end > offset);

            if (load->dest.ssa.bit_size > 32) {
               assert(load->dest.ssa.bit_size == 64);
               has_64bit_load = true;
            }

            unsigned first_dw = offset / 4;
            unsigned last_dw = (offset_end - 1) / 4;

            for (unsigned dw = first_dw; dw <= last_dw; dw++) {
               struct ubo_load load = {
                  .block = block,
                  .uses = loop_factor,
                  .offset_dw = dw,
               };
               util_dynarray_append(&load_arr, struct ubo_load, load);
            }
         }
      }
   }

   struct ubo_load *loads = load_arr.data;
   unsigned nr_loads = util_dynarray_num_elements(&load_arr, struct ubo_load);
   if (nr_loads == 0) {
      *gather_count = 0;
      return NULL;
   }

   qsort(loads, nr_loads, sizeof(*loads), cmp_ubo_load_by_block_offset);

   /* De-duplicate load entries. They should have been sorted by BTI first
    * then offset.  Any duplicate accesses should now be adjacent in the list.
    */
   unsigned idx = 0;
   for (unsigned i = 1; i < nr_loads; i++) {
      if (loads[i].block == loads[idx].block &&
          loads[i].offset_dw == loads[idx].offset_dw) {
         loads[idx].uses += loads[i].uses;
      } else {
         loads[++idx] = loads[i];
      }
   }
   nr_loads = idx + 1;

   if (nr_loads * 4 > max_gather_size) {
      qsort(loads, nr_loads, sizeof(*loads), cmp_ubo_load_by_uses_inv);
      nr_loads = max_gather_size / 4;
      qsort(loads, nr_loads, sizeof(*loads), cmp_ubo_load_by_block_offset);
   }

   struct util_dynarray gather_arr;
   util_dynarray_init(&gather_arr, mem_ctx);

   struct brw_ubo_gather *gather = NULL;
   uint8_t block = -1;
   uint32_t max_dw = 0;
   for (unsigned i = 0; i < nr_loads; i++) {
      if (gather == NULL || loads[i].block != block ||
          loads[i].offset_dw > max_dw) {
         gather = util_dynarray_grow(&gather_arr, struct brw_ubo_gather, 1);
         *gather = (struct brw_ubo_gather) {
            /* We use BTI here.  We'll fix it later */
            .block = loads[i].block,
            .start = loads[i].offset_dw * 4,
         };
         /* If we have a 64-bit load anywhere, make sure all of our gathers
          * are 64-bit aligned
          */
         if (has_64bit_load && (gather->start % 8)) {
            assert(gather->start % 8 == 4);
            gather->start -= 4;
         }
         block = loads[i].block;
         max_dw = loads[i].offset_dw + 31;
      }

      assert(loads[i].offset_dw * 4 >= gather->start);
      assert(loads[i].offset_dw <= max_dw);
      uint32_t rel_dw = loads[i].offset_dw - gather->start / 4;
      assert(rel_dw < 32);

      if (has_64bit_load) {
         /* Round down to an even number and set two bits */
         rel_dw &= ~1u;
         assert(rel_dw == 30 || gather->dwords < BITFIELD_BIT(rel_dw + 2));
         gather->dwords |= BITFIELD_RANGE(rel_dw, 2);
         assert(util_bitcount(gather->dwords) % 2 == 0);
      } else {
         assert(gather->dwords < BITFIELD_BIT(rel_dw));
         gather->dwords |= BITFIELD_BIT(rel_dw);
      }
   }

   *gather_count =
      util_dynarray_num_elements(&gather_arr, struct brw_ubo_gather);
   return gather_arr.data;
}

/** Lowered gathered UBO loads to nir_intrinsic_load_push_constant */
void
brw_nir_lower_gathered_ubo_loads(nir_shader *nir,
                                 unsigned gather_start,
                                 unsigned gather_count,
                                 struct brw_ubo_gather *gathers)
{
   if (gather_count == 0)
      return;

   struct hash_table_u64 *remap_table = _mesa_hash_table_u64_create(NULL);
   uint32_t packed_dw = 0;
   for (unsigned i = 0; i < gather_count; i++) {
      unsigned dwords = gathers[i].dwords;
      while (dwords) {
         int dw = u_bit_scan(&dwords);
         uint64_t key = (uint64_t)gathers[i].block << 32 |
                        (gathers[i].start / 4 + dw);
         assert(packed_dw < 0x1000);
         uintptr_t value = 0x1000 | packed_dw++;
         _mesa_hash_table_u64_insert(remap_table, key, (void *)value);
      }
   }
   const unsigned gather_size = packed_dw * 4;

   nir_foreach_function(function, nir) {
      if (!function->impl)
         continue;

      bool progress = false;

      nir_builder b;
      nir_builder_init(&b, function->impl);

      nir_foreach_block(block, function->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *ubo_load = nir_instr_as_intrinsic(instr);
            if (ubo_load->intrinsic != nir_intrinsic_load_ubo)
               continue;

            if (!nir_src_is_const(ubo_load->src[0]) ||
                !nir_src_is_const(ubo_load->src[1]))
               continue;

            uint8_t block = nir_src_as_uint(ubo_load->src[0]);
            uint32_t offset = nir_src_as_uint(ubo_load->src[1]);
            uint32_t offset_end =
               offset + ubo_load->num_components * ubo_load->dest.ssa.bit_size / 8;

            unsigned first_dw = offset / 4;
            unsigned last_dw = (offset_end - 1) / 4;

            unsigned remap_dw = 0;
            bool found = true;
            for (unsigned dw = first_dw; dw <= last_dw; dw++) {
               uint64_t key = (uint64_t)block << 32 | dw;
               void *entry = _mesa_hash_table_u64_search(remap_table, key);
               if (entry == NULL) {
                  found = false;
                  break;
               }
               assert((uintptr_t)entry & 0x1000);
               uint32_t entry_dw = (uintptr_t)entry & 0xfff;
               if (dw == first_dw) {
                  remap_dw = entry_dw;
               } else {
                  assert(entry_dw == remap_dw + dw - first_dw);
               }
            }
            if (!found)
               continue;

            /* Compute the re-mapped offset and add in whatever is left-over
             * in offset in case we're looking at an 8 or 16-bit value that's
             * not at the start of a dword.
             */
            uint32_t remap_offset = remap_dw * 4 + (offset & 0x3);

            b.cursor = nir_before_instr(&ubo_load->instr);

            nir_intrinsic_instr *push_load =
               nir_intrinsic_instr_create(b.shader,
                                          nir_intrinsic_load_uniform);
            push_load->src[0] = nir_src_for_ssa(nir_imm_int(&b, remap_offset));
            nir_intrinsic_set_base(push_load, gather_start);
            nir_intrinsic_set_range(push_load, gather_size);
            nir_intrinsic_set_type(push_load,
               nir_type_uint | ubo_load->dest.ssa.bit_size);

            push_load->num_components = ubo_load->num_components;
            nir_ssa_dest_init(&push_load->instr, &push_load->dest,
                              ubo_load->dest.ssa.num_components,
                              ubo_load->dest.ssa.bit_size, NULL);

            nir_builder_instr_insert(&b, &push_load->instr);

            nir_ssa_def_rewrite_uses(&ubo_load->dest.ssa,
                                     nir_src_for_ssa(&push_load->dest.ssa));
            nir_instr_remove(&ubo_load->instr);
            progress = true;
         }
      }

      if (progress) {
         nir_metadata_preserve(function->impl, nir_metadata_block_index |
                                               nir_metadata_dominance);
      } else {
#ifndef NDEBUG
         function->impl->valid_metadata &= ~nir_metadata_not_properly_reset;
#endif
      }
   }

   _mesa_hash_table_u64_destroy(remap_table, NULL);
}
