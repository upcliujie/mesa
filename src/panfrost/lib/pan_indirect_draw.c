/*
 * Copyright (C) 2021 Collabora, Ltd.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <stdio.h>
#include "pan_bo.h"
#include "pan_shader.h"
#include "pan_scoreboard.h"
#include "pan_encoder.h"
#include "pan_indirect_draw.h"
#include "pan_pool.h"
#include "pan_util.h"
#include "panfrost-quirks.h"
#include "compiler/nir/nir_builder.h"
#include "util/u_memory.h"
#include "util/macros.h"

#define WORD(x) ((x) * 4)

#define LOOP \
        for (nir_loop *l = nir_push_loop(b); l != NULL; \
             nir_pop_loop(b, l), l = NULL)
#define BREAK nir_jump(b, nir_jump_break)
#define CONTINUE nir_jump(b, nir_jump_continue)

#define IF(cond) nir_push_if(b, cond);
#define ELSE nir_push_else(b, NULL);
#define ENDIF nir_pop_if(b, NULL);

struct draw_data {
        nir_ssa_def *draw_count;
        nir_ssa_def *draw_buf;
        nir_ssa_def *draw_buf_stride;
        nir_ssa_def *index_buf;
        nir_ssa_def *restart_index;
        nir_ssa_def *vertex_count;
        nir_ssa_def *instance_count;
        nir_ssa_def *vertex_start;
        nir_ssa_def *index_bias;
        nir_ssa_def *last_draw;
        nir_ssa_def *first_draw;
        nir_ssa_def *draw_ctx;
};

struct instance_size {
        nir_ssa_def *raw;
        nir_ssa_def *padded;
        nir_ssa_def *packed;
};

struct jobs_data {
        nir_ssa_def *vertex_job;
        nir_ssa_def *tiler_job;
        nir_ssa_def *base_vertex_offset;
        nir_ssa_def *offset_start;
        nir_ssa_def *invocation;
        nir_ssa_def *prev_offset_start;
};

struct varyings_data {
        nir_ssa_def *varying_bufs;
        nir_ssa_def *pos_ptr;
        nir_ssa_def *psiz_ptr;
        nir_register *mem_ptr;
};

struct attribs_data {
        nir_ssa_def *attrib_count;
        nir_ssa_def *attrib_bufs;
        nir_ssa_def *attribs;
};

struct indirect_draw_shader_builder {
        nir_builder b;
        const struct panfrost_device *dev;
        unsigned flags;
        struct draw_data draw;
        struct instance_size instance_size;
        struct jobs_data jobs;
        struct varyings_data varyings;
        struct attribs_data attribs;
};

struct indirect_draw_info {
        uint32_t count;
        uint32_t instance_count;
        uint32_t start;
        int32_t index_bias;
        uint32_t start_instance;
};

struct indirect_draw_context {
        mali_ptr next_job;
        uint32_t draw_idx;
};

struct indirect_draw_inputs {
        mali_ptr draw_ctx;
        mali_ptr draw_buf;
        mali_ptr draw_count_ptr;
        mali_ptr index_buf;
        mali_ptr vertex_job;
        mali_ptr tiler_job;
        mali_ptr attrib_bufs;
        mali_ptr attribs;
        mali_ptr varying_bufs;
        mali_ptr varying_mem;
        uint32_t draw_count;
        uint32_t draw_buf_stride;
        uint32_t restart_index;
        uint32_t attrib_count;
};

static inline unsigned
get_index_size(unsigned flags)
{
        unsigned idx_size = flags & PAN_INDIRECT_DRAW_INDEX_SIZE_MASK;

        return !idx_size ? 0 : 1 << (idx_size - 1);
}

static nir_ssa_def *
get_input_data(nir_builder *b, unsigned offset, unsigned size)
{
        assert(!(offset & 0x3));
        assert(size && !(size & 0x3));

        return nir_load_ubo(b, size / sizeof(uint32_t), 32,
                            nir_imm_int(b, 0),
                            nir_imm_int(b, offset),
                            .align_mul = 4,
                            .align_offset = 0,
                            .range_base = 0,
                            .range = ~0);
}

#define get_input_field(b, name) \
        get_input_data(b, offsetof(struct indirect_draw_inputs, name), \
                       sizeof(((struct indirect_draw_inputs *)0)->name))

static nir_ssa_def *
get_address(nir_builder *b, nir_ssa_def *base, nir_ssa_def *offset)
{
        nir_ssa_def *base_lo = nir_channel(b, base, 0);
        nir_ssa_def *addr_lo = base_lo;
        nir_ssa_def *addr_hi = nir_channel(b, base, 1);
        addr_lo = nir_iadd(b, addr_lo, offset);
        addr_hi = nir_iadd(b, addr_hi,
                           nir_bcsel(b,
                                     nir_ult(b, addr_lo, base_lo),
                                     nir_imm_int(b, 1), nir_imm_int(b, 0)));

        return nir_vec2(b, addr_lo, addr_hi);
}

static nir_ssa_def *
get_address_imm(nir_builder *b, nir_ssa_def *base, unsigned offset)
{
        return get_address(b, base, nir_imm_int(b, offset));
}

static nir_ssa_def *
load_global(nir_builder *b, nir_ssa_def *addr, unsigned ncomps)
{
        return nir_load_global(b, nir_pack_64_2x32(b, addr), 4, ncomps, 32);
}

static void
store_global(nir_builder *b, nir_ssa_def *addr,
             nir_ssa_def *value, unsigned ncomps)
{
        nir_store_global(b, nir_pack_64_2x32(b, addr), 4, value,
                         (1 << ncomps) - 1);
}

static nir_ssa_def *
get_draw_ctx_data(struct indirect_draw_shader_builder *builder,
                  unsigned offset, unsigned size)
{
        nir_builder *b = &builder->b;
        return load_global(b,
                           get_address_imm(b, builder->draw.draw_ctx, offset),
                           size / sizeof(uint32_t));
}

static void
set_draw_ctx_data(struct indirect_draw_shader_builder *builder,
                  unsigned offset, nir_ssa_def *value, unsigned size)
{
        nir_builder *b = &builder->b;
        store_global(b,
                     get_address_imm(b, builder->draw.draw_ctx, offset),
                     value, size / sizeof(uint32_t));
}

#define get_draw_ctx_field(builder, name) \
        get_draw_ctx_data(builder, \
                          offsetof(struct indirect_draw_context, name), \
                          sizeof(((struct indirect_draw_context *)0)->name))

#define set_draw_ctx_field(builder, name, val) \
        set_draw_ctx_data(builder, \
                          offsetof(struct indirect_draw_context, name), \
                          val, \
                          sizeof(((struct indirect_draw_context *)0)->name))

#define get_draw_field(b, draw_ptr, field) \
        load_global(b, \
                    get_address_imm(b, draw_ptr, \
                                    offsetof(struct indirect_draw_info, field)), \
                    sizeof(((struct indirect_draw_info *)0)->field) / sizeof(uint32_t))

static void
extract_inputs(struct indirect_draw_shader_builder *builder)
{
        nir_builder *b = &builder->b;

        if (builder->flags & PAN_INDIRECT_DRAW_MULTI_DRAW)
                builder->draw.draw_ctx = get_input_field(b, draw_ctx);

        builder->draw.draw_buf = get_input_field(b, draw_buf);
        builder->draw.draw_buf_stride = get_input_field(b, draw_buf_stride);

        if (builder->flags & PAN_INDIRECT_DRAW_INDIRECT_DRAW_COUNT) {
                builder->draw.draw_count =
                        load_global(b, get_input_field(b, draw_count_ptr), 1);
        } else {
                builder->draw.draw_count = get_input_field(b, draw_count);
        }

        if (get_index_size(builder->flags)) {
                builder->draw.index_buf = get_input_field(b, index_buf);
                if (builder->flags & PAN_INDIRECT_DRAW_PRIMITIVE_RESTART) {
                        builder->draw.restart_index =
                                get_input_field(b, restart_index);
                }
        }

        builder->jobs.vertex_job = get_input_field(b, vertex_job);
        builder->jobs.tiler_job = get_input_field(b, tiler_job);
        builder->attribs.attrib_bufs = get_input_field(b, attrib_bufs);
        builder->attribs.attribs = get_input_field(b, attribs);
        builder->attribs.attrib_count = get_input_field(b, attrib_count);
        builder->varyings.varying_bufs = get_input_field(b, varying_bufs);
        builder->varyings.mem_ptr = nir_local_reg_create(b->impl);
        builder->varyings.mem_ptr->num_components = 2;
        nir_store_reg(b, builder->varyings.mem_ptr,
                      get_input_field(b, varying_mem), 3);
}

static void
init_shader_builder(struct indirect_draw_shader_builder *builder,
                    const struct panfrost_device *dev,
                    unsigned flags)
{
        memset(builder, 0, sizeof(*builder));
        builder->dev = dev;
        builder->flags = flags;

        builder->b =
                nir_builder_init_simple_shader(MESA_SHADER_COMPUTE,
                                               panfrost_get_shader_options(dev),
                                               "indirect_draw(index_size=%d%s%s%s%s%s)",
                                               get_index_size(builder->flags),
                                               flags & PAN_INDIRECT_DRAW_HAS_PSIZ ?
                                               ",psiz" : "",
                                               flags & PAN_INDIRECT_DRAW_INDIRECT_DRAW_COUNT ?
                                               ",indirect_draw_count" : "",
                                               flags & PAN_INDIRECT_DRAW_PRIMITIVE_RESTART ?
                                               ",primitive_restart" : "",
                                               flags & PAN_INDIRECT_DRAW_UPDATE_PRIM_SIZE ?
                                               ",update_primitive_size" : "",
                                               flags & PAN_INDIRECT_DRAW_MULTI_DRAW ?
                                               ",multi_draw" : "");

        nir_builder *b = &builder->b;
        nir_variable_create(b->shader, nir_var_mem_ubo,
                            glsl_uint_type(), "inputs");
        b->shader->info.num_ubos++;

        extract_inputs(builder);
}

static void
update_tiler_next_ptr(struct indirect_draw_shader_builder *builder,
                      nir_ssa_def *job_ptr)
{
        if (!(builder->flags & PAN_INDIRECT_DRAW_MULTI_DRAW))
                return;

        nir_builder *b = &builder->b;
        nir_ssa_def *ctx_next_ptr =
                get_address_imm(b, builder->draw.draw_ctx, WORD(2));
        nir_ssa_def *job_next_ptr =
                get_address_imm(b, job_ptr, WORD(6));

        IF (builder->draw.first_draw) {
                IF (nir_inot(b, builder->draw.last_draw)) {
                        /* First draw: save the next pointer and loop back to the
                         * compute job
                         */

                        nir_ssa_def *compute = load_global(b, ctx_next_ptr, 2);
                        nir_ssa_def *next = load_global(b, job_next_ptr, 2);
                        store_global(b, ctx_next_ptr, next, 2);
                        store_global(b, job_next_ptr, compute, 2);
                } ENDIF
        } ELSE {
                IF (builder->draw.last_draw) {
                        /* Terminal draw: restore the next pointer */
                        nir_ssa_def *next = load_global(b, ctx_next_ptr, 2);
                        store_global(b, job_next_ptr, next, 2);
                } ENDIF
        } ENDIF
}

