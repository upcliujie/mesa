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
#include "pan_scoreboard.h"
#include "pan_encoder.h"
#include "pan_indirect_draw.h"
#include "pan_pool.h"
#include "pan_util.h"
#include "panfrost-quirks.h"
#include "../midgard/midgard_compile.h"
#include "../bifrost/bifrost_compile.h"
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

enum indirect_draw_ubo_id {
        INDIRECT_DRAW_ARRAY,
        INDEX_ARRAY,
        INDIRECT_DRAW_COUNT_ARRAY,
        NUM_UBOS,
};

enum indirect_draw_ssbo_id {
        VERTEX_JOB,
        TILER_JOB,
        DRAW_IDX,
        ATTRIB_BUFS,
        ATTRIBS,
        VARYING_BUFS,
        NUM_SSBOS,
};

enum indirect_draw_uniform_id {
        DRAW_INFO_UNIFORM,
        VARYING_MEM_UNIFORM,
        NUM_UNIFORMS,
};

struct draw_ctx {
        nir_ssa_def *count;
        nir_ssa_def *instance_count;
        nir_ssa_def *start;
        nir_ssa_def *index_bias;
        nir_ssa_def *instance_size;
        nir_ssa_def *padded_instance_size;
        nir_ssa_def *packed_instance_size;
        nir_ssa_def *base_vertex_offset;
        nir_ssa_def *offset_start;
        nir_ssa_def *desc_buf_offset;
        unsigned desc_offset;
        nir_ssa_def *invocation;
        nir_deref_instr *pos_ptr;
        nir_deref_instr *psiz_ptr;
        nir_ssa_def *last;
};

struct indirect_draw_shader_ctx {
        nir_builder b;
        bool is_bifrost;
        bool primitive_restart;
        bool indirect_draw_count;
        bool has_psiz;
        bool update_prim_size;
        unsigned index_size;
        nir_ssa_def *draw_count;
        nir_ssa_def *draw_buf_stride;
        nir_ssa_def *attrib_count;
        nir_ssa_def *restart_index;
        nir_deref_instr *varying_mem_ptr;
        struct draw_ctx draw;
};

static void
extract_draw_info(struct indirect_draw_shader_ctx *ctx)
{
        nir_builder *b = &ctx->b;
        nir_variable *uniform =
                nir_variable_create(b->shader, nir_var_uniform,
                                    glsl_vector_type(GLSL_TYPE_UINT, 4),
                                    "draw_info");
        uniform->data.driver_location = DRAW_INFO_UNIFORM;
        nir_ssa_def *draw_info =
                nir_load_deref(b, nir_build_deref_var(b, uniform));

        if (!ctx->indirect_draw_count) {
                ctx->draw_count = nir_channel(b, draw_info, 0);
        } else {
                nir_ssa_def *offset = nir_channel(b, draw_info, 0);
                nir_ssa_def *ubo_id = nir_imm_int(b, INDIRECT_DRAW_COUNT_ARRAY);

                ctx->draw_count = nir_load_ubo(b, 1, 32, ubo_id, offset,
                                               .align_mul = 4,
                                               .align_offset = 0,
                                               .range_base = 0,
                                               .range = ~0);
        }

        ctx->attrib_count = nir_channel(b, draw_info, 1);
        ctx->draw_buf_stride = nir_channel(b, draw_info, 2);

        if (ctx->primitive_restart)
                ctx->restart_index = nir_channel(b, draw_info, 3);
}

static void
extract_descs_info(struct indirect_draw_shader_ctx *ctx)
{
        nir_builder *b = &ctx->b;
        nir_variable *uniform =
                nir_variable_create(b->shader, nir_var_uniform,
                                    glsl_vector_type(GLSL_TYPE_UINT, 2),
                                    "varying_mem");
        uniform->data.driver_location = VARYING_MEM_UNIFORM;
        nir_ssa_def *varying_mem =
                nir_load_deref(b, nir_build_deref_var(b, uniform));

        nir_variable *var_mem_ptr_var =
                nir_local_variable_create(b->impl,
                                          glsl_vector_type(GLSL_TYPE_UINT, 2),
                                          "var_mem_ptr");
        ctx->varying_mem_ptr = nir_build_deref_var(b, var_mem_ptr_var);
        nir_store_deref(b, ctx->varying_mem_ptr, varying_mem, 3);
}

