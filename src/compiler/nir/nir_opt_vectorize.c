/*
 * Copyright © 2015 Connor Abbott
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
 *
 */

#include "nir.h"
#include "nir_vla.h"
#include "nir_builder.h"
#include "util/u_dynarray.h"

#define HASH(hash, data) XXH32(&data, sizeof(data), hash)

static uint32_t
hash_alu_src(uint32_t hash, const nir_alu_src *src,
             uint32_t num_components, uint32_t max_vec, bool aggressive)
{
   assert(!src->abs && !src->negate);
   assert(src->src.is_ssa);

   /* don't hash constant sources. these can always be combined. */
   if (nir_src_is_const(src->src))
      return hash;

   /* aggressive vectorization allows to combine sources. */
   if (aggressive && src->src.ssa->num_components == num_components)
      return hash;

   /* hash swizzle higher than max_vec */
   uint32_t swizzle = (src->swizzle[0] & ~(max_vec - 1));
   hash = HASH(hash, swizzle);

   void *hash_data = src->src.ssa;
   return HASH(hash, hash_data);
}

static uint32_t
hash_instr(const void *data)
{
   const nir_instr *instr = (nir_instr *) data;
   assert(instr->type == nir_instr_type_alu);
   uint32_t max_vec = instr->pass_flags & 0xF;
   bool aggressive = instr->pass_flags >> 4;

   nir_alu_instr *alu = nir_instr_as_alu(instr);
   uint32_t hash = HASH(0, alu->op);

   hash = HASH(hash, alu->dest.dest.ssa.bit_size);

   for (unsigned i = 0; i < nir_op_infos[alu->op].num_inputs; i++)
      hash = hash_alu_src(hash, &alu->src[i],
                          alu->dest.dest.ssa.num_components,
                          max_vec, aggressive);

   return hash;
}

static bool
alu_srcs_equal(const nir_alu_src *src1, const nir_alu_src *src2,
               uint32_t src1_components, uint32_t src2_components,
               uint32_t max_vec, bool aggressive)
{
   assert(!src1->abs);
   assert(!src1->negate);
   assert(!src2->abs);
   assert(!src2->negate);
   assert(src1->src.is_ssa);
   assert(src2->src.is_ssa);

   /* aggressive vectorization allows to combine the sources
    * if the number of components matches. */
   if ((nir_src_is_const(src1->src) ||
        (aggressive && src1->src.ssa->num_components == src1_components)) &&
       (nir_src_is_const(src2->src) ||
        (aggressive && src2->src.ssa->num_components == src2_components)))
      return true;

   uint32_t mask = ~(max_vec - 1);
   if ((src1->swizzle[0] & mask) != (src2->swizzle[0] & mask))
      return false;

   return src1->src.ssa == src2->src.ssa;
}

static bool
instrs_equal(const void *data1, const void *data2)
{
   const nir_instr *instr1 = (nir_instr *) data1;
   const nir_instr *instr2 = (nir_instr *) data2;
   switch (instr1->type) {
   case nir_instr_type_alu: {
      nir_alu_instr *alu1 = nir_instr_as_alu(instr1);
      nir_alu_instr *alu2 = nir_instr_as_alu(instr2);

      if (alu1->op != alu2->op)
         return false;

      if (alu1->dest.dest.ssa.bit_size != alu2->dest.dest.ssa.bit_size)
         return false;

      uint32_t max_vec = instr1->pass_flags & 0xF;
      bool aggressive = instr1->pass_flags >> 4;
      for (unsigned i = 0; i < nir_op_infos[alu1->op].num_inputs; i++) {
         if (!alu_srcs_equal(&alu1->src[i], &alu2->src[i],
                             alu1->dest.dest.ssa.num_components,
                             alu2->dest.dest.ssa.num_components,
                             max_vec, aggressive))
            return false;
      }

      return true;
   }

   default:
      unreachable("bad instruction type");
   }
}

static struct set *
vec_instr_set_create(void)
{
   return _mesa_set_create(NULL, hash_instr, instrs_equal);
}