static void
update_job(struct indirect_draw_shader_builder *builder, enum mali_job_type type)
{
        nir_builder *b = &builder->b;
        nir_ssa_def *job_ptr =
                type == MALI_JOB_TYPE_VERTEX ?
                builder->jobs.vertex_job : builder->jobs.tiler_job;

        /* Update the invocation words. */
        store_global(b, get_address_imm(b, job_ptr, WORD(8)),
                     builder->jobs.invocation, 2);

        unsigned draw_offset =
                type == MALI_JOB_TYPE_VERTEX ?
                pan_section_offset(COMPUTE_JOB, DRAW) :
                pan_is_bifrost(builder->dev) ?
                pan_section_offset(BIFROST_TILER_JOB, DRAW) :
                pan_section_offset(MIDGARD_TILER_JOB, DRAW);
        unsigned prim_offset =
                pan_is_bifrost(builder->dev) ?
                pan_section_offset(BIFROST_TILER_JOB, PRIMITIVE) :
                pan_section_offset(MIDGARD_TILER_JOB, PRIMITIVE);
        unsigned psiz_offset =
                pan_is_bifrost(builder->dev) ?
                pan_section_offset(BIFROST_TILER_JOB, PRIMITIVE_SIZE) :
                pan_section_offset(MIDGARD_TILER_JOB, PRIMITIVE_SIZE);
        unsigned index_size = get_index_size(builder->flags);

        if (type == MALI_JOB_TYPE_TILER) {
                /* Update PRIMITIVE.{base_vertex_offset,count} */
                store_global(b,
                             get_address_imm(b, job_ptr, prim_offset + WORD(1)),
                             builder->jobs.base_vertex_offset, 1);
                store_global(b,
                             get_address_imm(b, job_ptr, prim_offset + WORD(3)),
                             nir_iadd_imm(b, builder->draw.vertex_count, -1), 1);

                if (index_size) {
                        nir_ssa_def *addr =
                                get_address_imm(b, job_ptr, prim_offset + WORD(4));
                        nir_ssa_def *indices = load_global(b, addr, 2);
                        nir_ssa_def *offset =
                                nir_imul_imm(b, builder->draw.vertex_start, index_size);

                        indices = get_address(b, indices, offset);
                        store_global(b, addr, indices, 2);
                }

                /* Update PRIMITIVE_SIZE.size_array */
                if ((builder->flags & PAN_INDIRECT_DRAW_HAS_PSIZ) &&
                    (builder->flags & PAN_INDIRECT_DRAW_UPDATE_PRIM_SIZE)) {
                        store_global(b,
                                     get_address_imm(b, job_ptr, psiz_offset + WORD(0)),
                                     builder->varyings.psiz_ptr, 2);
                }

                /* Update DRAW.position */
                store_global(b, get_address_imm(b, job_ptr, draw_offset + WORD(4)),
                             builder->varyings.pos_ptr, 2);

                update_tiler_next_ptr(builder, job_ptr);
        }

        nir_ssa_def *draw_w01 =
                load_global(b, get_address_imm(b, job_ptr, draw_offset + WORD(0)), 2);
        nir_ssa_def *draw_w0 = nir_channel(b, draw_w01, 0);

        if (builder->flags & PAN_INDIRECT_DRAW_MULTI_DRAW) {
                /* Retrieve the previous offset_start before updating it
                 * (needed to adjust attrib offsets).
                 */
                nir_ssa_def *prev_instance_size = nir_ushr_imm(b, draw_w0, 16);
                nir_ssa_def *prev_offset_start = nir_channel(b, draw_w01, 1);
                builder->jobs.prev_offset_start =
                        nir_bcsel(b, nir_ieq_imm(b, prev_instance_size, 0),
                                  nir_imm_int(b, 0), prev_offset_start);
        } else {
                builder->jobs.prev_offset_start = nir_imm_int(b, 0);
        }

        /* Update DRAW.{instance_size,offset_start} */
        nir_ssa_def *instance_size =
                nir_bcsel(b,
                          nir_ilt(b, builder->draw.instance_count, nir_imm_int(b, 2)),
                          nir_imm_int(b, 0), builder->instance_size.packed);
        draw_w01 = nir_vec2(b,
                            nir_ior(b, nir_iand_imm(b, draw_w0, 0xffff),
                                    nir_ishl(b, instance_size, nir_imm_int(b, 16))),
                            builder->jobs.offset_start);
        store_global(b, get_address_imm(b, job_ptr, draw_offset + WORD(0)),
                     draw_w01, 2);
}