static void
init_shader_ctx(struct indirect_draw_shader_ctx *ctx,
                const struct panfrost_device *dev,
                unsigned index_size, bool has_psiz,
                bool indirect_draw_count,
                bool primitive_restart,
                bool update_prim_size)
{
        memset(ctx, 0, sizeof(*ctx));
        ctx->indirect_draw_count = indirect_draw_count;
        ctx->index_size = index_size;
        ctx->primitive_restart = primitive_restart;
        ctx->has_psiz = has_psiz;
        ctx->update_prim_size = update_prim_size;
        ctx->is_bifrost = dev->quirks & IS_BIFROST;

        nir_shader *shader = nir_shader_create(NULL, MESA_SHADER_COMPUTE,
                                               ctx->is_bifrost ?
                                               &bifrost_nir_options :
                                               &midgard_nir_options,
                                               NULL);
        nir_function *fn = nir_function_create(shader, "main");
        fn->is_entrypoint = true;
        nir_function_impl *impl = nir_function_impl_create(fn);

        nir_builder *b = &ctx->b;
        nir_builder_init(b, impl);
        ctx->b.cursor = nir_before_block(nir_start_block(impl));

        static const char *ubo_names[] = {
                [INDIRECT_DRAW_ARRAY] = "draws",
                [INDEX_ARRAY] = "indices",
                [INDIRECT_DRAW_COUNT_ARRAY] = "draw_count",
        };

        for (unsigned i = 0; i < ARRAY_SIZE(ubo_names); i++) {
                if (i == INDEX_ARRAY && ctx->index_size == 0)
                        continue;

                if (i == INDIRECT_DRAW_COUNT_ARRAY &&
                    !ctx->indirect_draw_count)
                        continue;

                nir_variable *ubo =
                        nir_variable_create(shader, nir_var_mem_ubo,
                                            glsl_uint_type(), ubo_names[i]);
                ubo->data.driver_location = i;
                ubo->data.binding = i;
                shader->info.num_ubos++;
        }

        static const char *ssbo_names[] = {
                [VERTEX_JOB] = "vertex_job",
                [TILER_JOB] = "tiler_job",
                [DRAW_IDX] = "draw_idx",
                [ATTRIB_BUFS] = "attrib_bufs",
                [ATTRIBS] = "attribs",
                [VARYING_BUFS] = "varying_bufs",
        };

        for (unsigned i = 0; i < ARRAY_SIZE(ssbo_names); i++) {
                nir_variable *ssbo =
                        nir_variable_create(shader, nir_var_mem_ssbo,
                                            glsl_uint_type(), ssbo_names[i]);
                ssbo->data.driver_location = i;
                ssbo->data.binding = i;
        }

        extract_draw_info(ctx);
        extract_descs_info(ctx);
}

static nir_ssa_def *
load_desc(struct indirect_draw_shader_ctx *ctx,
          enum indirect_draw_ssbo_id ssbo_id,
          unsigned offset, unsigned ncomps)
{
        nir_builder *b = &ctx->b;
        nir_ssa_def *offs =
                ctx->draw.desc_buf_offset ?
                nir_iadd_imm(b, ctx->draw.desc_buf_offset, offset) :
                nir_imm_int(b, offset);

        return nir_load_ssbo(b, ncomps, 32, nir_imm_int(b, ssbo_id), offs,
                             .align_mul = 4);
}

static void
store_desc(struct indirect_draw_shader_ctx *ctx,
           enum indirect_draw_ssbo_id ssbo_id,
           unsigned offset, nir_ssa_def *val, unsigned ncomps)
{
        nir_builder *b = &ctx->b;
        nir_ssa_def *offs =
                ctx->draw.desc_buf_offset ?
                nir_iadd_imm(b, ctx->draw.desc_buf_offset, offset) :
                nir_imm_int(b, offset);

        nir_store_ssbo(b, val, nir_imm_int(b, ssbo_id), offs,
                       .write_mask = (1 << ncomps) - 1,
                       .align_mul = 4);
}