static void
vec_instr_set_destroy(struct set *instr_set)
{
   _mesa_set_destroy(instr_set, NULL);
}

static bool
instr_can_rewrite(nir_instr *instr, bool vectorize_16bit)
{
   switch (instr->type) {
   case nir_instr_type_alu: {
      nir_alu_instr *alu = nir_instr_as_alu(instr);

      /* Don't try and vectorize mov's. Either they'll be handled by copy
       * prop, or they're actually necessary and trying to vectorize them
       * would result in fighting with copy prop.
       */
      if (alu->op == nir_op_mov)
         return false;

      /* no need to hash instructions which are already vectorized */
      if (alu->dest.dest.ssa.num_components >= 4)
         return false;

      if (vectorize_16bit &&
          (alu->dest.dest.ssa.num_components >= 2 ||
           alu->dest.dest.ssa.bit_size != 16))
         return false;

      if (nir_op_infos[alu->op].output_size != 0)
         return false;

      for (unsigned i = 0; i < nir_op_infos[alu->op].num_inputs; i++) {
         if (nir_op_infos[alu->op].input_sizes[i] != 0)
            return false;

         /* don't hash instructions which are already swizzled
          * outside of max_components: these should better be scalarized */
         uint32_t mask = vectorize_16bit ? ~1 : ~3;
         for (unsigned j = 0; j < alu->dest.dest.ssa.num_components; j++) {
            if ((alu->src[i].swizzle[0] & mask) != (alu->src[i].swizzle[i] & mask))
               return false;
         }
      }

      return true;
   }

   /* TODO support phi nodes */
   default:
      break;
   }

   return false;
}

static void
rewrite_sources(nir_builder *b, nir_ssa_def *ssa1, nir_ssa_def *ssa2,
                nir_ssa_def *new_ssa, struct set *instr_set)
{
   unsigned alu1_components = ssa1->num_components;
   unsigned alu2_components = ssa2->num_components;

   unsigned swiz[NIR_MAX_VEC_COMPONENTS];
   for (unsigned i = 0; i < NIR_MAX_VEC_COMPONENTS; i++)
      swiz[i] = i;
   nir_ssa_def *new_alu1 = nir_swizzle(b, new_ssa, swiz,
                                       alu1_components);

   for (unsigned i = 0; i < alu2_components; i++)
      swiz[i] += alu1_components;
   nir_ssa_def *new_alu2 = nir_swizzle(b, new_ssa, swiz,
                                       alu2_components);

   nir_foreach_use_safe(src, ssa1) {
      if (src->parent_instr->type == nir_instr_type_alu) {
         if (src->parent_instr == new_ssa->parent_instr)
            continue;

         /* find user in hashset */
         struct set_entry *entry = _mesa_set_search(instr_set, src->parent_instr);

         /* For ALU instructions, rewrite the source directly to avoid a
          * round-trip through copy propagation.
          */
         nir_instr_rewrite_src(src->parent_instr, src,
                               nir_src_for_ssa(new_ssa));

         /* Rehash users: note that all instructions in the stack change.
          * As hashing only checks the first instruction, this means
          * that the hash might or might not change with the current user */
         if (entry) {
            _mesa_set_remove(instr_set, entry);
            _mesa_set_add(instr_set, src->parent_instr);
         }
      } else {
         nir_instr_rewrite_src(src->parent_instr, src,
                               nir_src_for_ssa(new_alu1));
      }
   }

   nir_foreach_if_use_safe(src, ssa1) {
      nir_if_rewrite_condition(src->parent_if, nir_src_for_ssa(new_alu1));
   }

   nir_foreach_use_safe(src, ssa2) {
      if (src->parent_instr->type == nir_instr_type_alu) {
         if (src->parent_instr == new_ssa->parent_instr)
            continue;

         /* For ALU instructions, rewrite the source directly to avoid a
          * round-trip through copy propagation.
          */

         nir_alu_instr *use = nir_instr_as_alu(src->parent_instr);

         unsigned src_index = 5;
         for (unsigned i = 0; i < nir_op_infos[use->op].num_inputs; i++) {
            if (&use->src[i].src == src) {
               src_index = i;
               break;
            }
         }
         assert(src_index != 5);

         nir_instr_rewrite_src(src->parent_instr, src,
                               nir_src_for_ssa(new_ssa));

         for (unsigned i = 0;
              i < nir_ssa_alu_instr_src_components(use, src_index); i++) {
            use->src[src_index].swizzle[i] += alu1_components;
         }
      } else {
         nir_instr_rewrite_src(src->parent_instr, src,
                               nir_src_for_ssa(new_alu2));
      }
   }

   nir_foreach_if_use_safe(src, ssa2) {
      nir_if_rewrite_condition(src->parent_if, nir_src_for_ssa(new_alu2));
   }
}

