/*
 * Copyright (C) 2018 Alyssa Rosenzweig
 * Copyright (C) 2020 Collabora Ltd.
 * Copyright © 2017 Intel Corporation
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
 */

#include "util/macros.h"
#include "util/u_prim.h"
#include "util/u_vbuf.h"
#include "util/u_helpers.h"

#include "panfrost-quirks.h"

#include "pan_pool.h"
#include "pan_bo.h"
#include "pan_cmdstream.h"
#include "pan_context.h"
#include "pan_job.h"
#include "pan_shader.h"
#include "pan_texture.h"

#if (PAN_ARCH == 4)
#define panx(name) pan_ ## name ## _v4
#elif (PAN_ARCH == 5)
#define panx(name) pan_ ## name ## _v5
#elif (PAN_ARCH == 6)
#define panx(name) pan_ ## name ## _v6
#elif (PAN_ARCH == 7)
#define panx(name) pan_ ## name ## _v7
#else
#error Invalid PAN_ARCH
#endif

static bool
panfrost_fs_required(
      struct panfrost_shader_state *fs,
      struct panfrost_blend_state *blend,
      struct pipe_framebuffer_state *state)
{
   /* If we generally have side effects */
   if (fs->info.fs.sidefx)
      return true;

   /* If colour is written we need to execute */
   for (unsigned i = 0; i < state->nr_cbufs; ++i) {
      if (state->cbufs[i] && !blend->info[i].no_colour)
         return true;
   }

   /* If depth is written and not implied we need to execute.
    * TODO: Predicate on Z/S writes being enabled */
   return (fs->info.fs.writes_depth || fs->info.fs.writes_stencil);
}