static void
split_div(nir_builder *b, nir_ssa_def *div, nir_ssa_def **r_e, nir_ssa_def **d)
{
        nir_ssa_def *r = nir_imax(b, nir_ufind_msb(b, div), nir_imm_int(b, 0));
        nir_ssa_def *div64 = nir_u2u64(b, div);
        nir_ssa_def *half_div64 = nir_u2u64(b, nir_ushr_imm(b, div, 1));
        nir_ssa_def *f0 = nir_iadd(b,
                                   nir_ishl(b, nir_imm_int64(b, 1),
                                            nir_iadd_imm(b, r, 32)),
                                   half_div64);
        nir_ssa_def *fi = nir_idiv(b, f0, div64);
        nir_ssa_def *ff = nir_isub(b, f0, nir_imul(b, fi, div64));
        nir_ssa_def *e = nir_bcsel(b, nir_ilt(b, half_div64, ff),
                                   nir_imm_int(b, 1 << 5), nir_imm_int(b, 0));
        *d = nir_iand_imm(b, nir_u2u32(b, fi), ~(1 << 31));
        *r_e = nir_ior(b, r, e);
}

static void
update_vertex_attrib_buf(struct indirect_draw_shader_builder *builder,
                         nir_ssa_def *attrib_buf_ptr,
                         enum mali_attribute_type type,
                         nir_ssa_def *div1,
                         nir_ssa_def *div2)
{
        nir_builder *b = &builder->b;
        unsigned type_mask = BITFIELD_MASK(6);
        nir_ssa_def *w01 = load_global(b, attrib_buf_ptr, 2);
        nir_ssa_def *w0 = nir_channel(b, w01, 0);
        nir_ssa_def *w1 = nir_channel(b, w01, 1);

        w0 = nir_iand_imm(b, nir_channel(b, w01, 0), ~type_mask);
        w0 = nir_ior(b, w0, nir_imm_int(b, type));
        w1 = nir_ior(b, w1, nir_ishl(b, div1, nir_imm_int(b, 24)));

        store_global(b, attrib_buf_ptr, nir_vec2(b, w0, w1), 2);

        if (type == MALI_ATTRIBUTE_TYPE_1D_NPOT_DIVISOR) {
                assert(div2);
                store_global(b, get_address_imm(b, attrib_buf_ptr, WORD(5)),
                             div2, 1);
        }
}