enum src_dependency {
   dependent = 0,
   independent = 1,
   hoist_src1 = 2,
   hoist_src2 = 3,
   constants = 4,
   unknown = 5,
};

static bool
alu_is_independent(nir_alu_instr *alu, struct util_dynarray *deps)
{
   for (unsigned i = 0; i < nir_op_infos[alu->op].num_inputs; i++) {
      util_dynarray_foreach(deps, nir_ssa_def*, ssa) {
         if (*ssa == alu->src[i].src.ssa) {
            util_dynarray_append(deps, nir_ssa_def*, &alu->dest.dest.ssa);
            return false;
         }
      }
   }
   return true;
}

static enum src_dependency
instr_is_independent(nir_instr *begin, nir_instr *end, nir_instr *to_test)
{
   struct util_dynarray deps;
   util_dynarray_init(&deps, NULL);
   util_dynarray_append(&deps, nir_ssa_def*,
                        &nir_instr_as_alu(begin)->dest.dest.ssa);
   nir_instr *current = nir_instr_next(begin);
   enum src_dependency dep = independent;

   while (current && current != end) {
      if (current->type == nir_instr_type_alu) {
         bool is_independent = alu_is_independent(nir_instr_as_alu(current),
                                                  &deps);

         /* to_test is considered independent if it does not depend on begin
          * and nothing but ALU and constants is between begin and to_test */
         if (current == to_test) {
            util_dynarray_fini(&deps);
            return is_independent ? dep : dependent;
         }
      } else if (current->type != nir_instr_type_load_const) {
         dep = dependent;
         if (!end || end->block != begin->block)
            break;
      }
      current = nir_instr_next(current);
   }

   util_dynarray_fini(&deps);
   return unknown;
}

/**
 * Checks if src2 depends on src1.
 * As the sources are going to be moved together, this function
 * returns false if any other instruction type except ALU or load_const
 * is found between the sources.
 */
static enum src_dependency
check_sources_independent(nir_src *src1, nir_src *src2)
{
   /* if the second source is constant, it cannot depend on the first one */
   if (nir_src_as_const_value(*src2))
      return nir_src_as_const_value(*src1) ? constants : independent;

   /* shortcut: src2 directly depends on instr1 */
   if (src2->ssa->parent_instr == src1->parent_instr)
      return dependent;

   /* we only consider ALU and constant sources */
   if (src2->ssa->parent_instr->type != nir_instr_type_alu)
      return dependent;

   nir_instr *start;
   if (nir_src_as_const_value(*src1)) {
      /* if src1 is const, src2 has to be able to dominate instr1 */
      if (src2->ssa->parent_instr->block != src1->parent_instr->block)
         return src2->parent_instr->block == src1->parent_instr->block ?
                independent : dependent;

      start = src1->parent_instr;
   } else {
      /* we only consider ALU and constant sources */
      if (src1->ssa->parent_instr->type != nir_instr_type_alu)
         return dependent;

      /* the sources have to be in the same block or constant */
      if (src1->ssa->parent_instr->block != src2->ssa->parent_instr->block)
         return dependent;

      start = src1->ssa->parent_instr;
   }

   /* iterate forward from src1 and search for src2 */
   enum src_dependency dep;
   dep = instr_is_independent(start, src2->parent_instr,
                              src2->ssa->parent_instr);
   if (dep != unknown)
      return dep == independent ? hoist_src2 : dependent;

   /* if we still didn't find src2, it has to be before instr1... */
   if (nir_src_as_const_value(*src1))
      return independent;

   /* ...or even before src1 */
   dep = instr_is_independent(src2->ssa->parent_instr, NULL,
                              src1->ssa->parent_instr);
   return dep == independent ? hoist_src1 : dependent;
}