#if PAN_ARCH >= 5
static void
panfrost_emit_blend(struct panfrost_batch *batch, void *rts,
             mali_ptr *blend_shaders)
{
   unsigned rt_count = batch->key.nr_cbufs;
   struct panfrost_context *ctx = batch->ctx;
   const struct panfrost_blend_state *so = ctx->blend;

#if PAN_ARCH >= 6
   const struct panfrost_device *dev = pan_device(ctx->base.screen);
   struct panfrost_shader_state *fs = panfrost_get_shader_state(ctx, PIPE_SHADER_FRAGMENT);

   /* Always have at least one render target for depth-only passes */
   for (unsigned i = 0; i < MAX2(rt_count, 1); ++i) {
      /* Disable blending for unbacked render targets */
      if (rt_count == 0 || !batch->key.cbufs[i]) {
         pan_pack(rts + i * MALI_BLEND_LENGTH, BLEND, cfg) {
            cfg.enable = false;
            cfg.bifrost.internal.mode = MALI_BIFROST_BLEND_MODE_OFF;
         }

         continue;
      }

      struct pan_blend_info info = so->info[i];
      enum pipe_format format = batch->key.cbufs[i]->format;
      const struct util_format_description *format_desc;
      unsigned chan_size = 0;

      format_desc = util_format_description(format);

      for (unsigned i = 0; i < format_desc->nr_channels; i++)
         chan_size = MAX2(format_desc->channel[0].size, chan_size);

      /* Fixed point constant */
      float constant_f = pan_blend_get_constant(
            info.constant_mask,
            ctx->blend_color.color);

      u16 constant = constant_f * ((1 << chan_size) - 1);
      constant <<= 16 - chan_size;

      struct mali_blend_packed *packed = rts + (i * MALI_BLEND_LENGTH);

      /* Word 0: Flags and constant */
      pan_pack(packed, BLEND, cfg) {
         if (info.no_colour) {
            cfg.enable = false;
         } else {
            cfg.srgb = util_format_is_srgb(batch->key.cbufs[i]->format);
            cfg.load_destination = info.load_dest;

            cfg.round_to_fb_precision = !ctx->blend->base.dither;
            cfg.alpha_to_one = ctx->blend->base.alpha_to_one;
         }

         cfg.bifrost.constant = constant;
      }

      if (!blend_shaders[i]) {
         /* Word 1: Blend Equation */
         STATIC_ASSERT(MALI_BLEND_EQUATION_LENGTH == 4);
         packed->opaque[1] = so->equation[i].opaque[0];
      }

      /* Words 2 and 3: Internal blend */
      if (blend_shaders[i]) {
         /* The blend shader's address needs to be at
          * the same top 32 bit as the fragment shader.
          * TODO: Ensure that's always the case.
          */
         assert(!fs->bin.bo ||
               (blend_shaders[i] & (0xffffffffull << 32)) ==
               (fs->bin.gpu & (0xffffffffull << 32)));

         unsigned ret_offset = fs->info.bifrost.blend[i].return_offset;
         assert(!(ret_offset & 0x7));

         pan_pack(&packed->opaque[2], BIFROST_INTERNAL_BLEND, cfg) {
            cfg.mode = MALI_BIFROST_BLEND_MODE_SHADER;
            cfg.shader.pc = (u32) blend_shaders[i];
            cfg.shader.return_value = ret_offset ?
               fs->bin.gpu + ret_offset : 0;
         }
      } else {
         pan_pack(&packed->opaque[2], BIFROST_INTERNAL_BLEND, cfg) {
            cfg.mode = info.opaque ?
               MALI_BIFROST_BLEND_MODE_OPAQUE :
               MALI_BIFROST_BLEND_MODE_FIXED_FUNCTION;

            /* If we want the conversion to work properly,
             * num_comps must be set to 4
             */
            cfg.fixed_function.num_comps = 4;
            cfg.fixed_function.conversion.memory_format =
               panfrost_format_to_bifrost_blend(dev, format);
            cfg.fixed_function.conversion.register_format =
               fs->info.bifrost.blend[i].format;
            cfg.fixed_function.rt = i;
         }
      }
   }
#else
   /* Always have at least one render target for depth-only passes */
   for (unsigned i = 0; i < MAX2(rt_count, 1); ++i) {
      struct mali_blend_packed *packed = rts + (i * MALI_BLEND_LENGTH);

      /* Disable blending for unbacked render targets */
      if (rt_count == 0 || !batch->key.cbufs[i]) {
         pan_pack(packed, BLEND, cfg) {
            cfg.midgard.equation.color_mask = 0xf;
            cfg.midgard.equation.rgb.a = MALI_BLEND_OPERAND_A_SRC;
            cfg.midgard.equation.rgb.b = MALI_BLEND_OPERAND_B_SRC;
            cfg.midgard.equation.rgb.c = MALI_BLEND_OPERAND_C_ZERO;
            cfg.midgard.equation.alpha.a = MALI_BLEND_OPERAND_A_SRC;
            cfg.midgard.equation.alpha.b = MALI_BLEND_OPERAND_B_SRC;
            cfg.midgard.equation.alpha.c = MALI_BLEND_OPERAND_C_ZERO;
         }

         continue;
      }

      pan_pack(packed, BLEND, cfg) {
         struct pan_blend_info info = so->info[i];

         if (info.no_colour) {
            cfg.enable = false;
            continue;
         }

         cfg.srgb = util_format_is_srgb(batch->key.cbufs[i]->format);
         cfg.load_destination = info.load_dest;
         cfg.round_to_fb_precision = !ctx->blend->base.dither;
         cfg.alpha_to_one = ctx->blend->base.alpha_to_one;
         cfg.midgard.blend_shader = (blend_shaders[i] != 0);
         if (blend_shaders[i]) {
            cfg.midgard.shader_pc = blend_shaders[i];
         } else {
            cfg.midgard.constant = pan_blend_get_constant(
                  info.constant_mask,
                  ctx->blend_color.color);
         }
      }

      if (!blend_shaders[i]) {
         /* Word 2: Blend Equation */
         STATIC_ASSERT(MALI_BLEND_EQUATION_LENGTH == 4);
         packed->opaque[2] = so->equation[i].opaque[0];
      }
   }
#endif
}
#endif