static void
adjust_attrib_offset(struct indirect_draw_shader_builder *builder,
                     nir_ssa_def *attrib_ptr, nir_ssa_def *attrib_buf_ptr)
{
        nir_builder *b = &builder->b;
        nir_ssa_def *zero = nir_imm_int(b, 0);
        nir_ssa_def *two = nir_imm_int(b, 0);
        nir_ssa_def *add_prev_offset =
                nir_ine(b, builder->jobs.prev_offset_start, zero);
        nir_ssa_def *sub_cur_offset =
                nir_iand(b, nir_ine(b, builder->jobs.offset_start, zero),
                         nir_ige(b, builder->draw.instance_count, two));

        IF (nir_ior(b, add_prev_offset, sub_cur_offset)) {
                nir_ssa_def *stride =
                        load_global(b, get_address_imm(b, attrib_buf_ptr, WORD(2)), 1);
                nir_ssa_def *offset =
                        load_global(b, get_address_imm(b, attrib_ptr, WORD(1)), 1);

                offset = nir_iadd(b, offset,
                                  nir_imul(b, stride,
                                  builder->jobs.prev_offset_start));
                offset = nir_isub(b, offset,
                                  nir_imul(b, stride,
                                  builder->jobs.offset_start));
                store_global(b, get_address_imm(b, attrib_ptr, WORD(1)),
                             offset, 1);
        } ENDIF
}

static void
update_vertex_attribs(struct indirect_draw_shader_builder *builder)
{
        nir_builder *b = &builder->b;
        nir_register *attrib_idx_reg = nir_local_reg_create(b->impl);
        attrib_idx_reg->num_components = 1;
        nir_store_reg(b, attrib_idx_reg, nir_imm_int(b, 0), 1);
        nir_ssa_def *single_instance =
                nir_ilt(b, builder->draw.instance_count, nir_imm_int(b, 2));

        LOOP {
                nir_ssa_def *attrib_idx = nir_load_reg(b, attrib_idx_reg);
                IF (nir_ige(b, attrib_idx, builder->attribs.attrib_count))
                        BREAK;
                ENDIF

                nir_ssa_def *attrib_buf_ptr =
                         get_address(b, builder->attribs.attrib_bufs,
                                     nir_imul_imm(b, attrib_idx,
                                                  2 * MALI_ATTRIBUTE_BUFFER_LENGTH));
                nir_ssa_def *attrib_ptr =
                         get_address(b, builder->attribs.attribs,
                                     nir_imul_imm(b, attrib_idx,
                                                  MALI_ATTRIBUTE_LENGTH));

                nir_ssa_def *r_e, *d;

                if (!pan_is_bifrost(builder->dev)) {
                        IF (nir_ieq_imm(b, attrib_idx, PAN_VERTEX_ID)) {
                                nir_ssa_def *r_p =
                                        nir_bcsel(b, single_instance,
                                                  nir_imm_int(b, 0x9f),
                                                  builder->instance_size.packed);

                                store_global(b,
                                             get_address_imm(b, attrib_buf_ptr, WORD(4)),
                                             nir_ishl(b, r_p, nir_imm_int(b, 24)), 1);

                                nir_store_reg(b, attrib_idx_reg,
                                              nir_iadd_imm(b, attrib_idx, 1), 1);
                                CONTINUE;
                        } ENDIF

                        IF (nir_ieq_imm(b, attrib_idx, PAN_INSTANCE_ID)) {
                                split_div(b, builder->instance_size.padded,
                                          &r_e, &d);
                                nir_ssa_def *default_div =
                                        nir_ior(b, single_instance,
                                                nir_ilt(b,
                                                        builder->instance_size.padded,
                                                        nir_imm_int(b, 2)));
                                r_e = nir_bcsel(b, default_div,
                                                nir_imm_int(b, 0x3f), r_e);
                                d = nir_bcsel(b, default_div,
                                              nir_imm_int(b, (1u << 31) - 1), d);
                                store_global(b,
                                             get_address_imm(b, attrib_buf_ptr, WORD(1)),
                                             nir_vec2(b, nir_ishl(b, r_e, nir_imm_int(b, 24)), d),
                                             2);
                                nir_store_reg(b, attrib_idx_reg,
                                              nir_iadd_imm(b, attrib_idx, 1), 1);
                                CONTINUE;
                        } ENDIF
                }

                nir_ssa_def *div =
                        load_global(b, get_address_imm(b, attrib_buf_ptr, WORD(7)), 1);

                div = nir_imul(b, div, builder->instance_size.padded);

                nir_ssa_def *multi_instance =
                        nir_ige(b, builder->draw.instance_count, nir_imm_int(b, 2));

                IF (nir_ine(b, div, nir_imm_int(b, 0))) {
                        IF (multi_instance) {
                                nir_ssa_def *div_pow2 =
                                        nir_ilt(b, nir_bit_count(b, div), nir_imm_int(b, 2));

                                IF (div_pow2) {
                                        nir_ssa_def *exp =
                                                nir_imax(b, nir_ufind_msb(b, div),
                                                         nir_imm_int(b, 0));
                                        update_vertex_attrib_buf(builder, attrib_buf_ptr,
                                                                 MALI_ATTRIBUTE_TYPE_1D_POT_DIVISOR,
                                                                 exp, NULL);
                                } ELSE {
                                        split_div(b, div, &r_e, &d);
                                        update_vertex_attrib_buf(builder, attrib_buf_ptr,
                                                                 MALI_ATTRIBUTE_TYPE_1D_NPOT_DIVISOR,
                                                                 r_e, d);
                                } ENDIF
                        } ELSE {
                                /* Single instance with a non-0 divisor: all
                                 * accesses should point to attribute 0, pick
                                 * the biggest pot divisor.
                                 */
                                update_vertex_attrib_buf(builder, attrib_buf_ptr,
                                                         MALI_ATTRIBUTE_TYPE_1D_POT_DIVISOR,
                                                         nir_imm_int(b, 31), NULL);
                        } ENDIF

                        adjust_attrib_offset(builder, attrib_ptr, attrib_buf_ptr);
                } ELSE {
                        IF (multi_instance) {
                                update_vertex_attrib_buf(builder, attrib_buf_ptr,
                                                         MALI_ATTRIBUTE_TYPE_1D_MODULUS,
                                                         builder->instance_size.packed, NULL);
                        } ELSE {
                                update_vertex_attrib_buf(builder, attrib_buf_ptr,
                                                         MALI_ATTRIBUTE_TYPE_1D,
                                                         nir_imm_int(b, 0), NULL);
			} ENDIF
                } ENDIF

                nir_store_reg(b, attrib_idx_reg, nir_iadd_imm(b, attrib_idx, 1), 1);
        }
}