static void
merge_alu_srcs(nir_builder *b, struct set *instr_set, enum src_dependency dep,
               nir_alu_src *src1, unsigned alu1_components,
               nir_alu_src *src2, unsigned alu2_components)
{
   nir_ssa_def *ssa1 = src1->src.ssa;
   nir_ssa_def *ssa2 = src2->src.ssa;
   nir_const_value *c1 = nir_src_as_const_value(src1->src);
   nir_const_value *c2 = nir_src_as_const_value(src2->src);

   /* hoist one of the sources if necessary */
   if (dep == hoist_src1 || dep == hoist_src2) {
      nir_instr *to_hoist, *limit;
      if (dep == hoist_src1) {
         to_hoist = ssa1->parent_instr;
         limit = ssa2->parent_instr;
      } else {
         to_hoist = ssa2->parent_instr;
         limit = c1 ? src1->src.parent_instr :
                      ssa1->parent_instr;
      }

      struct util_dynarray deps;
      util_dynarray_init(&deps, NULL);
      util_dynarray_append(&deps, nir_ssa_def*,
                           &nir_instr_as_alu(limit)->dest.dest.ssa);
      b->cursor = nir_before_instr(limit);
      nir_instr *next = nir_instr_next(limit);
      while (next != to_hoist) {
         /* move independent instructions before limit:
          * to_hoist might depend on them */
         nir_instr *current = next;
         next = nir_instr_next(next);
         if (current->type == nir_instr_type_alu &&
             !alu_is_independent(nir_instr_as_alu(current), &deps))
            continue;

         nir_instr_remove(current);
         nir_builder_instr_insert(b, current);
      }
      util_dynarray_fini(&deps);

      /* insert to_hoist after limit (or before in case limit is instr1) */
      if (c1 == NULL)
         b->cursor = nir_after_instr(limit);
      nir_instr_remove(to_hoist);
      nir_builder_instr_insert(b, to_hoist);
   }

   /* replace constants by new ones: these don't rewrite all users */
   if (c1) {
      nir_const_value value[NIR_MAX_VEC_COMPONENTS];
      for (unsigned j = 0; j < alu1_components; j++)
         value[j].u64 = c1[src1->swizzle[j]].u64;
      b->cursor = nir_before_instr(c2 ? src1->src.parent_instr :
                                        ssa2->parent_instr);
      ssa1 = nir_build_imm(b, alu1_components, ssa1->bit_size, value);

      nir_instr_rewrite_src(src1->src.parent_instr, &src1->src,
                            nir_src_for_ssa(ssa1));
      for (unsigned j = 0; j < alu1_components; j++)
         src1->swizzle[j] = j;
   }
   if (c2) {
      nir_const_value value[NIR_MAX_VEC_COMPONENTS];
      for (unsigned j = 0; j < alu2_components; j++)
         value[j].u64 = c2[src2->swizzle[j]].u64;
      b->cursor = nir_after_instr(ssa1->parent_instr);
      ssa2 = nir_build_imm(b, alu2_components, ssa2->bit_size, value);

      nir_instr_rewrite_src(src2->src.parent_instr, &src2->src,
                            nir_src_for_ssa(ssa2));
      for (unsigned j = 0; j < alu2_components; j++)
         src2->swizzle[j] = j;
   }

   /* create new merged vecN source */
   assert(ssa1->num_components == alu1_components);
   assert(ssa2->num_components == alu2_components);
   nir_ssa_def* components[NIR_MAX_VEC_COMPONENTS];
   for (unsigned j = 0; j < alu1_components; j++)
      components[j] = nir_channel(b, ssa1, j);
   for (unsigned j = 0; j < alu2_components; j++)
      components[alu1_components + j] = nir_channel(b, ssa2, j);

   nir_ssa_def *def = nir_vec(b, components, alu1_components + alu2_components);

   rewrite_sources(b, ssa1, ssa2, def, instr_set);
}