/* Construct a partial RSD corresponding to no executed fragment shader, and
 * merge with the existing partial RSD. This depends only on the architecture,
 * so packing separately allows the packs to be constant folded away. */

static void
pan_merge_empty_fs(struct mali_renderer_state_packed *rsd)
{
   struct mali_renderer_state_packed empty_rsd;

   pan_pack(&empty_rsd, RENDERER_STATE, cfg) {
#if PAN_ARCH >= 6
      cfg.properties.bifrost.shader_modifies_coverage = true;
      cfg.properties.bifrost.allow_forward_pixel_to_kill = true;
      cfg.properties.bifrost.allow_forward_pixel_to_be_killed = true;
      cfg.properties.bifrost.zs_update_operation = MALI_PIXEL_KILL_STRONG_EARLY;
#else
      cfg.shader.shader = 0x1;
      cfg.properties.midgard.work_register_count = 1;
      cfg.properties.depth_source = MALI_DEPTH_SOURCE_FIXED_FUNCTION;
      cfg.properties.midgard.force_early_z = true;
#endif
   }

   pan_merge((*rsd), empty_rsd, RENDERER_STATE);
}

#if PAN_ARCH == 5
/* Get the last blend shader, for an erratum workaround */

static mali_ptr
panfrost_last_nonnull(mali_ptr *ptrs, unsigned count)
{
   for (signed i = ((signed) count - 1); i >= 0; --i) {
      if (ptrs[i])
         return ptrs[i];
   }

   return 0;
}
#endif

static void
panfrost_prepare_fs_state(struct panfrost_context *ctx,
           mali_ptr *blend_shaders,
           struct mali_renderer_state_packed *rsd)
{
   struct pipe_rasterizer_state *rast = &ctx->rasterizer->base;
   const struct panfrost_zsa_state *zsa = ctx->depth_stencil;
   struct panfrost_shader_state *fs = panfrost_get_shader_state(ctx, PIPE_SHADER_FRAGMENT);
   UNUSED struct panfrost_blend_state *so = ctx->blend;
   bool alpha_to_coverage = ctx->blend->base.alpha_to_coverage;
   bool msaa = rast->multisample;