static void
update_job(struct indirect_draw_shader_ctx *ctx, enum mali_job_type type)
{
        nir_builder *b = &ctx->b;
        enum indirect_draw_ssbo_id ssbo =
                type == MALI_JOB_TYPE_VERTEX ? VERTEX_JOB : TILER_JOB;

        nir_ssa_def *val = load_desc(ctx, ssbo, WORD(4), 1);

        /* Update the invocation words. */
        store_desc(ctx, ssbo, WORD(8), ctx->draw.invocation, 2);

        unsigned draw_offset =
                type == MALI_JOB_TYPE_VERTEX ?
                pan_section_offset(COMPUTE_JOB, DRAW) :
                ctx->is_bifrost ?
                pan_section_offset(BIFROST_TILER_JOB, DRAW) :
                pan_section_offset(MIDGARD_TILER_JOB, DRAW);
        unsigned prim_offset =
                ctx->is_bifrost ?
                pan_section_offset(BIFROST_TILER_JOB, PRIMITIVE) :
                pan_section_offset(MIDGARD_TILER_JOB, PRIMITIVE);
        unsigned psiz_offset =
                ctx->is_bifrost ?
                pan_section_offset(BIFROST_TILER_JOB, PRIMITIVE_SIZE) :
                pan_section_offset(MIDGARD_TILER_JOB, PRIMITIVE_SIZE);

        if (type == MALI_JOB_TYPE_TILER) {
                /* Update PRIMITIVE.{base_vertex_offset,count} */
                store_desc(ctx, ssbo, prim_offset + WORD(1),
                           ctx->draw.base_vertex_offset, 1);
                store_desc(ctx, ssbo, prim_offset + WORD(3),
                           nir_iadd_imm(b, ctx->draw.count, -1), 1);

                if (ctx->index_size != 0) {
                        nir_ssa_def *indices =
                                load_desc(ctx, ssbo, prim_offset + WORD(4), 2);
                        nir_ssa_def *offset =
                                nir_imul_imm(b, ctx->draw.start, ctx->index_size);

                        indices = nir_pack_64_2x32(b, indices);
                        indices = nir_iadd(b, indices, nir_u2u64(b, offset));
                        indices = nir_unpack_64_2x32(b, indices);
                        store_desc(ctx, ssbo, prim_offset + WORD(4), indices, 2);
                }

                /* Update PRIMITIVE_SIZE.size_array */
                if (ctx->draw.psiz_ptr && ctx->update_prim_size) {
                        store_desc(ctx, ssbo, psiz_offset + WORD(0),
                                   nir_load_deref(b, ctx->draw.psiz_ptr),
                                   2);
                }

                /* Update DRAW.position */
                store_desc(ctx, ssbo, draw_offset + WORD(4),
                           nir_load_deref(b, ctx->draw.pos_ptr),
                           2);

                /* Terminal draw: we need to break the loop by forcing
                 * HEADER.next to NULL.
                 */
                IF (ctx->draw.last)
                        store_desc(ctx, ssbo, WORD(6), nir_imm_ivec2(b, 0, 0), 2);
                ENDIF
        }

        /* Update DRAW.{instance_size,offset_start} */
        nir_ssa_def *instance_size =
                nir_bcsel(b,
                          nir_ilt(b, ctx->draw.instance_count, nir_imm_int(b, 2)),
                          nir_imm_int(b, 0),
                          ctx->draw.packed_instance_size);
        val = load_desc(ctx, ssbo, draw_offset + WORD(0), 1);
        val = nir_ior(b, val,
                      nir_ishl(b, instance_size, nir_imm_int(b, 16)));
        store_desc(ctx, ssbo, draw_offset + WORD(0),
                   nir_vec2(b, val, ctx->draw.offset_start), 2);
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
update_vertex_attrib_buf(struct indirect_draw_shader_ctx *ctx,
                         enum mali_attribute_type type,
                         nir_ssa_def *div1,
                         nir_ssa_def *div2)
{
        nir_builder *b = &ctx->b;
        unsigned type_mask = 0x3f;
        nir_ssa_def *w01 = load_desc(ctx, ATTRIB_BUFS, WORD(0), 2);
        nir_ssa_def *w0 = nir_channel(b, w01, 0);
        nir_ssa_def *w1 = nir_channel(b, w01, 1);

        if ((type | MALI_ATTRIBUTE_TYPE_1D) != type)
                w0 = nir_iand_imm(b, nir_channel(b, w01, 0), ~type_mask);

        w0 = nir_ior(b, w0, nir_imm_int(b, type));
        w1 = nir_ior(b, w1, nir_ishl(b, div1, nir_imm_int(b, 24)));

        store_desc(ctx, ATTRIB_BUFS, WORD(0), nir_vec2(b, w0, w1), 2);

        if (type != MALI_ATTRIBUTE_TYPE_1D_NPOT_DIVISOR)
                return;

        assert(div2);
        store_desc(ctx, ATTRIB_BUFS, WORD(5), div2, 1);
}

static void
update_vertex_attribs(struct indirect_draw_shader_ctx *ctx)
{
        nir_builder *b = &ctx->b;
        nir_variable *attrib_idx_var =
                nir_local_variable_create(b->impl, glsl_uint_type(), "attrib_idx");
        nir_deref_instr *attrib_idx_deref = nir_build_deref_var(b, attrib_idx_var);
        nir_store_deref(b, attrib_idx_deref, nir_imm_int(b, 0), 1);
        nir_ssa_def *single_instance =
                nir_ilt(b, ctx->draw.instance_count, nir_imm_int(b, 2));

        LOOP {
                nir_ssa_def *attrib_idx = nir_load_deref(b, attrib_idx_deref);
                IF (nir_ige(b, attrib_idx, ctx->attrib_count))
                        BREAK;
                ENDIF

                nir_ssa_def *attrib_buf_offset =
                         nir_imul_imm(b, attrib_idx,
                                      2 * MALI_ATTRIBUTE_BUFFER_LENGTH);
                nir_ssa_def *attrib_offset =
                         nir_imul_imm(b, attrib_idx, MALI_ATTRIBUTE_LENGTH);

                ctx->draw.desc_buf_offset = attrib_buf_offset;

                nir_ssa_def *r_e, *d;

                if (!ctx->is_bifrost) {
                        IF (nir_ieq_imm(b, attrib_idx, PAN_VERTEX_ID)) {
                                nir_ssa_def *r_p =
                                        nir_bcsel(b, single_instance,
                                                  nir_imm_int(b, 0x9f),
                                                  ctx->draw.packed_instance_size);

                                store_desc(ctx, ATTRIB_BUFS ,WORD(4),
                                           nir_ishl(b, r_p, nir_imm_int(b, 24)), 1);

                                nir_store_deref(b, attrib_idx_deref,
                                                nir_iadd_imm(b, attrib_idx, 1), 1);
                                CONTINUE;
                        } ENDIF

                        IF (nir_ieq_imm(b, attrib_idx, PAN_INSTANCE_ID)) {
                                split_div(b, ctx->draw.padded_instance_size,
                                          &r_e, &d);
                                nir_ssa_def *default_div =
                                        nir_ior(b, single_instance,
                                                nir_ilt(b,
                                                        ctx->draw.padded_instance_size,
                                                         nir_imm_int(b, 2)));
                                r_e = nir_bcsel(b, default_div,
                                                nir_imm_int(b, 0x3f), r_e);
                                d = nir_bcsel(b, default_div,
                                              nir_imm_int(b, (1u << 31) - 1), d);
                                store_desc(ctx, ATTRIB_BUFS, WORD(1),
                                           nir_vec2(b, nir_ishl(b, r_e, nir_imm_int(b, 24)), d),
                                           2);
                                nir_store_deref(b, attrib_idx_deref,
                                                nir_iadd_imm(b, attrib_idx, 1), 1);
                                CONTINUE;
                        } ENDIF
                }

                nir_ssa_def *div = load_desc(ctx, ATTRIB_BUFS, WORD(7), 1);

                div = nir_imul(b, div, ctx->draw.padded_instance_size);

                /* Set a zero stride for single instance draws */
                IF (nir_iand(b, single_instance, nir_ine(b, div, nir_imm_int(b, 0))))
                        store_desc(ctx, ATTRIB_BUFS, WORD(2), nir_imm_int(b, 0), 1);
                ENDIF

                nir_ssa_def *multi_instance =
                        nir_ige(b, ctx->draw.instance_count, nir_imm_int(b, 2));

                IF (nir_ine(b, div, nir_imm_int(b, 0))) {
                        IF (multi_instance) {
                                nir_ssa_def *div_pow2 =
                                        nir_ilt(b, nir_bit_count(b, div), nir_imm_int(b, 2));

                                IF (nir_ine(b, ctx->draw.offset_start, nir_imm_int(b, 0))) {
                                        nir_ssa_def *stride = load_desc(ctx, ATTRIB_BUFS, WORD(2), 1);
                                        ctx->draw.desc_buf_offset = attrib_offset;
                                        nir_ssa_def *offset = load_desc(ctx, ATTRIBS, WORD(1), 1);
                                        offset = nir_isub(b, offset,
                                                          nir_imul(b, stride,
                                                                   ctx->draw.offset_start));
                                        store_desc(ctx, ATTRIBS, WORD(1), offset, 1);
                                        ctx->draw.desc_buf_offset = attrib_buf_offset;
                                } ENDIF

                                IF (div_pow2) {
                                        nir_ssa_def *exp =
                                                nir_imax(b, nir_ufind_msb(b, div),
                                                         nir_imm_int(b, 0));
                                        update_vertex_attrib_buf(ctx,
                                                                 MALI_ATTRIBUTE_TYPE_1D_POT_DIVISOR,
                                                                 exp, NULL);
                                } ELSE {
                                        split_div(b, div, &r_e, &d);
                                        update_vertex_attrib_buf(ctx,
                                                                 MALI_ATTRIBUTE_TYPE_1D_NPOT_DIVISOR,
                                                                 r_e, d);
                                } ENDIF
                        } ENDIF
                } ELSE {
                        IF (multi_instance)
                                update_vertex_attrib_buf(ctx, MALI_ATTRIBUTE_TYPE_1D_MODULUS,
                                                         ctx->draw.packed_instance_size, NULL);
                        ENDIF
                } ENDIF

                nir_store_deref(b, attrib_idx_deref, nir_iadd_imm(b, attrib_idx, 1), 1);
        }

        ctx->draw.desc_buf_offset = NULL;
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

static void
update_varying_buf(struct indirect_draw_shader_ctx *ctx,
                   unsigned buf_idx, nir_ssa_def *vertex_count,
                   nir_deref_instr *ptr_deref)
{
        unsigned buf_offset = buf_idx * MALI_ATTRIBUTE_BUFFER_LENGTH;
        nir_builder *b = &ctx->b;

        nir_ssa_def *stride = load_desc(ctx, VARYING_BUFS, buf_offset + WORD(2), 1);
        nir_ssa_def *size = nir_imul(b, stride, vertex_count);
        nir_ssa_def *aligned_size = nir_iand_imm(b, nir_iadd_imm(b, size, 63), ~63);
        nir_ssa_def *var_mem_ptr = nir_load_deref(b, ctx->varying_mem_ptr);
        nir_ssa_def *w0 =
                nir_ior(b, nir_channel(b, var_mem_ptr, 0),
                        nir_imm_int(b, MALI_ATTRIBUTE_TYPE_1D));
        nir_ssa_def *w1 = nir_channel(b, var_mem_ptr, 1);
        store_desc(ctx, VARYING_BUFS, buf_offset + WORD(0),
                   nir_vec4(b, w0, w1, stride, size), 4);

        if (ptr_deref)
                nir_store_deref(b, ptr_deref, var_mem_ptr, 3);

        var_mem_ptr =
                nir_unpack_64_2x32(b,
                                   nir_iadd(b,
                                            nir_pack_64_2x32(b, var_mem_ptr),
                                            nir_u2u64(b, aligned_size)));
        nir_store_deref(b, ctx->varying_mem_ptr, var_mem_ptr, 3);
}

static void
update_varyings(struct indirect_draw_shader_ctx *ctx)
{
        nir_builder *b = &ctx->b;
        nir_ssa_def *vertex_count =
                nir_imul(b, ctx->draw.padded_instance_size,
                         ctx->draw.instance_count);

        update_varying_buf(ctx, PAN_VARY_GENERAL, vertex_count, NULL);
        update_varying_buf(ctx, PAN_VARY_POSITION, vertex_count,
                           ctx->draw.pos_ptr);

        if (ctx->draw.psiz_ptr) {
                update_varying_buf(ctx, PAN_VARY_PSIZ, vertex_count,
                                   ctx->draw.psiz_ptr);
        }
}

static void
get_invocation(struct indirect_draw_shader_ctx *ctx)
{
        nir_builder *b = &ctx->b;
        nir_ssa_def *one = nir_imm_int(b, 1);
        nir_ssa_def *max_vertex =
                nir_usub_sat(b, ctx->draw.instance_size, one);
        nir_ssa_def *max_instance =
                nir_usub_sat(b, ctx->draw.instance_count, one);
        nir_ssa_def *split =
                nir_bcsel(b, nir_ieq_imm(b, max_instance, 0),
                          nir_imm_int(b, 32),
                          nir_iadd_imm(b, nir_ufind_msb(b, max_vertex), 1));

        ctx->draw.invocation =
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
update_jobs(struct indirect_draw_shader_ctx *ctx)
{
        get_invocation(ctx);
        update_job(ctx, MALI_JOB_TYPE_VERTEX);
        update_job(ctx, MALI_JOB_TYPE_TILER);
}

static void
get_instance_size(struct indirect_draw_shader_ctx *ctx)
{
        nir_builder *b = &ctx->b;

        if (ctx->index_size == 0) {
                ctx->draw.base_vertex_offset = nir_imm_int(b, 0);
                ctx->draw.offset_start = ctx->draw.start;
                ctx->draw.instance_size = ctx->draw.count;
                return;
        }

        nir_variable *idx_var =
                nir_local_variable_create(b->impl, glsl_uint_type(), "idx");
        nir_deref_instr *idx_deref = nir_build_deref_var(b, idx_var);
        nir_store_deref(b, idx_deref, ctx->draw.start, 1);

        nir_variable *min_var =
                nir_local_variable_create(b->impl, glsl_uint_type(), "min");
        nir_deref_instr *min_deref = nir_build_deref_var(b, min_var);
        nir_store_deref(b, min_deref,
                        nir_imm_int(b, (1ULL << (ctx->index_size * 8)) - 1),
                        1);
        nir_variable *max_var =
                nir_local_variable_create(b->impl, glsl_uint_type(), "max");
        nir_deref_instr *max_deref = nir_build_deref_var(b, max_var);
        nir_store_deref(b, max_deref, nir_imm_int(b, 0), 1);

        nir_ssa_def *end = nir_iadd(b, ctx->draw.start, ctx->draw.count);

        LOOP {
                nir_ssa_def *idx = nir_load_deref(b, idx_deref);
                IF (nir_ige(b, idx, end))
                        BREAK;
                ENDIF

                nir_ssa_def *idx_offset = nir_imul_imm(b, idx, ctx->index_size);

                nir_ssa_def *val =
                        nir_load_ubo(b, 1, 32,
                                     nir_imm_int(b, INDEX_ARRAY),
                                     nir_iand(b, idx_offset, nir_imm_int(b, ~3)),
                                     .align_mul = 4,
                                     .align_offset = 0,
                                     .range_base = 0,
                                     .range = ~0);

                nir_ssa_def *shift =
                        nir_imul_imm(b, nir_iand_imm(b, idx_offset, 3), 8);

                val = nir_iand_imm(b, nir_ushr(b, val, shift),
                                   (1ull << (ctx->index_size * 8)) - 1);

                if (ctx->restart_index) {
                        IF (nir_ine(b, val, ctx->restart_index)) {
                                nir_store_deref(b, min_deref,
                                                nir_umin(b, nir_load_deref(b, min_deref), val),
                                                1);
                                nir_store_deref(b, max_deref,
                                                nir_umax(b, nir_load_deref(b, max_deref), val),
                                                1);
                        } ENDIF
                } else {
                        nir_store_deref(b, min_deref,
                                        nir_umin(b, nir_load_deref(b, min_deref), val),
                                        1);
                        nir_store_deref(b, max_deref,
                                        nir_umax(b, nir_load_deref(b, max_deref), val),
                                        1);
                }

                nir_store_deref(b, idx_deref, nir_iadd_imm(b, idx, 1), 1);
        }

        nir_ssa_def *min = nir_load_deref(b, min_deref);
        nir_ssa_def *max = nir_load_deref(b, max_deref);
        ctx->draw.base_vertex_offset = nir_ineg(b, min);
        ctx->draw.offset_start = nir_iadd(b, min, ctx->draw.index_bias);
        ctx->draw.instance_size = nir_iadd_imm(b, nir_usub_sat(b, max, min), 1);
}

static void
draw(struct indirect_draw_shader_ctx *ctx)
{
        nir_builder *b = &ctx->b;

        nir_ssa_def *draw_idx = load_desc(ctx, DRAW_IDX, WORD(0), 1);

        nir_ssa_def *next_draw_idx = nir_iadd_imm(b, draw_idx, 1);
        ctx->draw.last = nir_ige(b, next_draw_idx, ctx->draw_count);
        store_desc(ctx, DRAW_IDX, WORD(0), next_draw_idx, 1);

        if (ctx->has_psiz) {
                nir_variable *psiz_ptr_var =
                        ctx->has_psiz ?
                        nir_local_variable_create(b->impl,
                                                  glsl_vector_type(GLSL_TYPE_UINT, 2),
                                                  "psiz_ptr") :
                        NULL;
                ctx->draw.psiz_ptr = nir_build_deref_var(b, psiz_ptr_var);
                nir_store_deref(b, ctx->draw.psiz_ptr, nir_imm_ivec2(b, 0, 0), 3);
        }

        nir_variable *pos_ptr_var =
                nir_local_variable_create(b->impl,
                                          glsl_vector_type(GLSL_TYPE_UINT, 2),
                                          "pos_ptr");
        ctx->draw.pos_ptr = nir_build_deref_var(b, pos_ptr_var);
        nir_store_deref(b, ctx->draw.pos_ptr, nir_imm_ivec2(b, 0, 0), 3);

        nir_ssa_def *draw_offset =
                nir_imul(b, draw_idx, ctx->draw_buf_stride);

        ctx->draw.count =
                nir_load_ubo(b, 1, 32,
                             nir_imm_int(b, INDIRECT_DRAW_ARRAY),
                             draw_offset,
                             .align_mul = 4,
                             .align_offset = 0,
                             .range_base = 0,
                             .range = ~0);
        ctx->draw.instance_count =
                nir_load_ubo(b, 1, 32,
                             nir_imm_int(b, INDIRECT_DRAW_ARRAY),
                             nir_iadd_imm(b, draw_offset, 4),
                             .align_mul = 4,
                             .align_offset = 0,
                             .range_base = 0,
                             .range = ~0);
        ctx->draw.start =
                nir_load_ubo(b, 1, 32,
                             nir_imm_int(b, INDIRECT_DRAW_ARRAY),
                             nir_iadd_imm(b, draw_offset, 8),
                             .align_mul = 4,
                             .align_offset = 0,
                             .range_base = 0,
                             .range = ~0);
        ctx->draw.index_bias =
                ctx->index_size != 0 ?
                nir_load_ubo(b, 1, 32,
                             nir_imm_int(b, INDIRECT_DRAW_ARRAY),
                             nir_iadd_imm(b, draw_offset, 12),
                             .align_mul = 4,
                             .align_offset = 0,
                             .range_base = 0,
                             .range = ~0) :
                NULL;

        /* start_instance is ignored since we don't support gl_BaseInstance yet */

        get_instance_size(ctx);

        ctx->draw.padded_instance_size =
                get_padded_count(b, ctx->draw.instance_size,
                                 &ctx->draw.packed_instance_size);

        update_varyings(ctx);
        update_jobs(ctx);
        update_vertex_attribs(ctx);
}

static unsigned
get_shader_id(unsigned index_size, bool has_psiz,
              bool indirect_draw_count,
              bool primitive_restart,
              bool update_prim_size)
{
        unsigned id = index_size ? util_logbase2(index_size) + 1 : 0;

        if (has_psiz)
                id |= PAN_INDIRECT_DRAW_HAS_PSIZ;

        if (indirect_draw_count)
                id |= PAN_INDIRECT_DRAW_INDIRECT_DRAW_COUNT;

        if (primitive_restart)
                id |= PAN_INDIRECT_DRAW_PRIMITIVE_RESTART;

        if (update_prim_size)
                id |= PAN_INDIRECT_DRAW_UPDATE_PRIM_SIZE;

        return id;
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
        bool is_bifrost = dev->quirks & IS_BIFROST;

        pan_pack(out, RENDERER_STATE, state) {
                if (is_bifrost)
                        prepare_bifrost_shader_state(s, prog, shader_bo, &state);
                else
                        prepare_midgard_shader_state(s, prog, shader_bo, &state);
        }
}

static int
uniform_type_size(const struct glsl_type *type, bool bindless)
{
   return glsl_count_vec4_slots(type, false, bindless);
}

static void
create_indirect_draw_shader(struct panfrost_device *dev,
                            unsigned index_size, bool has_psiz,
                            bool indirect_draw_count,
                            bool primitive_restart,
                            bool update_prim_size)
{
        /* Build the shader */

        struct indirect_draw_shader_ctx ctx;
        init_shader_ctx(&ctx, dev, index_size, has_psiz,
                        indirect_draw_count, primitive_restart,
                        update_prim_size);

        nir_builder *b = &ctx.b;

        draw(&ctx);

        NIR_PASS_V(b->shader, nir_lower_io, nir_var_uniform,
                   uniform_type_size, (nir_lower_io_options)0);

        if (ctx.is_bifrost)
                NIR_PASS_V(b->shader, nir_lower_uniforms_to_ubo, 16);

        panfrost_program *program;

        struct panfrost_compile_inputs inputs = { .gpu_id = dev->gpu_id };

        if (ctx.is_bifrost)
                program = bifrost_compile_shader_nir(NULL, b->shader, &inputs);
        else
                program = midgard_compile_shader_nir(NULL, b->shader, &inputs);

        struct panfrost_bo *bo =
                panfrost_bo_create(dev, program->compiled.size, PAN_BO_EXECUTE);

        memcpy(bo->ptr.cpu, program->compiled.data, program->compiled.size);

        unsigned shader_id = get_shader_id(index_size, has_psiz,
                                           indirect_draw_count,
                                           primitive_restart,
                                           update_prim_size);
        void *state = dev->indirect_draw_shaders.states->ptr.cpu +
                      (shader_id * MALI_RENDERER_STATE_LENGTH);

        prepare_shader_state(dev, b->shader, program, bo, state);

        struct pan_indirect_draw_shader *info =
                &dev->indirect_draw_shaders.shaders[shader_id];

        info->bo = bo;
        info->stack_size = program->tls_size;
        assert(program->tls_size == 0);
        info->shared_size = b->shader->info.cs.shared_size;
        assert(b->shader->info.cs.shared_size == 0);
        info->sysval_count = program->sysval_count;
        memcpy(info->sysvals, program->sysvals,
               sizeof(program->sysvals[0]) * program->sysval_count);

        ralloc_free(b->shader);
        ralloc_free(program);
}

static mali_ptr
get_renderer_state(struct panfrost_device *dev,
                   unsigned index_size, bool has_psiz,
                   bool indirect_draw_count,
                   bool primitive_restart,
                   bool update_prim_size)
{
        unsigned shader_id = get_shader_id(index_size, has_psiz,
                                           indirect_draw_count,
                                           primitive_restart,
                                           update_prim_size);
        struct pan_indirect_draw_shader *info =
                &dev->indirect_draw_shaders.shaders[shader_id];

        if (!info->bo) {
                create_indirect_draw_shader(dev, index_size, has_psiz,
                                            indirect_draw_count,
                                            primitive_restart,
                                            update_prim_size);
                assert(info->bo);
        }

        mali_ptr state = dev->indirect_draw_shaders.states->ptr.gpu +
                         (shader_id * MALI_RENDERER_STATE_LENGTH);

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
               const struct pan_indirect_draw_descs_info *descs_info)
{
        struct panfrost_device *dev = pool->dev;
        bool is_bifrost = dev->quirks & IS_BIFROST;
        bool indirect_draw_count = draw_info->draw_count_buf != 0;
        unsigned shader_id = get_shader_id(draw_info->index_size,
                                           descs_info->has_psiz,
                                           indirect_draw_count,
                                           draw_info->primitive_restart,
                                           descs_info->update_prim_size);
        struct pan_indirect_draw_shader *s =
                &dev->indirect_draw_shaders.shaders[shader_id];

        unsigned ubo0_sz = (s->sysval_count + NUM_UNIFORMS) * 16;
        struct panfrost_ptr ubo0_ptr =
                panfrost_pool_alloc_aligned(pool,
                                            ubo0_sz,
                                            64);

        struct pan_sysval_uniform *sysval = ubo0_ptr.cpu;

        /* Set SSBO sysval */
        assert(s->sysval_count == NUM_SSBOS);
        for (unsigned i = 0; i < s->sysval_count; i++) {
                assert(PAN_SYSVAL_TYPE(s->sysvals[i]) == PAN_SYSVAL_SSBO);
                switch (PAN_SYSVAL_ID(s->sysvals[i])) {
                case VERTEX_JOB:
                        sysval[i].du[0] = descs_info->vertex_job;
                        sysval[i].u[2] = MALI_COMPUTE_JOB_LENGTH;
                        break;
                case TILER_JOB:
                        sysval[i].du[0] = descs_info->tiler_job;
                        sysval[i].u[2] = is_bifrost ?
                                         MALI_BIFROST_TILER_JOB_LENGTH :
                                         MALI_MIDGARD_TILER_JOB_LENGTH;
                        break;
                case DRAW_IDX:
                        sysval[i].du[0] = descs_info->draw_idx;
                        sysval[i].u[2] = sizeof(uint32_t);
                        break;
                case ATTRIB_BUFS:
                        sysval[i].du[0] = descs_info->attrib_bufs;
                        sysval[i].u[2] = descs_info->attrib_count * 2 *
                                         MALI_ATTRIBUTE_BUFFER_LENGTH;
                        break;
                case ATTRIBS:
                        sysval[i].du[0] = descs_info->attribs;
                        sysval[i].u[2] = descs_info->attrib_count *
                                         MALI_ATTRIBUTE_LENGTH;
                        break;
                case VARYING_BUFS:
                        sysval[i].du[0] = descs_info->varying_bufs;
                        sysval[i].u[2] = descs_info->varying_buf_count *
                                         MALI_ATTRIBUTE_BUFFER_LENGTH;
                        break;
                default:
                        unreachable("Invalid SSBO ID\n");
                }
        }

        /* Draw info uniform */
        unsigned draw_buf_stride = (draw_info->index_size ? 5 : 4) * 4;
        if (draw_info->draw_buf_stride) {
                assert(draw_info->draw_buf_stride > draw_buf_stride);
                draw_buf_stride = draw_info->draw_buf_stride;
        }

        /* Draw info uniform */
        sysval[s->sysval_count + DRAW_INFO_UNIFORM].u[0] = draw_info->draw_count;
        sysval[s->sysval_count + DRAW_INFO_UNIFORM].u[1] = descs_info->attrib_count;
        sysval[s->sysval_count + DRAW_INFO_UNIFORM].u[2] = draw_buf_stride;
        sysval[s->sysval_count + DRAW_INFO_UNIFORM].u[3] = draw_info->restart_index;

        /* Varying memory uniform */
        sysval[s->sysval_count + VARYING_MEM_UNIFORM].du[0] = descs_info->varying_mem;

        unsigned ubo_mask = 1 << INDIRECT_DRAW_ARRAY;
        if (draw_info->index_size != 0)
                ubo_mask |= 1 << INDEX_ARRAY;
        if (indirect_draw_count)
                ubo_mask |= 1 << INDIRECT_DRAW_COUNT_ARRAY;

        unsigned ubo_count = util_bitcount((ubo_mask << 1) | 1);

        struct panfrost_ptr ubos_ptr =
                panfrost_pool_alloc_aligned(pool,
                                            MALI_UNIFORM_BUFFER_LENGTH *
                                            ubo_count,
                                            MALI_UNIFORM_BUFFER_LENGTH);
        struct mali_uniform_buffer_packed *ubo = ubos_ptr.cpu;

        pan_pack(&ubo[0], UNIFORM_BUFFER, cfg) {
                cfg.entries = s->sysval_count + NUM_UNIFORMS;
                cfg.pointer = ubo0_ptr.gpu;
        }

        pan_pack(&ubo[INDIRECT_DRAW_ARRAY + 1], UNIFORM_BUFFER, cfg) {
                unsigned draw_buf_size = MIN2(draw_buf_stride *
                                              draw_info->draw_count,
                                              draw_info->draw_buf_size);

                cfg.entries = DIV_ROUND_UP(draw_buf_size, 16);
                cfg.pointer = draw_info->draw_buf;
        }

        if (draw_info->index_size != 0) {
                assert(draw_info->index_buf != 0 && draw_info->index_buf_size > 0);
                pan_pack(&ubo[INDEX_ARRAY + 1], UNIFORM_BUFFER, cfg) {
                        cfg.entries = DIV_ROUND_UP(draw_info->index_buf_size, 16);
                        cfg.pointer = draw_info->index_buf;
                }
        } else {
                memset(&ubo[INDEX_ARRAY + 1], 0, sizeof(*ubo));
        }

        if (indirect_draw_count) {
                assert(draw_info->draw_count_buf != 0 &&
                       draw_info->draw_count_buf_size > 0);
                pan_pack(&ubo[INDIRECT_DRAW_COUNT_ARRAY + 1], UNIFORM_BUFFER, cfg) {
                        cfg.entries = DIV_ROUND_UP(draw_info->draw_count_buf_size, 16);
                        cfg.pointer = draw_info->draw_count_buf;
                }
        } else {
                memset(&ubo[INDIRECT_DRAW_COUNT_ARRAY + 1], 0, sizeof(*ubo));
        }

        return ubos_ptr.gpu;
}

struct panfrost_ptr
panfrost_emit_indirect_draw(struct pan_pool *pool,
                            struct pan_scoreboard *scoreboard,
                            const struct pan_indirect_draw_info *draw_info,
                            const struct pan_indirect_draw_descs_info *descs_info)
{
        struct panfrost_device *dev = pool->dev;
        bool is_bifrost = dev->quirks & IS_BIFROST;
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

        bool indirect_draw_count = draw_info->draw_count_buf != 0;

        pan_section_pack(job, COMPUTE_JOB, DRAW, cfg) {
                cfg.draw_descriptor_is_64b = true;
                if (is_bifrost)
                        cfg.texture_descriptor_is_64b = true;
                cfg.state = get_renderer_state(dev, draw_info->index_size,
                                               descs_info->has_psiz,
                                               indirect_draw_count,
                                               draw_info->primitive_restart,
                                               descs_info->update_prim_size);
                cfg.thread_storage = get_tls(pool);
                cfg.uniform_buffers =
                        get_const_bufs(pool, draw_info, descs_info);
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