/*
 * Tries to combine two instructions whose sources are different components of
 * the same instructions into one vectorized instruction. Note that instr1
 * should dominate instr2.
 */
static nir_instr *
instr_try_combine(struct nir_shader *nir, nir_instr *instr1, nir_instr *instr2,
                  struct set *instr_set, nir_opt_vectorize_cb filter, void *data)
{
   assert(instr1->type == nir_instr_type_alu);
   assert(instr2->type == nir_instr_type_alu);
   nir_alu_instr *alu1 = nir_instr_as_alu(instr1);
   nir_alu_instr *alu2 = nir_instr_as_alu(instr2);
   assert(alu1->dest.dest.ssa.bit_size == alu2->dest.dest.ssa.bit_size);
   unsigned alu1_components = alu1->dest.dest.ssa.num_components;
   unsigned alu2_components = alu2->dest.dest.ssa.num_components;
   unsigned total_components = alu1_components + alu2_components;

   if (total_components > 4)
      return NULL;

   if (nir->options->vectorize_vec2_16bit) {
      assert(total_components == 2);
      assert(alu1->dest.dest.ssa.bit_size == 16);
   }

   /* For aggressive vectorizations, except for constant merging and f2f16,
    * we only allow one packing instruction per vectorization */
   int beneficial = 1;
   enum src_dependency dependencies[2];
   for (unsigned i = 0; i < nir_op_infos[alu1->op].num_inputs; i++) {
      if (alu1->src[i].src.ssa == alu2->src[i].src.ssa)
         continue;

      beneficial--;
      enum src_dependency dep = check_sources_independent(&alu1->src[i].src,
                                                          &alu2->src[i].src);
      if (dep == dependent)
         return NULL;

      /* we only allow source merging for constants and on two-operand ALUs */
      if (i >= 2) {
         if (dep != constants)
            return NULL;
      } else {
         dependencies[i] = dep;
      }

      if (dep == constants ||
          (alu1->src[i].src.ssa->parent_instr->type == nir_instr_type_alu &&
           alu2->src[i].src.ssa->parent_instr->type == nir_instr_type_alu &&
           nir_instr_as_alu(alu1->src[i].src.ssa->parent_instr)->op == nir_op_f2f16 &&
           nir_instr_as_alu(alu2->src[i].src.ssa->parent_instr)->op == nir_op_f2f16))
         beneficial++;
   }

   if (beneficial < 0)
      return NULL;

   nir_builder b;
   nir_builder_init(&b, nir_cf_node_get_function(&instr1->block->cf_node));

   for (unsigned i = 0; i < nir_op_infos[alu1->op].num_inputs; i++) {
      /* handle src merging case */
      if (alu1->src[i].src.ssa != alu2->src[i].src.ssa) {
         merge_alu_srcs(&b, instr_set, i <= 2 ? dependencies[i] : constants,
                        &alu1->src[i], alu1_components,
                        &alu2->src[i], alu2_components);
      }
   }

   b.cursor = nir_after_instr(instr1);

   nir_alu_instr *new_alu = nir_alu_instr_create(b.shader, alu1->op);
   nir_ssa_dest_init(&new_alu->instr, &new_alu->dest.dest,
                     total_components, alu1->dest.dest.ssa.bit_size, NULL);
   new_alu->dest.write_mask = (1 << total_components) - 1;
   new_alu->instr.pass_flags = alu1->instr.pass_flags;

   /* If either channel is exact, we have to preserve it even if it's
    * not optimal for other channels.
    */
   new_alu->exact = alu1->exact || alu2->exact;

   /* If all channels don't wrap, we can say that the whole vector doesn't
    * wrap.
    */
   new_alu->no_signed_wrap = alu1->no_signed_wrap && alu2->no_signed_wrap;
   new_alu->no_unsigned_wrap = alu1->no_unsigned_wrap && alu2->no_unsigned_wrap;