#if 0
/*
 * Needed if we want to patch streamout buffer entries
 */
static nir_ssa_def *
count_output_vertices(nir_builder *b, nir_ssa_def *prim_info,
                      nir_ssa_def *instance_size)
{
        nir_ssa_def *min = nir_iand_imm(b, prim_info, 7);
        nir_ssa_def *incr =
                nir_iand_imm(b, nir_ushr_imm(b, prim_info, 3), 7);
        nir_ssa_def *div =
                nir_iand_imm(b, nir_ushr_imm(b, prim_info, 6), 7);
        nir_ssa_def *bias =
                nir_iand_imm(b, nir_ushr_imm(b, prim_info, 9), 3);

        nir_ssa_def *vertex_count =
                nir_imul(b, nir_idiv(b, instance_size, incr), incr);

        vertex_count = nir_bcsel(b, nir_ilt(b, vertex_count, min),
                                 nir_imm_int(b, 0), vertex_count);
        nir_ssa_def *prim_count =
                nir_isub(b,
                         nir_idiv(b, vertex_count, nir_imax(b, div, nir_imm_int(b, 1))),
                         bias);

        return nir_bcsel(b, nir_ieq_imm(b, div, 0), vertex_count,
                         nir_imul(b, prim_count, min));
}
#endif

static nir_ssa_def *
update_varying_buf(struct indirect_draw_shader_builder *builder,
                   nir_ssa_def *varying_buf_ptr,
                   nir_ssa_def *vertex_count)
{
        nir_builder *b = &builder->b;

        nir_ssa_def *stride =
                load_global(b, get_address_imm(b, varying_buf_ptr, WORD(2)), 1);
        nir_ssa_def *size = nir_imul(b, stride, vertex_count);
        nir_ssa_def *aligned_size =
                nir_iand_imm(b, nir_iadd_imm(b, size, 63), ~63);
        nir_ssa_def *var_mem_ptr =
                nir_load_reg(b, builder->varyings.mem_ptr);
        nir_ssa_def *w0 =
                nir_ior(b, nir_channel(b, var_mem_ptr, 0),
                        nir_imm_int(b, MALI_ATTRIBUTE_TYPE_1D));
        nir_ssa_def *w1 = nir_channel(b, var_mem_ptr, 1);
        store_global(b, get_address_imm(b, varying_buf_ptr, WORD(0)),
                     nir_vec4(b, w0, w1, stride, size), 4);

        nir_store_reg(b, builder->varyings.mem_ptr,
                      get_address(b, var_mem_ptr, aligned_size), 3);

        return var_mem_ptr;
}

static void
update_varyings(struct indirect_draw_shader_builder *builder)
{
        nir_builder *b = &builder->b;
        nir_ssa_def *vertex_count =
                nir_imul(b, builder->instance_size.padded,
                         builder->draw.instance_count);
        nir_ssa_def *buf_ptr =
                get_address_imm(b, builder->varyings.varying_bufs,
                                PAN_VARY_GENERAL *
                                MALI_ATTRIBUTE_BUFFER_LENGTH);
        update_varying_buf(builder, buf_ptr, vertex_count);

        buf_ptr = get_address_imm(b, builder->varyings.varying_bufs,
                                  PAN_VARY_POSITION *
                                  MALI_ATTRIBUTE_BUFFER_LENGTH);
        builder->varyings.pos_ptr =
                update_varying_buf(builder, buf_ptr, vertex_count);

        if (builder->flags & PAN_INDIRECT_DRAW_HAS_PSIZ) {
                buf_ptr = get_address_imm(b, builder->varyings.varying_bufs,
                                          PAN_VARY_PSIZ *
                                          MALI_ATTRIBUTE_BUFFER_LENGTH);
                builder->varyings.psiz_ptr =
                        update_varying_buf(builder, buf_ptr, vertex_count);
        }
}