   pan_pack(rsd, RENDERER_STATE, cfg) {
#if PAN_ARCH >= 6
      if (panfrost_fs_required(fs, so, &ctx->pipe_framebuffer)) {
         /* Track if any colour buffer is reused across draws, either
          * from reading it directly, or from failing to write it */
         unsigned rt_mask = ctx->fb_rt_mask;
         bool blend_reads_dest = (so->load_dest_mask & rt_mask);

         cfg.properties.bifrost.allow_forward_pixel_to_kill =
            fs->info.fs.can_fpk &&
            !(rt_mask & ~fs->info.outputs_written) &&
            !alpha_to_coverage &&
            !blend_reads_dest;
      }
#else
      unsigned rt_count = ctx->pipe_framebuffer.nr_cbufs;

      if (panfrost_fs_required(fs, ctx->blend, &ctx->pipe_framebuffer)) {
         cfg.properties.midgard.force_early_z =
            fs->info.fs.can_early_z && !alpha_to_coverage &&
            ((enum mali_func) zsa->base.alpha_func == MALI_FUNC_ALWAYS);

         bool has_blend_shader = false;

         for (unsigned c = 0; c < rt_count; ++c)
            has_blend_shader |= (blend_shaders[c] != 0);

         /* TODO: Reduce this limit? */
         if (has_blend_shader)
            cfg.properties.midgard.work_register_count = MAX2(fs->info.work_reg_count, 8);
         else
            cfg.properties.midgard.work_register_count = fs->info.work_reg_count;

         /* Workaround a hardware errata where early-z cannot be enabled
          * when discarding even when the depth buffer is read-only, by
          * lying to the hardware about the discard and setting the
          * reads tilebuffer? flag to compensate */
         cfg.properties.midgard.shader_reads_tilebuffer =
            !zsa->enabled && fs->info.fs.can_discard;
         cfg.properties.midgard.shader_contains_discard =
            zsa->enabled && fs->info.fs.can_discard;
      }

#if PAN_ARCH == 4
      if (rt_count > 0) {
         cfg.multisample_misc.sfbd_load_destination = so->info[0].load_dest;
         cfg.multisample_misc.sfbd_blend_shader = (blend_shaders[0] != 0);
         cfg.stencil_mask_misc.sfbd_write_enable = !so->info[0].no_colour;
         cfg.stencil_mask_misc.sfbd_srgb = util_format_is_srgb(ctx->pipe_framebuffer.cbufs[0]->format);
         cfg.stencil_mask_misc.sfbd_dither_disable = !so->base.dither;
         cfg.stencil_mask_misc.sfbd_alpha_to_one = so->base.alpha_to_one;

         if (blend_shaders[0]) {
            cfg.sfbd_blend_shader = blend_shaders[0];
         } else {
            cfg.sfbd_blend_constant = pan_blend_get_constant(
                  so->info[0].constant_mask,
                  ctx->blend_color.color);
         }
      } else {
         /* If there is no colour buffer, leaving fields default is
          * fine, except for blending which is nonnullable */
         cfg.sfbd_blend_equation.color_mask = 0xf;
         cfg.sfbd_blend_equation.rgb.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.sfbd_blend_equation.rgb.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.sfbd_blend_equation.rgb.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.sfbd_blend_equation.alpha.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.sfbd_blend_equation.alpha.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.sfbd_blend_equation.alpha.c = MALI_BLEND_OPERAND_C_ZERO;
      }
#elif PAN_ARCH == 5
      /* Workaround on v5 */
      cfg.sfbd_blend_shader = panfrost_last_nonnull(blend_shaders, rt_count);
#endif
#endif

      cfg.multisample_misc.sample_mask = msaa ? ctx->sample_mask : 0xFFFF;

      cfg.multisample_misc.evaluate_per_sample =
         msaa && (ctx->min_samples > 1);

      cfg.stencil_mask_misc.alpha_to_coverage = alpha_to_coverage;
      cfg.depth_units = rast->offset_units * 2.0f;
      cfg.depth_factor = rast->offset_scale;

      bool back_enab = zsa->base.stencil[1].enabled;
      cfg.stencil_front.reference_value = ctx->stencil_ref.ref_value[0];
      cfg.stencil_back.reference_value = ctx->stencil_ref.ref_value[back_enab ? 1 : 0];

#if PAN_ARCH <= 5
      /* v6+ removes alpha testing */
      cfg.alpha_reference = zsa->base.alpha_ref_value;
#endif
   }
}