   for (unsigned i = 0; i < nir_op_infos[alu1->op].num_inputs; i++) {
      assert(alu1->src[i].src.ssa == alu2->src[i].src.ssa);
      new_alu->src[i].src = alu1->src[i].src;

      for (unsigned j = 0; j < alu1_components; j++)
         new_alu->src[i].swizzle[j] = alu1->src[i].swizzle[j];

      for (unsigned j = 0; j < alu2_components; j++) {
         new_alu->src[i].swizzle[j + alu1_components] =
            alu2->src[i].swizzle[j];
      }
   }

   nir_builder_instr_insert(&b, &new_alu->instr);

   rewrite_sources(&b, &alu1->dest.dest.ssa, &alu2->dest.dest.ssa,
                   &new_alu->dest.dest.ssa, instr_set);

   assert(list_is_empty(&alu1->dest.dest.ssa.uses));
   assert(list_is_empty(&alu1->dest.dest.ssa.if_uses));
   assert(list_is_empty(&alu2->dest.dest.ssa.uses));
   assert(list_is_empty(&alu2->dest.dest.ssa.if_uses));

   nir_instr_remove(instr1);
   nir_instr_remove(instr2);

   return &new_alu->instr;
}

static bool
vec_instr_set_add_or_rewrite(struct nir_shader *nir, struct set *instr_set,
                             nir_instr *instr,
                             nir_opt_vectorize_cb filter, void *data,
                             bool aggressive)
{
   if (!instr_can_rewrite(instr, nir->options->vectorize_vec2_16bit))
      return false;

   if (filter && !filter(instr, data))
      return false;

   /* set max vector to instr pass flags: this is used to hash swizzles */
   instr->pass_flags = (aggressive ? 1 : 0) << 4 |
                       (nir->options->vectorize_vec2_16bit ? 2 : 4);

   struct set_entry *entry = _mesa_set_search(instr_set, instr);
   if (entry) {
      nir_instr *old_instr = (nir_instr *) entry->key;
      _mesa_set_remove(instr_set, entry);
      nir_instr *new_instr = instr_try_combine(nir, old_instr, instr,
                                               instr_set, filter, data);
      if (new_instr) {
         if (instr_can_rewrite(new_instr, nir->options->vectorize_vec2_16bit))
            _mesa_set_add(instr_set, new_instr);
         return true;
      }
   }

   _mesa_set_add(instr_set, instr);
   return false;
}

static bool
vectorize_block(struct nir_shader *nir, nir_block *block,
                struct set *instr_set,
                nir_opt_vectorize_cb filter, void *data, bool aggressive)
{
   bool progress = false;

   nir_foreach_instr_safe(instr, block) {
      if (vec_instr_set_add_or_rewrite(nir, instr_set, instr, filter, data, aggressive))
         progress = true;
   }

   for (unsigned i = 0; i < block->num_dom_children; i++) {
      nir_block *child = block->dom_children[i];
      progress |= vectorize_block(nir, child, instr_set, filter, data, aggressive);
   }

   nir_foreach_instr_reverse(instr, block) {
      if (instr_can_rewrite(instr, nir->options->vectorize_vec2_16bit))
         _mesa_set_remove_key(instr_set, instr);
   }

   return progress;
}

static bool
nir_opt_vectorize_impl(struct nir_shader *nir, nir_function_impl *impl,
                       nir_opt_vectorize_cb filter, void *data, bool aggressive)
{
   struct set *instr_set = vec_instr_set_create();

   nir_metadata_require(impl, nir_metadata_dominance);

   bool progress = vectorize_block(nir, nir_start_block(impl), instr_set,
                                   filter, data, aggressive);

   if (progress)
      nir_metadata_preserve(impl, nir_metadata_block_index |
                                  nir_metadata_dominance);

   vec_instr_set_destroy(instr_set);
   return progress;
}

bool
nir_opt_vectorize(nir_shader *shader, nir_opt_vectorize_cb filter,
                  void *data, bool aggressive)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (function->impl)
         progress |= nir_opt_vectorize_impl(shader, function->impl, filter,
                                            data, aggressive);
   }

   return progress;
}