static void
get_invocation(struct indirect_draw_shader_builder *builder)
{
        nir_builder *b = &builder->b;
        nir_ssa_def *one = nir_imm_int(b, 1);
        nir_ssa_def *max_vertex =
                nir_usub_sat(b, builder->instance_size.raw, one);
        nir_ssa_def *max_instance =
                nir_usub_sat(b, builder->draw.instance_count, one);
        nir_ssa_def *split =
                nir_bcsel(b, nir_ieq_imm(b, max_instance, 0),
                          nir_imm_int(b, 32),
                          nir_iadd_imm(b, nir_ufind_msb(b, max_vertex), 1));

        builder->jobs.invocation =
                nir_vec2(b,
                         nir_ior(b, max_vertex,
                                 nir_ishl(b, max_instance, split)),
                         nir_ior(b, nir_ishl(b, split, nir_imm_int(b, 22)),
                                 nir_imm_int(b, 2 << 28)));
}

static nir_ssa_def *
get_padded_count(nir_builder *b, nir_ssa_def *val, nir_ssa_def **packed)
{
        nir_ssa_def *one = nir_imm_int(b, 1);
        nir_ssa_def *zero = nir_imm_int(b, 0);
        nir_ssa_def *eleven = nir_imm_int(b, 11);
        nir_ssa_def *four = nir_imm_int(b, 4);

        nir_ssa_def *exp =
                nir_usub_sat(b, nir_imax(b, nir_ufind_msb(b, val), zero), four);
        nir_ssa_def *base = nir_ushr(b, val, exp);

        base = nir_iadd(b, base,
                        nir_bcsel(b, nir_ine(b, val, nir_ishl(b, base, exp)), one, zero));

        nir_ssa_def *rshift = nir_imax(b, nir_find_lsb(b, base), zero);
        exp = nir_iadd(b, exp, rshift);
        base = nir_ushr(b, base, rshift);
        base = nir_iadd(b, base, nir_bcsel(b, nir_ige(b, base, eleven), one, zero));
        rshift = nir_imax(b, nir_find_lsb(b, base), zero);
        exp = nir_iadd(b, exp, rshift);
        base = nir_ushr(b, base, rshift);

        *packed = nir_ior(b, exp,
                          nir_ishl(b, nir_ushr_imm(b, base, 1), nir_imm_int(b, 5)));
        return nir_ishl(b, base, exp);
}

static void
update_jobs(struct indirect_draw_shader_builder *builder)
{
        get_invocation(builder);
        update_job(builder, MALI_JOB_TYPE_VERTEX);
        update_job(builder, MALI_JOB_TYPE_TILER);
}

static void
get_instance_size(struct indirect_draw_shader_builder *builder)
{
        unsigned index_size = get_index_size(builder->flags);
        nir_builder *b = &builder->b;

        if (!index_size) {
                builder->jobs.base_vertex_offset = nir_imm_int(b, 0);
                builder->jobs.offset_start = builder->draw.vertex_start;
                builder->instance_size.raw = builder->draw.vertex_count;
                return;
        }

        nir_register *idx_reg = nir_local_reg_create(b->impl);
        idx_reg->num_components = 1;
        nir_store_reg(b, idx_reg, builder->draw.vertex_start, 1);

        nir_register *min_reg = nir_local_reg_create(b->impl);
        min_reg->num_components = 1;
        nir_store_reg(b, min_reg,
                      nir_imm_int(b, (1ULL << (index_size * 8)) - 1),
                      1);
        nir_register *max_reg = nir_local_reg_create(b->impl);
        max_reg->num_components = 1;
        nir_store_reg(b, max_reg, nir_imm_int(b, 0), 1);

        nir_ssa_def *end =
                nir_iadd(b, builder->draw.vertex_start,
                         builder->draw.vertex_count);

        LOOP {
                nir_ssa_def *idx = nir_load_reg(b, idx_reg);
                IF (nir_ige(b, idx, end))
                        BREAK;
                ENDIF

                nir_ssa_def *idx_offset = nir_imul_imm(b, idx, index_size);
                nir_ssa_def *addr =
                        get_address(b, builder->draw.index_buf,
                                    nir_iand(b, idx_offset, nir_imm_int(b, ~3)));

                nir_ssa_def *val = load_global(b, addr, 1);

                nir_ssa_def *shift =
                        nir_imul_imm(b, nir_iand_imm(b, idx_offset, 3), 8);

                val = nir_iand_imm(b, nir_ushr(b, val, shift),
                                   (1ull << (index_size * 8)) - 1);

                if (builder->draw.restart_index) {
                        IF (nir_ine(b, val, builder->draw.restart_index)) {
                                nir_store_reg(b, min_reg,
                                              nir_umin(b, nir_load_reg(b, min_reg), val),
                                              1);
                                nir_store_reg(b, max_reg,
                                              nir_umax(b, nir_load_reg(b, max_reg), val),
                                              1);
                        } ENDIF
                } else {
                        nir_store_reg(b, min_reg,
                                      nir_umin(b, nir_load_reg(b, min_reg), val),
                                      1);
                        nir_store_reg(b, max_reg,
                                      nir_umax(b, nir_load_reg(b, max_reg), val),
                                      1);
                }

                nir_store_reg(b, idx_reg, nir_iadd_imm(b, idx, 1), 1);
        }

        nir_ssa_def *min = nir_load_reg(b, min_reg);
        nir_ssa_def *max = nir_load_reg(b, max_reg);
        builder->jobs.base_vertex_offset = nir_ineg(b, min);
        builder->jobs.offset_start = nir_iadd(b, min, builder->draw.index_bias);
        builder->instance_size.raw = nir_iadd_imm(b, nir_usub_sat(b, max, min), 1);
}