static void
panfrost_emit_frag_shader(struct panfrost_context *ctx,
           struct mali_renderer_state_packed *fragmeta,
           mali_ptr *blend_shaders)
{
   const struct panfrost_zsa_state *zsa = ctx->depth_stencil;
   const struct panfrost_rasterizer *rast = ctx->rasterizer;
   struct panfrost_shader_state *fs =
      panfrost_get_shader_state(ctx, PIPE_SHADER_FRAGMENT);

   /* We need to merge several several partial renderer state descriptors,
    * so stage to temporary storage rather than reading back write-combine
    * memory, which will trash performance. */
   struct mali_renderer_state_packed rsd;
   panfrost_prepare_fs_state(ctx, blend_shaders, &rsd);

#if PAN_ARCH == 4
   if (ctx->pipe_framebuffer.nr_cbufs > 0 && !blend_shaders[0]) {
      /* Word 14: SFBD Blend Equation */
      STATIC_ASSERT(MALI_BLEND_EQUATION_LENGTH == 4);
      rsd.opaque[14] = ctx->blend->equation[0].opaque[0];
   }
#endif

   /* Merge with CSO state and upload */
   if (panfrost_fs_required(fs, ctx->blend, &ctx->pipe_framebuffer))
      pan_merge(rsd, fs->partial_rsd, RENDERER_STATE);
   else
      pan_merge_empty_fs(&rsd);

   /* Word 8, 9 Misc state */
   rsd.opaque[8] |= zsa->rsd_depth.opaque[0]
             | rast->multisample.opaque[0];

   rsd.opaque[9] |= zsa->rsd_stencil.opaque[0]
             | rast->stencil_misc.opaque[0];

   /* Word 10, 11 Stencil Front and Back */
   rsd.opaque[10] |= zsa->stencil_front.opaque[0];
   rsd.opaque[11] |= zsa->stencil_back.opaque[0];

   memcpy(fragmeta, &rsd, sizeof(rsd));
}

static mali_ptr
pan_emit_frag_shader_meta(struct panfrost_batch *batch)
{
   struct panfrost_context *ctx = batch->ctx;
   struct panfrost_shader_state *ss = panfrost_get_shader_state(ctx, PIPE_SHADER_FRAGMENT);

   /* Add the shader BO to the batch. */
   panfrost_batch_add_bo(batch, ss->bin.bo,
               PAN_BO_ACCESS_SHARED |
               PAN_BO_ACCESS_READ |
               PAN_BO_ACCESS_FRAGMENT);

   struct panfrost_ptr xfer;

#if PAN_ARCH == 4
   xfer = panfrost_pool_alloc_desc(&batch->pool, RENDERER_STATE);
#else
   unsigned rt_count = MAX2(ctx->pipe_framebuffer.nr_cbufs, 1);

   xfer = panfrost_pool_alloc_desc_aggregate(&batch->pool,
         PAN_DESC(RENDERER_STATE),
         PAN_DESC_ARRAY(rt_count, BLEND));
#endif

   mali_ptr blend_shaders[PIPE_MAX_COLOR_BUFS];
   unsigned shader_offset = 0;
   struct panfrost_bo *shader_bo = NULL;

   for (unsigned c = 0; c < ctx->pipe_framebuffer.nr_cbufs; ++c) {
      if (ctx->pipe_framebuffer.cbufs[c]) {
         blend_shaders[c] = panfrost_get_blend(batch,
               c, &shader_bo, &shader_offset);
      }
   }

   panfrost_emit_frag_shader(ctx, (struct mali_renderer_state_packed *) xfer.cpu, blend_shaders);

#if PAN_ARCH == 4
   batch->draws |= PIPE_CLEAR_COLOR0;
   batch->resolve |= PIPE_CLEAR_COLOR0;
#else
   panfrost_emit_blend(batch, xfer.cpu + MALI_RENDERER_STATE_LENGTH, blend_shaders);

   for (unsigned i = 0; i < batch->key.nr_cbufs; ++i) {
      if (!ctx->blend->info[i].no_colour && batch->key.cbufs[i]) {
         batch->draws |= (PIPE_CLEAR_COLOR0 << i);
         batch->resolve |= (PIPE_CLEAR_COLOR0 << i);
      }
   }
#endif

   if (ctx->depth_stencil->base.depth_enabled)
      batch->read |= PIPE_CLEAR_DEPTH;

   if (ctx->depth_stencil->base.stencil[0].enabled)
      batch->read |= PIPE_CLEAR_STENCIL;

   return xfer.gpu;
}

static enum mali_index_type
panfrost_translate_index_size(unsigned size)
{
   switch (size) {
   case 1: return MALI_INDEX_TYPE_UINT8;
   case 2: return MALI_INDEX_TYPE_UINT16;
   case 4: return MALI_INDEX_TYPE_UINT32;
   default: unreachable("Invalid index size");
   }
}