static void
draw(struct indirect_draw_shader_builder *builder)
{
        unsigned index_size = get_index_size(builder->flags);
        nir_builder *b = &builder->b;

        nir_ssa_def *draw_idx;

        if (builder->flags & PAN_INDIRECT_DRAW_MULTI_DRAW) {
                draw_idx = get_draw_ctx_field(builder, draw_idx);

                nir_ssa_def *next_draw_idx = nir_iadd_imm(b, draw_idx, 1);
                builder->draw.last_draw =
                        nir_ige(b, next_draw_idx, builder->draw.draw_count);
                builder->draw.first_draw = nir_ieq_imm(b, draw_idx, 0);
                set_draw_ctx_field(builder, draw_idx, next_draw_idx);
        } else {
                draw_idx = nir_imm_int(b, 0);
        }

        nir_ssa_def *draw_ptr =
                get_address(b, builder->draw.draw_buf,
                            nir_imul(b, draw_idx, builder->draw.draw_buf_stride));

        builder->draw.vertex_count = get_draw_field(b, draw_ptr, count);
        assert(builder->draw.vertex_count->num_components);
        builder->draw.instance_count =
                get_draw_field(b, draw_ptr, instance_count);
        builder->draw.vertex_start = get_draw_field(b, draw_ptr, start);
        if (index_size) {
                builder->draw.index_bias =
                        get_draw_field(b, draw_ptr, index_bias);
        }

        /* start_instance is ignored since we don't support gl_BaseInstance yet */

        get_instance_size(builder);

        builder->instance_size.padded =
                get_padded_count(b, builder->instance_size.raw,
                                 &builder->instance_size.packed);

        update_varyings(builder);
        update_jobs(builder);
        update_vertex_attribs(builder);
}

static void
prepare_bifrost_shader_state(nir_shader *s,
                             panfrost_program *prog,
                             struct panfrost_bo *shader_bo,
                             struct MALI_RENDERER_STATE *state)
{
        state->shader.shader = shader_bo->ptr.gpu;
        state->properties.uniform_buffer_count = MAX2(s->info.num_ubos, 1);
        state->preload.uniform_count =
                MIN2(s->num_uniforms + prog->sysval_count, prog->uniform_cutoff);
        state->preload.compute.local_invocation_xy = true;
        state->preload.compute.local_invocation_z = true;
        state->preload.compute.work_group_x = true;
        state->preload.compute.work_group_y = true;
        state->preload.compute.work_group_z = true;
        state->preload.compute.global_invocation_x = true;
        state->preload.compute.global_invocation_y = true;
        state->preload.compute.global_invocation_z = true;
}

static void
prepare_midgard_shader_state(nir_shader *s,
                             panfrost_program *prog,
                             struct panfrost_bo *shader_bo,
                             struct MALI_RENDERER_STATE *state)
{
        state->shader.shader = shader_bo->ptr.gpu | prog->first_tag;
        state->properties.uniform_buffer_count = s->info.num_ubos + 1;
        state->properties.midgard.uniform_count =
                MIN2(s->num_uniforms + prog->sysval_count, prog->uniform_cutoff);
        state->properties.midgard.shader_has_side_effects = s->info.writes_memory;
        state->properties.midgard.work_register_count = prog->work_register_count;
}

static void
prepare_shader_state(struct panfrost_device *dev,
                     nir_shader *s,
                     panfrost_program *prog,
                     struct panfrost_bo *shader_bo,
                     void *out)
{
        pan_pack(out, RENDERER_STATE, state) {
                if (pan_is_bifrost(dev))
                        prepare_bifrost_shader_state(s, prog, shader_bo, &state);
                else
                        prepare_midgard_shader_state(s, prog, shader_bo, &state);
        }
}

static void
create_indirect_draw_shader(struct panfrost_device *dev,
                            unsigned flags)
{
        assert(flags < PAN_INDIRECT_DRAW_NUM_SHADERS);
        struct indirect_draw_shader_builder builder;
        init_shader_builder(&builder, dev, flags);

        nir_builder *b = &builder.b;

        draw(&builder);

        if (pan_is_bifrost(dev))
                NIR_PASS_V(b->shader, nir_lower_uniforms_to_ubo, 16);

        struct panfrost_compile_inputs inputs = { .gpu_id = dev->gpu_id };
        panfrost_program *program =
                panfrost_compile_shader(dev, NULL, b->shader, &inputs);

        struct panfrost_bo *bo =
                panfrost_bo_create(dev, program->compiled.size, PAN_BO_EXECUTE);

        memcpy(bo->ptr.cpu, program->compiled.data, program->compiled.size);

        void *state = dev->indirect_draw_shaders.states->ptr.cpu +
                      (flags * MALI_RENDERER_STATE_LENGTH);

        prepare_shader_state(dev, b->shader, program, bo, state);

        struct pan_indirect_draw_shader *info =
                &dev->indirect_draw_shaders.shaders[flags];

        info->bo = bo;
        assert(!program->tls_size);
        assert(!b->shader->info.cs.shared_size);
        assert(!program->sysval_count);

        ralloc_free(b->shader);
        ralloc_free(program);
}

static mali_ptr
get_renderer_state(struct panfrost_device *dev, unsigned flags)
{
        struct pan_indirect_draw_shader *info =
                &dev->indirect_draw_shaders.shaders[flags];

        if (!info->bo) {
                create_indirect_draw_shader(dev, flags);
                assert(info->bo);
        }

        mali_ptr state = dev->indirect_draw_shaders.states->ptr.gpu +
                         (flags * MALI_RENDERER_STATE_LENGTH);

        return state;
}

static mali_ptr
get_tls(struct pan_pool *pool)
{
        struct panfrost_ptr ptr =
                panfrost_pool_alloc_aligned(pool,
                                            MALI_LOCAL_STORAGE_LENGTH,
                                            64);

        pan_pack(ptr.cpu, LOCAL_STORAGE, ls) {
                ls.wls_base_pointer = 0;
                ls.wls_instances = MALI_LOCAL_STORAGE_NO_WORKGROUP_MEM;
                ls.wls_size_scale = 0;
        };

        return ptr.gpu;
}

static mali_ptr
get_const_bufs(struct pan_pool *pool,
               const struct pan_indirect_draw_info *draw_info,
               mali_ptr compute_job)
{
        struct panfrost_ptr inputs_ptr =
                panfrost_pool_alloc_aligned(pool,
                                            ALIGN(sizeof(struct indirect_draw_inputs), 16),
					    16);

        struct indirect_draw_inputs *inputs = inputs_ptr.cpu;

        inputs->draw_buf = draw_info->draw_buf;
        inputs->draw_count_ptr = draw_info->draw_count_ptr;
        inputs->index_buf = draw_info->index_buf;
        inputs->vertex_job = draw_info->vertex_job;
        inputs->tiler_job = draw_info->tiler_job;
        inputs->attrib_bufs = draw_info->attrib_bufs;
        inputs->attribs = draw_info->attribs;
        inputs->varying_bufs = draw_info->varying_bufs;
        inputs->varying_mem = draw_info->varying_mem;
        inputs->draw_count = draw_info->draw_count;
        inputs->draw_buf_stride = draw_info->draw_buf_stride;
        inputs->restart_index = draw_info->restart_index;
        inputs->attrib_count = draw_info->attrib_count;

        if (draw_info->flags & PAN_INDIRECT_DRAW_MULTI_DRAW) {
                struct panfrost_ptr draw_ctx_ptr =
                        panfrost_pool_alloc_aligned(pool,
                                                    sizeof(struct indirect_draw_context),
                                                    sizeof(mali_ptr));
                struct indirect_draw_context *draw_ctx = draw_ctx_ptr.cpu;
                draw_ctx->draw_idx = 0;
                draw_ctx->next_job = compute_job;
                inputs->draw_ctx = draw_ctx_ptr.gpu;
                assert(inputs->draw_count > 1);
        } else {
                assert(inputs->draw_count = 1);
                inputs->draw_ctx = 0;
        }

        struct panfrost_ptr ubos_ptr =
                panfrost_pool_alloc_aligned(pool,
                                            MALI_UNIFORM_BUFFER_LENGTH * 2,
                                            MALI_UNIFORM_BUFFER_LENGTH);
        struct mali_uniform_buffer_packed *ubo = ubos_ptr.cpu;

        /* UBO0 is empty: no sysvals no uniform */
        memset(&ubo[0], 0, sizeof(*ubo));

        /* UBO1 contains all the shader inputs */
        pan_pack(&ubo[1], UNIFORM_BUFFER, cfg) {
                cfg.entries = DIV_ROUND_UP(sizeof(*inputs), 16);
                cfg.pointer = inputs_ptr.gpu;
        }

        return ubos_ptr.gpu;
}

struct panfrost_ptr
panfrost_emit_indirect_draw(struct pan_pool *pool,
                            struct pan_scoreboard *scoreboard,
                            const struct pan_indirect_draw_info *draw_info)
{
        struct panfrost_device *dev = pool->dev;
        struct panfrost_ptr ptr =
                panfrost_pool_alloc_aligned(pool,
                                            MALI_COMPUTE_JOB_LENGTH,
                                            64);
        void *job = ptr.cpu;

        void *invocation =
                pan_section_ptr(job, COMPUTE_JOB, INVOCATION);
        panfrost_pack_work_groups_compute(invocation,
                                          1, 1, 1, 1, 1, 1,
                                          false);

        pan_section_pack(job, COMPUTE_JOB, PARAMETERS, cfg) {
                cfg.job_task_split = 2;
        }

        pan_section_pack(job, COMPUTE_JOB, DRAW, cfg) {
                cfg.draw_descriptor_is_64b = true;
                if (pan_is_bifrost(dev))
                        cfg.texture_descriptor_is_64b = true;
                cfg.state = get_renderer_state(dev, draw_info->flags);
                cfg.thread_storage = get_tls(pool);
                cfg.uniform_buffers =
                        get_const_bufs(pool, draw_info, ptr.gpu);
        }

        pan_section_pack(job, COMPUTE_JOB, DRAW_PADDING, cfg);

        return ptr;
}


void
panfrost_init_indirect_draw_shaders(struct panfrost_device *dev)
{
        unsigned state_bo_size = PAN_INDIRECT_DRAW_NUM_SHADERS *
                                 MALI_RENDERER_STATE_LENGTH;

        dev->indirect_draw_shaders.states =
                panfrost_bo_create(dev, state_bo_size, 0);
}

void
panfrost_cleanup_indirect_draw_shaders(struct panfrost_device *dev)
{
        for (unsigned i = 0; PAN_INDIRECT_DRAW_NUM_SHADERS; i++) {
                struct pan_indirect_draw_shader *info =
                        &dev->indirect_draw_shaders.shaders[i];
                panfrost_bo_unreference(info->bo);
        }

        panfrost_bo_unreference(dev->indirect_draw_shaders.states);
}