#define DEFINE_CASE(c) case PIPE_PRIM_##c: return MALI_DRAW_MODE_##c;

static int
pan_draw_mode(enum pipe_prim_type mode)
{
   switch (mode) {
      DEFINE_CASE(POINTS);
      DEFINE_CASE(LINES);
      DEFINE_CASE(LINE_LOOP);
      DEFINE_CASE(LINE_STRIP);
      DEFINE_CASE(TRIANGLES);
      DEFINE_CASE(TRIANGLE_STRIP);
      DEFINE_CASE(TRIANGLE_FAN);
      DEFINE_CASE(QUADS);
      DEFINE_CASE(QUAD_STRIP);
      DEFINE_CASE(POLYGON);

   default:
      unreachable("Invalid draw mode");
   }
}

#undef DEFINE_CASE

static bool
panfrost_is_implicit_prim_restart(const struct pipe_draw_info *info)
{
   unsigned implicit_index = (1 << (info->index_size * 8)) - 1;
   bool implicit = info->restart_index == implicit_index;
   return info->primitive_restart && implicit;
}

void
panx(draw_emit_tiler)(struct panfrost_batch *batch,
          const struct pipe_draw_info *info,
          const struct pipe_draw_start_count_bias *draw,
          void *invocation_template,
          mali_ptr shared_mem, mali_ptr indices,
          mali_ptr fs_vary, mali_ptr varyings,
          mali_ptr pos, mali_ptr psiz, void *job)
{
   struct panfrost_context *ctx = batch->ctx;
   struct pipe_rasterizer_state *rast = &ctx->rasterizer->base;

#if PAN_ARCH >= 6
   void *section = pan_section_ptr(job, BIFROST_TILER_JOB, INVOCATION);
#else
   void *section = pan_section_ptr(job, MIDGARD_TILER_JOB, INVOCATION);
#endif
   memcpy(section, invocation_template, MALI_INVOCATION_LENGTH);

#if PAN_ARCH >= 6
   section = pan_section_ptr(job, BIFROST_TILER_JOB, PRIMITIVE);
#else
   section = pan_section_ptr(job, MIDGARD_TILER_JOB, PRIMITIVE);
#endif

   pan_pack(section, PRIMITIVE, cfg) {
      cfg.draw_mode = pan_draw_mode(info->mode);
      if (panfrost_writes_point_size(ctx))
         cfg.point_size_array_format = MALI_POINT_SIZE_ARRAY_FORMAT_FP16;

      /* For line primitives, PRIMITIVE.first_provoking_vertex must
       * be set to true and the provoking vertex is selected with
       * DRAW.flat_shading_vertex.
       */
      if (info->mode == PIPE_PRIM_LINES ||
          info->mode == PIPE_PRIM_LINE_LOOP ||
          info->mode == PIPE_PRIM_LINE_STRIP)
         cfg.first_provoking_vertex = true;
      else
         cfg.first_provoking_vertex = rast->flatshade_first;

      if (panfrost_is_implicit_prim_restart(info)) {
         cfg.primitive_restart = MALI_PRIMITIVE_RESTART_IMPLICIT;
      } else if (info->primitive_restart) {
         cfg.primitive_restart = MALI_PRIMITIVE_RESTART_EXPLICIT;
         cfg.primitive_restart_index = info->restart_index;
      }

      cfg.job_task_split = 6;

      cfg.index_count = ctx->indirect_draw ? 1 : draw->count;
      if (info->index_size) {
         cfg.index_type = panfrost_translate_index_size(info->index_size);
         cfg.indices = indices;
         cfg.base_vertex_offset = draw->index_bias - ctx->offset_start;
      }
   }

   bool points = info->mode == PIPE_PRIM_POINTS;

#if PAN_ARCH >= 6
   void *prim_size = pan_section_ptr(job, BIFROST_TILER_JOB, PRIMITIVE_SIZE);
   panfrost_emit_primitive_size(ctx, points, psiz, prim_size);
   pan_section_pack(job, BIFROST_TILER_JOB, TILER, cfg) {
      cfg.address = panfrost_batch_get_bifrost_tiler(batch, ~0);
   }
   pan_section_pack(job, BIFROST_TILER_JOB, PADDING, padding) {}
#endif

#if PAN_ARCH >= 6
   section = pan_section_ptr(job, BIFROST_TILER_JOB, DRAW);
#else
   section = pan_section_ptr(job, MIDGARD_TILER_JOB, DRAW);
#endif

   pan_pack(section, DRAW, cfg) {
      cfg.four_components_per_vertex = true;
      cfg.draw_descriptor_is_64b = true;
#if PAN_ARCH < 6
      cfg.texture_descriptor_is_64b = true;
#endif
      cfg.front_face_ccw = rast->front_ccw;
      cfg.cull_front_face = rast->cull_face & PIPE_FACE_FRONT;
      cfg.cull_back_face = rast->cull_face & PIPE_FACE_BACK;
      cfg.position = pos;
      cfg.state = pan_emit_frag_shader_meta(batch);
      cfg.attributes = panfrost_emit_image_attribs(batch, &cfg.attribute_buffers, PIPE_SHADER_FRAGMENT);
      cfg.viewport = panfrost_emit_viewport(batch);
      cfg.varyings = fs_vary;
      cfg.varying_buffers = fs_vary ? varyings : 0;
      cfg.thread_storage = shared_mem;

      /* For all primitives but lines DRAW.flat_shading_vertex must
       * be set to 0 and the provoking vertex is selected with the
       * PRIMITIVE.first_provoking_vertex field.
       */
      if (info->mode == PIPE_PRIM_LINES ||
          info->mode == PIPE_PRIM_LINE_LOOP ||
          info->mode == PIPE_PRIM_LINE_STRIP) {
#if PAN_ARCH >= 6
         /* The logic is inverted on bifrost. */
         cfg.flat_shading_vertex = rast->flatshade_first;
#else
         cfg.flat_shading_vertex = !rast->flatshade_first;
#endif
      }

      cfg.offset_start = batch->ctx->offset_start;
      cfg.instance_size = batch->ctx->instance_count > 1 ?
         batch->ctx->padded_count : 1;

      cfg.uniform_buffers = panfrost_emit_const_buf(batch, PIPE_SHADER_FRAGMENT, &cfg.push_uniforms);
      cfg.textures = panfrost_emit_texture_descriptors(batch, PIPE_SHADER_FRAGMENT);
      cfg.samplers = panfrost_emit_sampler_descriptors(batch, PIPE_SHADER_FRAGMENT);

      if (ctx->occlusion_query && ctx->active_queries) {
         if (ctx->occlusion_query->type == PIPE_QUERY_OCCLUSION_COUNTER)
            cfg.occlusion_query = MALI_OCCLUSION_MODE_COUNTER;
         else
            cfg.occlusion_query = MALI_OCCLUSION_MODE_PREDICATE;
         cfg.occlusion = ctx->occlusion_query->bo->ptr.gpu;
         panfrost_batch_add_bo(ctx->batch, ctx->occlusion_query->bo,
                     PAN_BO_ACCESS_SHARED |
                     PAN_BO_ACCESS_RW |
                     PAN_BO_ACCESS_FRAGMENT);
      }
   }

#if PAN_ARCH >= 6
   pan_section_pack(job, BIFROST_TILER_JOB, DRAW_PADDING, cfg);
#else
   void *prim_size = pan_section_ptr(job, MIDGARD_TILER_JOB, PRIMITIVE_SIZE);
   panfrost_emit_primitive_size(ctx, points, psiz, prim_size);
#endif
}
