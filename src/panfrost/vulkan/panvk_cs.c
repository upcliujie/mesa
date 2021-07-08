/*
 * Copyright (C) 2021 Collabora Ltd.
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

#include "util/macros.h"
#include "compiler/shader_enums.h"

#include "panfrost-quirks.h"
#include "pan_cs.h"
#include "pan_encoder.h"
#include "pan_pool.h"

#include "panvk_cs.h"
#include "panvk_private.h"
#include "panvk_varyings.h"

static void
pan_prepare_crc(const struct panfrost_device *dev,
                const struct pan_fb_info *fb, int rt_crc,
                struct MALI_ZS_CRC_EXTENSION *ext)
{
        if (rt_crc < 0)
                return;

        assert(rt_crc < fb->rt_count);

        const struct pan_image_view *rt = fb->rts[rt_crc].view;
        const struct pan_image_slice_layout *slice = &rt->image->layout.slices[rt->first_level];
        ext->crc_base = (rt->image->layout.crc_mode == PAN_IMAGE_CRC_INBAND ?
                         (rt->image->data.bo->ptr.gpu + rt->image->data.offset) :
                         (rt->image->crc.bo->ptr.gpu + rt->image->crc.offset)) +
                        slice->crc.offset;
        ext->crc_row_stride = slice->crc.stride;

        if (dev->arch == 7)
                ext->crc_render_target = rt_crc;

        if (fb->rts[rt_crc].clear) {
                uint32_t clear_val = fb->rts[rt_crc].clear_value[0];
                ext->crc_clear_color = clear_val | 0xc000000000000000 |
                                       (((uint64_t)clear_val & 0xffff) << 32);
        }
}

static enum mali_block_format_v7
mod_to_block_fmt_v7(uint64_t mod)
{
        switch (mod) {
        case DRM_FORMAT_MOD_LINEAR:
                return MALI_BLOCK_FORMAT_V7_LINEAR;
	case DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED:
                return MALI_BLOCK_FORMAT_V7_TILED_U_INTERLEAVED;
        default:
                if (drm_is_afbc(mod))
                        return MALI_BLOCK_FORMAT_V7_AFBC;

                unreachable("Unsupported modifer");
        }
}

static enum mali_block_format
mod_to_block_fmt(uint64_t mod)
{
        switch (mod) {
        case DRM_FORMAT_MOD_LINEAR:
                return MALI_BLOCK_FORMAT_LINEAR;
	case DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED:
                return MALI_BLOCK_FORMAT_TILED_U_INTERLEAVED;
        default:
                if (drm_is_afbc(mod))
                        return MALI_BLOCK_FORMAT_AFBC;

                unreachable("Unsupported modifer");
        }
}

static enum mali_zs_format
translate_zs_format(enum pipe_format in)
{
        switch (in) {
        case PIPE_FORMAT_Z16_UNORM: return MALI_ZS_FORMAT_D16;
        case PIPE_FORMAT_Z24_UNORM_S8_UINT: return MALI_ZS_FORMAT_D24S8;
        case PIPE_FORMAT_Z24X8_UNORM: return MALI_ZS_FORMAT_D24X8;
        case PIPE_FORMAT_Z32_FLOAT: return MALI_ZS_FORMAT_D32;
        case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT: return MALI_ZS_FORMAT_D32_S8X24;
        default: unreachable("Unsupported depth/stencil format.");
        }
}

static enum mali_s_format
translate_s_format(enum pipe_format in)
{
        switch (in) {
        case PIPE_FORMAT_S8_UINT: return MALI_S_FORMAT_S8;
        case PIPE_FORMAT_S8_UINT_Z24_UNORM:
        case PIPE_FORMAT_S8X24_UINT:
                return MALI_S_FORMAT_S8X24;
        case PIPE_FORMAT_Z24_UNORM_S8_UINT:
        case PIPE_FORMAT_X24S8_UINT:
                return MALI_S_FORMAT_X24S8;
        case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
                return MALI_S_FORMAT_X32_S8X24;
        default:
                unreachable("Unsupported stencil format.");
        }
}

static enum mali_msaa
mali_sampling_mode(const struct pan_image_view *view)
{
        if (view->image->layout.nr_samples > 1) {
                assert(view->nr_samples == view->image->layout.nr_samples);
                assert(view->image->layout.slices[0].surface_stride != 0);
                return MALI_MSAA_LAYERED;
        }

        if (view->nr_samples > view->image->layout.nr_samples) {
                assert(view->image->layout.nr_samples == 1);
                return MALI_MSAA_AVERAGE;
        }

        assert(view->nr_samples == view->image->layout.nr_samples);
        assert(view->nr_samples == 1);

        return MALI_MSAA_SINGLE;
}

static void
pan_prepare_s(const struct panfrost_device *dev,
              const struct pan_fb_info *fb,
              struct MALI_ZS_CRC_EXTENSION *ext)
{
        const struct pan_image_view *s = fb->zs.view.s;

        if (!s)
                return;

        unsigned level = s->first_level;

        if (dev->arch < 7)
                ext->s_msaa = mali_sampling_mode(s);
        else
                ext->s_msaa_v7 = mali_sampling_mode(s);

        struct pan_surface surf;
        pan_iview_get_surface(s, 0, 0, 0, &surf);

        assert(s->image->layout.modifier == DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED ||
               s->image->layout.modifier == DRM_FORMAT_MOD_LINEAR);
        ext->s_writeback_base = surf.data;
        ext->s_writeback_row_stride = s->image->layout.slices[level].row_stride;
        ext->s_writeback_surface_stride =
                (s->image->layout.nr_samples > 1) ?
                s->image->layout.slices[level].surface_stride : 0;

        if (dev->arch >= 7)
                ext->s_block_format_v7 = mod_to_block_fmt_v7(s->image->layout.modifier);
        else
                ext->s_block_format = mod_to_block_fmt(s->image->layout.modifier);

        ext->s_write_format = translate_s_format(s->format);
}

static void
pan_prepare_zs(const struct panfrost_device *dev,
               const struct pan_fb_info *fb,
               struct MALI_ZS_CRC_EXTENSION *ext)
{
        const struct pan_image_view *zs = fb->zs.view.zs;

        if (!zs)
                return;

        unsigned level = zs->first_level;

        if (dev->arch < 7)
                ext->zs_msaa = mali_sampling_mode(zs);
        else
                ext->zs_msaa_v7 = mali_sampling_mode(zs);

        struct pan_surface surf;
        pan_iview_get_surface(zs, 0, 0, 0, &surf);

        if (drm_is_afbc(zs->image->layout.modifier)) {
                const struct pan_image_slice_layout *slice = &zs->image->layout.slices[level];

                ext->zs_afbc_header = surf.afbc.header;
                ext->zs_afbc_body = surf.afbc.body;

                if (pan_is_bifrost(dev)) {
                        ext->zs_afbc_row_stride = slice->afbc.row_stride /
                                                  AFBC_HEADER_BYTES_PER_TILE;
                } else {
                        ext->zs_block_format = MALI_BLOCK_FORMAT_AFBC;
                        ext->zs_afbc_body_size = 0x1000;
                        ext->zs_afbc_chunk_size = 9;
                        ext->zs_afbc_sparse = true;
                }
        } else {
                assert(zs->image->layout.modifier == DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED ||
                       zs->image->layout.modifier == DRM_FORMAT_MOD_LINEAR);

                /* TODO: Z32F(S8) support, which is always linear */

                ext->zs_writeback_base = surf.data;
                ext->zs_writeback_row_stride =
                        zs->image->layout.slices[level].row_stride;
                ext->zs_writeback_surface_stride =
                        (zs->image->layout.nr_samples > 1) ?
                        zs->image->layout.slices[level].surface_stride : 0;
        }

        if (dev->arch >= 7)
                ext->zs_block_format_v7 = mod_to_block_fmt_v7(zs->image->layout.modifier);
        else
                ext->zs_block_format = mod_to_block_fmt(zs->image->layout.modifier);

        ext->zs_write_format = translate_zs_format(zs->format);
        if (ext->zs_write_format == MALI_ZS_FORMAT_D24S8)
                ext->s_writeback_base = ext->zs_writeback_base;
}

static void
pan_emit_zs_crc_ext(const struct panfrost_device *dev,
                    const struct pan_fb_info *fb, int rt_crc,
                    void *zs_crc_ext)
{
        pan_pack(zs_crc_ext, ZS_CRC_EXTENSION, cfg) {
                pan_prepare_crc(dev, fb, rt_crc, &cfg);
                cfg.zs_clean_pixel_write_enable = fb->zs.clear.z || fb->zs.clear.s;
                pan_prepare_zs(dev, fb, &cfg);
                pan_prepare_s(dev, fb, &cfg);
        }
}

/* Measure format as it appears in the tile buffer */

static unsigned
pan_bytes_per_pixel_tib(enum pipe_format format)
{
        if (panfrost_blendable_formats_v7[format].internal) {
                /* Blendable formats are always 32-bits in the tile buffer,
                 * extra bits are used as padding or to dither */
                return 4;
        } else {
                /* Non-blendable formats are raw, rounded up to the nearest
                 * power-of-two size */
                unsigned bytes = util_format_get_blocksize(format);
                return util_next_power_of_two(bytes);
        }
}

static unsigned
pan_internal_cbuf_size(const struct pan_fb_info *fb,
                       unsigned *tile_size)
{
        unsigned total_size = 0;

        *tile_size = 16 * 16;
        for (int cb = 0; cb < fb->rt_count; ++cb) {
                const struct pan_image_view *rt = fb->rts[cb].view;

                if (!rt)
                        continue;

                total_size += pan_bytes_per_pixel_tib(rt->format) *
                              rt->nr_samples * (*tile_size);
        }

        /* We have a 4KB budget, let's reduce the tile size until it fits. */
        while (total_size > 4096) {
                total_size >>= 1;
                *tile_size >>= 1;
        }

        /* Align on 1k. */
        total_size = ALIGN_POT(total_size, 1024);

        /* Minimum tile size is 4x4. */
        assert(*tile_size >= 4 * 4);
        return total_size;
}

static inline enum mali_sample_pattern
pan_sample_pattern(unsigned samples)
{
        switch (samples) {
        case 1:  return MALI_SAMPLE_PATTERN_SINGLE_SAMPLED;
        case 4:  return MALI_SAMPLE_PATTERN_ROTATED_4X_GRID;
        case 8:  return MALI_SAMPLE_PATTERN_D3D_8X_GRID;
        case 16: return MALI_SAMPLE_PATTERN_D3D_16X_GRID;
        default: unreachable("Unsupported sample count");
        }
}

int
pan_select_crc_rt(const struct panfrost_device *dev, const struct pan_fb_info *fb)
{
        if (dev->arch < 7) {
                if (fb->rt_count == 1 && fb->rts[0].view && !fb->rts[0].discard &&
                    fb->rts[0].view->image->layout.crc_mode != PAN_IMAGE_CRC_NONE)
                        return 0;

                return -1;
        }

        bool best_rt_valid = false;
        int best_rt = -1;

        for (unsigned i = 0; i < fb->rt_count; i++) {
		if (!fb->rts[i].view || fb->rts[0].discard ||
                    fb->rts[i].view->image->layout.crc_mode == PAN_IMAGE_CRC_NONE)
                        continue;

                bool valid = *(fb->rts[i].crc_valid);
                bool full = !fb->extent.minx && !fb->extent.miny &&
                            fb->extent.maxx == (fb->width - 1) &&
                            fb->extent.maxy == (fb->height - 1);
                if (!full && !valid)
                        continue;

                if (best_rt < 0 || (valid && !best_rt_valid)) {
                        best_rt = i;
                        best_rt_valid = valid;
                }

                if (valid)
                        break;
        }

        return best_rt;
}

bool
pan_fbd_has_zs_crc_ext(const struct panfrost_device *dev,
                       const struct pan_fb_info *fb)
{
        if (dev->quirks & MIDGARD_SFBD)
                return false;

        return fb->zs.view.zs || fb->zs.view.s || pan_select_crc_rt(dev, fb) >= 0;
}

static enum mali_mfbd_color_format
pan_mfbd_raw_format(unsigned bits)
{
        switch (bits) {
        case    8: return MALI_MFBD_COLOR_FORMAT_RAW8;
        case   16: return MALI_MFBD_COLOR_FORMAT_RAW16;
        case   24: return MALI_MFBD_COLOR_FORMAT_RAW24;
        case   32: return MALI_MFBD_COLOR_FORMAT_RAW32;
        case   48: return MALI_MFBD_COLOR_FORMAT_RAW48;
        case   64: return MALI_MFBD_COLOR_FORMAT_RAW64;
        case   96: return MALI_MFBD_COLOR_FORMAT_RAW96;
        case  128: return MALI_MFBD_COLOR_FORMAT_RAW128;
        case  192: return MALI_MFBD_COLOR_FORMAT_RAW192;
        case  256: return MALI_MFBD_COLOR_FORMAT_RAW256;
        case  384: return MALI_MFBD_COLOR_FORMAT_RAW384;
        case  512: return MALI_MFBD_COLOR_FORMAT_RAW512;
        case  768: return MALI_MFBD_COLOR_FORMAT_RAW768;
        case 1024: return MALI_MFBD_COLOR_FORMAT_RAW1024;
        case 1536: return MALI_MFBD_COLOR_FORMAT_RAW1536;
        case 2048: return MALI_MFBD_COLOR_FORMAT_RAW2048;
        default: unreachable("invalid raw bpp");
        }
}

static void
pan_rt_init_format(const struct panfrost_device *dev,
                   const struct pan_image_view *rt,
                   struct MALI_RENDER_TARGET *cfg)
{
        /* Explode details on the format */

        const struct util_format_description *desc =
                util_format_description(rt->format);

        /* The swizzle for rendering is inverted from texturing */

        unsigned char swizzle[4];
        panfrost_invert_swizzle(desc->swizzle, swizzle);

        cfg->swizzle = panfrost_translate_swizzle_4(swizzle);

        /* Fill in accordingly, defaulting to 8-bit UNORM */

        if (desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB)
                cfg->srgb = true;

        struct pan_blendable_format fmt = panfrost_blendable_formats_v7[rt->format];

        if (fmt.internal) {
                cfg->internal_format = fmt.internal;
                cfg->writeback_format = fmt.writeback;
        } else {
                /* Construct RAW internal/writeback, where internal is
                 * specified logarithmically (round to next power-of-two).
                 * Offset specified from RAW8, where 8 = 2^3 */

                unsigned bits = desc->block.bits;
                unsigned offset = util_logbase2_ceil(bits) - 3;
                assert(offset <= 4);

                cfg->internal_format =
                        MALI_COLOR_BUFFER_INTERNAL_FORMAT_RAW8 + offset;

                cfg->writeback_format = pan_mfbd_raw_format(bits);
        }
}

static void
pan_prepare_rt(const struct panfrost_device *dev,
               const struct pan_fb_info *fb, unsigned idx,
               unsigned cbuf_offset,
               struct MALI_RENDER_TARGET *cfg)
{
        cfg->clean_pixel_write_enable = fb->rts[idx].clear;
        cfg->internal_buffer_offset = cbuf_offset;
        if (fb->rts[idx].clear) {
                cfg->clear.color_0 = fb->rts[idx].clear_value[0];
                cfg->clear.color_1 = fb->rts[idx].clear_value[1];
                cfg->clear.color_2 = fb->rts[idx].clear_value[2];
                cfg->clear.color_3 = fb->rts[idx].clear_value[3];
        }

        const struct pan_image_view *rt = fb->rts[idx].view;
        if (!rt || fb->rts[idx].discard) {
                cfg->internal_format = MALI_COLOR_BUFFER_INTERNAL_FORMAT_R8G8B8A8;
                cfg->internal_buffer_offset = cbuf_offset;
                if (dev->arch >= 7) {
                        cfg->bifrost_v7.writeback_block_format = MALI_BLOCK_FORMAT_V7_TILED_U_INTERLEAVED;
                        cfg->dithering_enable = true;
                }

                return;
        }

        cfg->write_enable = true;
        cfg->dithering_enable = true;

        unsigned level = rt->first_level;
        assert(rt->last_level == rt->first_level);
        assert(rt->last_layer == rt->first_layer);

        int row_stride = rt->image->layout.slices[level].row_stride;

        /* Only set layer_stride for layered MSAA rendering  */

        unsigned layer_stride =
                (rt->image->layout.nr_samples > 1) ?
                        rt->image->layout.slices[level].surface_stride : 0;

        cfg->writeback_msaa = mali_sampling_mode(rt);

        pan_rt_init_format(dev, rt, cfg);

        if (dev->arch >= 7)
                cfg->bifrost_v7.writeback_block_format = mod_to_block_fmt_v7(rt->image->layout.modifier);
        else
                cfg->midgard.writeback_block_format = mod_to_block_fmt(rt->image->layout.modifier);

        struct pan_surface surf;
        pan_iview_get_surface(rt, 0, 0, 0, &surf);

        if (drm_is_afbc(rt->image->layout.modifier)) {
                const struct pan_image_slice_layout *slice = &rt->image->layout.slices[level];

                if (pan_is_bifrost(dev)) {
                        cfg->afbc.row_stride = slice->afbc.row_stride /
                                               AFBC_HEADER_BYTES_PER_TILE;
                        cfg->bifrost_afbc.afbc_wide_block_enable =
                                panfrost_block_dim(rt->image->layout.modifier, true, 0) > 16;
                } else {
                        cfg->afbc.chunk_size = 9;
                        cfg->midgard_afbc.sparse = true;
                        cfg->afbc.body_size = slice->afbc.body_size;
                }

                cfg->afbc.header = surf.afbc.header;
                cfg->afbc.body = surf.afbc.body;

                if (rt->image->layout.modifier & AFBC_FORMAT_MOD_YTR)
                        cfg->afbc.yuv_transform_enable = true;
        } else {
                assert(rt->image->layout.modifier == DRM_FORMAT_MOD_LINEAR ||
                       rt->image->layout.modifier == DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED);
                cfg->rgb.base = surf.data;
                cfg->rgb.row_stride = row_stride;
                cfg->rgb.surface_stride = layer_stride;
        }
}

static void
pan_emit_rt(const struct panfrost_device *dev,
            const struct pan_fb_info *fb,
            unsigned idx, unsigned cbuf_offset, void *out)
{
        pan_pack(out, RENDER_TARGET, cfg) {
                pan_prepare_rt(dev, fb, idx, cbuf_offset, &cfg);
        }
}

static unsigned
pan_wls_instances(const struct pan_compute_dim *dim)
{
        return util_next_power_of_two(dim->x) *
               util_next_power_of_two(dim->y) *
               util_next_power_of_two(dim->z);
}

static unsigned
pan_wls_adjust_size(unsigned wls_size)
{
        return util_next_power_of_two(MAX2(wls_size, 128));
}

unsigned
pan_wls_mem_size(const struct panfrost_device *dev,
                 const struct pan_compute_dim *dim,
                 unsigned wls_size)
{
        unsigned instances = pan_wls_instances(dim);

        return pan_wls_adjust_size(wls_size) * instances * dev->core_count;
}

void
pan_emit_tls(const struct panfrost_device *dev,
             const struct pan_tls_info *info,
             void *out)
{
        pan_pack(out, LOCAL_STORAGE, cfg) {
                if (info->tls.size) {
                        unsigned shift =
                                panfrost_get_stack_shift(info->tls.size);

                        /* TODO: Why do we need to make the stack bigger than other platforms? */
	                if (dev->quirks & MIDGARD_SFBD)
                                shift = MAX2(shift, 512);

                        cfg.tls_size = shift;
                        cfg.tls_base_pointer = info->tls.ptr;
                }

                if (info->wls.size) {
                        assert(!(info->wls.ptr & 4095));
                        assert((info->wls.ptr & 0xffffffff00000000ULL) == ((info->wls.ptr + info->wls.size - 1) & 0xffffffff00000000ULL));
                        cfg.wls_base_pointer = info->wls.ptr;
                        unsigned wls_size = pan_wls_adjust_size(info->wls.size);
                        cfg.wls_instances = pan_wls_instances(&info->wls.dim);
                        cfg.wls_size_scale = util_logbase2(wls_size) + 1;
                } else {
                        cfg.wls_instances = MALI_LOCAL_STORAGE_NO_WORKGROUP_MEM;
                }
        }
}

static void
pan_emit_bifrost_mfbd_params(const struct panfrost_device *dev,
                             const struct pan_fb_info *fb,
                             void *fbd)
{
        pan_section_pack(fbd, MULTI_TARGET_FRAMEBUFFER, BIFROST_PARAMETERS, params) {
                params.sample_locations =
                        panfrost_sample_positions(dev, pan_sample_pattern(fb->nr_samples));
                params.pre_frame_0 = fb->bifrost.pre_post.modes[0];
                params.pre_frame_1 = fb->bifrost.pre_post.modes[1];
                params.post_frame = fb->bifrost.pre_post.modes[2];
                params.frame_shader_dcds = fb->bifrost.pre_post.dcds.gpu;
        }
}

static void
pan_emit_mfbd_bifrost_tiler(const struct pan_tiler_context *ctx, void *fbd)
{
        pan_section_pack(fbd, MULTI_TARGET_FRAMEBUFFER, BIFROST_TILER_POINTER, cfg) {
                cfg.address = ctx->bifrost;
        }
        pan_section_pack(fbd, MULTI_TARGET_FRAMEBUFFER, BIFROST_PADDING, padding);
}

static void
pan_emit_midgard_tiler(const struct panfrost_device *dev,
                       const struct pan_fb_info *fb,
                       const struct pan_tiler_context *tiler_ctx,
                       void *out)
{
        bool hierarchy = !(dev->quirks & MIDGARD_NO_HIER_TILING);

        assert(tiler_ctx->midgard.polygon_list->ptr.gpu);

        pan_pack(out, MIDGARD_TILER, cfg) {
                unsigned header_size;

                if (tiler_ctx->midgard.disable) {
                        cfg.hierarchy_mask =
                                hierarchy ?
                                MALI_MIDGARD_TILER_DISABLED :
                                MALI_MIDGARD_TILER_USER;
                        header_size = MALI_MIDGARD_TILER_MINIMUM_HEADER_SIZE;
                        cfg.polygon_list_size = header_size + (hierarchy ? 0 : 4);
                        cfg.heap_start = tiler_ctx->midgard.polygon_list->ptr.gpu;
                        cfg.heap_end = tiler_ctx->midgard.polygon_list->ptr.gpu;
		} else {
                        cfg.hierarchy_mask =
                                panfrost_choose_hierarchy_mask(fb->width,
                                                               fb->height,
                                                               1, hierarchy);
                        header_size = panfrost_tiler_header_size(fb->width,
                                                                 fb->height,
                                                                 cfg.hierarchy_mask,
                                                                 hierarchy);
                        cfg.polygon_list_size =
                                panfrost_tiler_full_size(fb->width, fb->height,
                                                         cfg.hierarchy_mask,
                                                         hierarchy);
                        cfg.heap_start = dev->tiler_heap->ptr.gpu;
                        cfg.heap_end = dev->tiler_heap->ptr.gpu + dev->tiler_heap->size;
                }

                cfg.polygon_list = tiler_ctx->midgard.polygon_list->ptr.gpu;
                cfg.polygon_list_body = cfg.polygon_list + header_size;
        }
}

static void
pan_emit_mfbd_midgard_tiler(const struct panfrost_device *dev,
                            const struct pan_fb_info *fb,
                            const struct pan_tiler_context *ctx,
                            void *fbd)
{
       pan_emit_midgard_tiler(dev, fb, ctx,
                              pan_section_ptr(fbd, MULTI_TARGET_FRAMEBUFFER, TILER));

        /* All weights set to 0, nothing to do here */
        pan_section_pack(fbd, MULTI_TARGET_FRAMEBUFFER, TILER_WEIGHTS, w);
}

static void
pan_emit_sfbd_tiler(const struct panfrost_device *dev,
                    const struct pan_fb_info *fb,
                    const struct pan_tiler_context *ctx,
                    void *fbd)
{
       pan_emit_midgard_tiler(dev, fb, ctx,
                              pan_section_ptr(fbd, SINGLE_TARGET_FRAMEBUFFER, TILER));

        /* All weights set to 0, nothing to do here */
        pan_section_pack(fbd, SINGLE_TARGET_FRAMEBUFFER, PADDING_1, padding);
        pan_section_pack(fbd, SINGLE_TARGET_FRAMEBUFFER, TILER_WEIGHTS, w);
}

static unsigned
pan_emit_mfbd(const struct panfrost_device *dev,
              const struct pan_fb_info *fb,
              const struct pan_tls_info *tls,
              const struct pan_tiler_context *tiler_ctx,
              void *out)
{
        unsigned tags = MALI_FBD_TAG_IS_MFBD;
        void *fbd = out;
        void *rtd = out + MALI_MULTI_TARGET_FRAMEBUFFER_LENGTH;

        if (pan_is_bifrost(dev)) {
                pan_emit_bifrost_mfbd_params(dev, fb, fbd);
        } else {
                pan_emit_tls(dev, tls,
                             pan_section_ptr(fbd, MULTI_TARGET_FRAMEBUFFER,
                                             LOCAL_STORAGE));
        }

        unsigned tile_size;
        unsigned internal_cbuf_size = pan_internal_cbuf_size(fb, &tile_size);
        int crc_rt = pan_select_crc_rt(dev, fb);
        bool has_zs_crc_ext = pan_fbd_has_zs_crc_ext(dev, fb);

        pan_section_pack(fbd, MULTI_TARGET_FRAMEBUFFER, PARAMETERS, cfg) {
                cfg.width = fb->width;
                cfg.height = fb->height;
                cfg.bound_max_x = fb->width - 1;
                cfg.bound_max_y = fb->height - 1;

                cfg.effective_tile_size = tile_size;
                cfg.tie_break_rule = MALI_TIE_BREAK_RULE_MINUS_180_IN_0_OUT;
                cfg.render_target_count = MAX2(fb->rt_count, 1);

                /* Default to 24 bit depth if there's no surface. */
                cfg.z_internal_format =
                        fb->zs.view.zs ?
                        panfrost_get_z_internal_format(fb->zs.view.zs->format) :
                        MALI_Z_INTERNAL_FORMAT_D24;

                cfg.z_clear = fb->zs.clear_value.depth;
                cfg.s_clear = fb->zs.clear_value.stencil;
                cfg.color_buffer_allocation = internal_cbuf_size;
                cfg.sample_count = fb->nr_samples;
                cfg.sample_pattern = pan_sample_pattern(fb->nr_samples);
                cfg.z_write_enable = (fb->zs.view.zs && !fb->zs.discard.z);
                cfg.s_write_enable = (fb->zs.view.s && !fb->zs.discard.s);
                cfg.has_zs_crc_extension = has_zs_crc_ext;

                if (crc_rt >= 0) {
                        bool *valid = fb->rts[crc_rt].crc_valid;
                        bool full = !fb->extent.minx && !fb->extent.miny &&
                                    fb->extent.maxx == (fb->width - 1) &&
                                    fb->extent.maxy == (fb->height - 1);

                        cfg.crc_read_enable = *valid;

                        /* If the data is currently invalid, still write CRC
                         * data if we are doing a full write, so that it is
                         * valid for next time. */
                        cfg.crc_write_enable = *valid || full;

                        *valid |= full;
                }
        }

        if (pan_is_bifrost(dev))
                pan_emit_mfbd_bifrost_tiler(tiler_ctx, fbd);
        else
                pan_emit_mfbd_midgard_tiler(dev, fb, tiler_ctx, fbd);

        if (has_zs_crc_ext) {
                pan_emit_zs_crc_ext(dev, fb, crc_rt,
                                    out + MALI_MULTI_TARGET_FRAMEBUFFER_LENGTH);
                rtd += MALI_ZS_CRC_EXTENSION_LENGTH;
                tags |= MALI_FBD_TAG_HAS_ZS_RT;
        }

        unsigned rt_count = MAX2(fb->rt_count, 1);
        unsigned cbuf_offset = 0;
        for (unsigned i = 0; i < rt_count; i++) {
                pan_emit_rt(dev, fb, i, cbuf_offset, rtd);
                rtd += MALI_RENDER_TARGET_LENGTH;
                if (!fb->rts[i].view)
                        continue;

                cbuf_offset += pan_bytes_per_pixel_tib(fb->rts[i].view->format) *
                               tile_size * fb->rts[i].view->image->layout.nr_samples;

                if (i != crc_rt)
                        *(fb->rts[i].crc_valid) = false;
        }
        tags |= MALI_POSITIVE(MAX2(fb->rt_count, 1)) << 2;

        return tags;
}

static void
pan_emit_sfbd(const struct panfrost_device *dev,
              const struct pan_fb_info *fb,
              const struct pan_tls_info *tls,
              const struct pan_tiler_context *tiler_ctx,
              void *fbd)
{
        pan_emit_tls(dev, tls,
                     pan_section_ptr(fbd, SINGLE_TARGET_FRAMEBUFFER,
                                     LOCAL_STORAGE));
        pan_section_pack(fbd, SINGLE_TARGET_FRAMEBUFFER, PARAMETERS, cfg) {
                cfg.bound_max_x = fb->width - 1;
                cfg.bound_max_y = fb->height - 1;
                cfg.dithering_enable = true;
                cfg.clean_pixel_write_enable = true;
                cfg.tie_break_rule = MALI_TIE_BREAK_RULE_MINUS_180_IN_0_OUT;
                if (fb->rts[0].clear) {
                        cfg.clear_color_0 = fb->rts[0].clear_value[0];
                        cfg.clear_color_1 = fb->rts[0].clear_value[1];
                        cfg.clear_color_2 = fb->rts[0].clear_value[2];
                        cfg.clear_color_3 = fb->rts[0].clear_value[3];
                }

                if (fb->zs.clear.z)
                        cfg.z_clear = fb->zs.clear_value.depth;

                if (fb->zs.clear.s)
                        cfg.s_clear = fb->zs.clear_value.stencil;

                if (fb->rt_count && fb->rts[0].view) {
                        const struct pan_image_view *rt = fb->rts[0].view;

                        const struct util_format_description *desc =
                                util_format_description(rt->format);

                        /* The swizzle for rendering is inverted from texturing */
                        unsigned char swizzle[4];
                        panfrost_invert_swizzle(desc->swizzle, swizzle);
                        cfg.swizzle = panfrost_translate_swizzle_4(swizzle);

                        struct pan_blendable_format fmt = panfrost_blendable_formats_v7[rt->format];
                        if (fmt.internal) {
                                cfg.internal_format = fmt.internal;
                                cfg.color_writeback_format = fmt.writeback;
                        } else {
                                unreachable("raw formats not finished for SFBD");
                        }

                        unsigned level = rt->first_level;
                        struct pan_surface surf;

                        pan_iview_get_surface(rt, 0, 0, 0, &surf);

                        cfg.color_write_enable = !fb->rts[0].discard;
                        cfg.color_writeback.base = surf.data;
                        cfg.color_writeback.row_stride =
	                        rt->image->layout.slices[level].row_stride;

                        cfg.color_block_format = mod_to_block_fmt(rt->image->layout.modifier);
                        assert(cfg.color_block_format == MALI_BLOCK_FORMAT_LINEAR ||
                               cfg.color_block_format == MALI_BLOCK_FORMAT_TILED_U_INTERLEAVED);

                        if (rt->image->layout.crc_mode != PAN_IMAGE_CRC_NONE) {
                                const struct pan_image_slice_layout *slice =
                                        &rt->image->layout.slices[level];

                                cfg.crc_buffer.row_stride = slice->crc.stride;
                                if (rt->image->layout.crc_mode == PAN_IMAGE_CRC_INBAND) {
                                        cfg.crc_buffer.base = rt->image->data.bo->ptr.gpu +
                                                              rt->image->data.offset +
                                                              slice->crc.offset;
                                } else {
                                        cfg.crc_buffer.base = rt->image->crc.bo->ptr.gpu +
                                                              rt->image->crc.offset +
                                                              slice->crc.offset;
                                }
                        }
                }

                if (fb->zs.view.zs) {
                        const struct pan_image_view *zs = fb->zs.view.zs;
                        unsigned level = zs->first_level;
                        struct pan_surface surf;

                        pan_iview_get_surface(zs, 0, 0, 0, &surf);

                        cfg.zs_write_enable = !fb->zs.discard.z;
                        cfg.zs_writeback.base = surf.data;
                        cfg.zs_writeback.row_stride =
                                zs->image->layout.slices[level].row_stride;
                        cfg.zs_block_format = mod_to_block_fmt(zs->image->layout.modifier);
                        assert(cfg.zs_block_format == MALI_BLOCK_FORMAT_LINEAR ||
                               cfg.zs_block_format == MALI_BLOCK_FORMAT_TILED_U_INTERLEAVED);

                        cfg.zs_format = translate_zs_format(zs->format);
                }

                cfg.sample_count = fb->nr_samples;

                /* XXX: different behaviour from MFBD and probably wrong... */
                cfg.msaa = mali_sampling_mode(fb->rts[0].view);
        }
        pan_emit_sfbd_tiler(dev, fb, tiler_ctx, fbd);
        pan_section_pack(fbd, SINGLE_TARGET_FRAMEBUFFER, PADDING_2, padding);
}

unsigned
pan_emit_fbd(const struct panfrost_device *dev,
             const struct pan_fb_info *fb,
             const struct pan_tls_info *tls,
             const struct pan_tiler_context *tiler_ctx,
             void *out)
{
        if (dev->quirks & MIDGARD_SFBD) {
                assert(fb->rt_count <= 1);
                pan_emit_sfbd(dev, fb, tls, tiler_ctx, out);
                return 0;
        } else {
                return pan_emit_mfbd(dev, fb, tls, tiler_ctx, out);
        }
}

static mali_pixel_format
panvk_varying_hw_format(const struct panvk_device *dev,
                        const struct panvk_varyings_info *varyings,
                        gl_shader_stage stage, unsigned idx)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;
   gl_varying_slot loc = varyings->stage[stage].loc[idx];
   bool fs = stage == MESA_SHADER_FRAGMENT;

   switch (loc) {
   case VARYING_SLOT_PNTC:
   case VARYING_SLOT_PSIZ:
      return (MALI_R16F << 12) |
             (pdev->quirks & HAS_SWIZZLES ?
              panfrost_get_default_swizzle(1) : 0);
   case VARYING_SLOT_POS:
      return ((fs ? MALI_RGBA32F : MALI_SNAP_4) << 12) |
             (pdev->quirks & HAS_SWIZZLES ?
              panfrost_get_default_swizzle(4) : 0);
   default:
      assert(!panvk_varying_is_builtin(stage, loc));
      return pdev->formats[varyings->varying[loc].format].hw;
   }
}

static void
panvk_emit_varying(const struct panvk_device *dev,
                   const struct panvk_varyings_info *varyings,
                   gl_shader_stage stage, unsigned idx,
                   void *attrib)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;
   gl_varying_slot loc = varyings->stage[stage].loc[idx];
   bool fs = stage == MESA_SHADER_FRAGMENT;

   pan_pack(attrib, ATTRIBUTE, cfg) {
      if (!panvk_varying_is_builtin(stage, loc)) {
         cfg.buffer_index = varyings->varying[loc].buf;
         cfg.offset = varyings->varying[loc].offset;
      } else {
         cfg.buffer_index =
            panvk_varying_buf_index(varyings,
                                    panvk_varying_buf_id(fs, loc));
      }
      cfg.offset_enable = !pan_is_bifrost(pdev);
      cfg.format = panvk_varying_hw_format(dev, varyings, stage, idx);
   }
}

void
panvk_emit_varyings(const struct panvk_device *dev,
                    const struct panvk_varyings_info *varyings,
                    gl_shader_stage stage,
                    void *descs)
{
   struct mali_attribute_packed *attrib = descs;

   for (unsigned i = 0; i < varyings->stage[stage].count; i++)
      panvk_emit_varying(dev, varyings, stage, i, attrib++);
}

static void
panvk_emit_varying_buf(const struct panvk_device *dev,
                       const struct panvk_varyings_info *varyings,
                       enum panvk_varying_buf_id id, void *buf)
{
   unsigned buf_idx = panvk_varying_buf_index(varyings, id);
   enum mali_attribute_special special_id = panvk_varying_special_buf_id(id);

   pan_pack(buf, ATTRIBUTE_BUFFER, cfg) {
      if (special_id) {
         cfg.type = 0;
         cfg.special = special_id;
      } else {
         unsigned offset = varyings->buf[buf_idx].address & 63;

         cfg.stride = varyings->buf[buf_idx].stride;
         cfg.size = varyings->buf[buf_idx].size + offset;
         cfg.pointer = varyings->buf[buf_idx].address & ~63ULL;
      }
   }
}

void
panvk_emit_varying_bufs(const struct panvk_device *dev,
                        const struct panvk_varyings_info *varyings,
                        void *descs)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;
   struct mali_attribute_buffer_packed *buf = descs;

   for (unsigned i = 0; i < PANVK_VARY_BUF_MAX; i++) {
      if (varyings->buf_mask & (1 << i))
         panvk_emit_varying_buf(dev, varyings, i, buf++);
   }

   if (pan_is_bifrost(pdev))
      memset(buf, 0, sizeof(*buf));
}

static void
panvk_emit_attrib_buf(const struct panvk_device *dev,
                      const struct panvk_attribs_info *info,
                      const struct panvk_draw_info *draw,
                      const struct panvk_attrib_buf *bufs,
                      unsigned buf_count,
                      unsigned idx, void *desc)
{
   ASSERTED const struct panfrost_device *pdev = &dev->physical_device->pdev;
   const struct panvk_attrib_buf_info *buf_info = &info->buf[idx];

   if (buf_info->special) {
      assert(!pan_is_bifrost(pdev));
      switch (buf_info->special_id) {
      case PAN_VERTEX_ID:
         panfrost_vertex_id(draw->padded_vertex_count, desc,
                            draw->instance_count > 1);
         return;
      case PAN_INSTANCE_ID:
         panfrost_instance_id(draw->padded_vertex_count, desc,
                              draw->instance_count > 1);
         return;
      default:
         unreachable("Invalid attribute ID");
      }
   }

   assert(idx < buf_count);
   const struct panvk_attrib_buf *buf = &bufs[idx];
   unsigned divisor = buf_info->per_instance ?
                      draw->padded_vertex_count : 0;
   unsigned stride = divisor && draw->instance_count == 1 ?
                     0 : buf_info->stride;
   mali_ptr addr = buf->address & ~63ULL;
   unsigned size = buf->size + (buf->address & 63);

   /* TODO: support instanced arrays */
   pan_pack(desc, ATTRIBUTE_BUFFER, cfg) {
      if (draw->instance_count > 1 && divisor) {
         cfg.type = MALI_ATTRIBUTE_TYPE_1D_MODULUS;
         cfg.divisor = divisor;
      }

      cfg.pointer = addr;
      cfg.stride = stride;
      cfg.size = size;
   }
}

void
panvk_emit_attrib_bufs(const struct panvk_device *dev,
                       const struct panvk_attribs_info *info,
                       const struct panvk_attrib_buf *bufs,
                       unsigned buf_count,
                       const struct panvk_draw_info *draw,
                       void *descs)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;
   struct mali_attribute_buffer_packed *buf = descs;

   for (unsigned i = 0; i < info->buf_count; i++)
      panvk_emit_attrib_buf(dev, info, draw, bufs, buf_count, i, buf++);

   /* A NULL entry is needed to stop prefecting on Bifrost */
   if (pan_is_bifrost(pdev))
      memset(buf, 0, sizeof(*buf));
}

static void
panvk_emit_attrib(const struct panvk_device *dev,
                  const struct panvk_attribs_info *attribs,
                  const struct panvk_attrib_buf *bufs,
                  unsigned buf_count,
                  unsigned idx, void *attrib)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;

   pan_pack(attrib, ATTRIBUTE, cfg) {
      cfg.buffer_index = attribs->attrib[idx].buf;
      cfg.offset = attribs->attrib[idx].offset +
                   (bufs[cfg.buffer_index].address & 63);
      cfg.format = pdev->formats[attribs->attrib[idx].format].hw;
   }
}

void
panvk_emit_attribs(const struct panvk_device *dev,
                   const struct panvk_attribs_info *attribs,
                   const struct panvk_attrib_buf *bufs,
                   unsigned buf_count,
                   void *descs)
{
   struct mali_attribute_packed *attrib = descs;

   for (unsigned i = 0; i < attribs->attrib_count; i++)
      panvk_emit_attrib(dev, attribs, bufs, buf_count, i, attrib++);
}

void
panvk_emit_ubos(const struct panvk_pipeline *pipeline,
                const struct panvk_descriptor_state *state,
                void *descs)
{
   struct mali_uniform_buffer_packed *ubos = descs;

   for (unsigned i = 0; i < ARRAY_SIZE(state->sets); i++) {
      const struct panvk_descriptor_set_layout *set_layout =
         pipeline->layout->sets[i].layout;
      const struct panvk_descriptor_set *set = state->sets[i].set;
      unsigned offset = pipeline->layout->sets[i].ubo_offset;

      if (!set_layout)
         continue;

      if (!set) {
         unsigned num_ubos = (set_layout->num_dynoffsets != 0) + set_layout->num_ubos;
         memset(&ubos[offset], 0, num_ubos * sizeof(*ubos));
      } else {
         memcpy(&ubos[offset], set->ubos, set_layout->num_ubos * sizeof(*ubos));
         if (set_layout->num_dynoffsets) {
            pan_pack(&ubos[offset + set_layout->num_ubos], UNIFORM_BUFFER, cfg) {
               cfg.pointer = state->sets[i].dynoffsets.gpu;
               cfg.entries = DIV_ROUND_UP(set->layout->num_dynoffsets, 16);
            }
         }
      }
   }

   for (unsigned i = 0; i < ARRAY_SIZE(pipeline->sysvals); i++) {
      if (!pipeline->sysvals[i].ids.sysval_count)
         continue;

      pan_pack(&ubos[pipeline->sysvals[i].ubo_idx], UNIFORM_BUFFER, cfg) {
         cfg.pointer = pipeline->sysvals[i].ubo ? :
                       state->sysvals[i];
         cfg.entries = pipeline->sysvals[i].ids.sysval_count;
      }
   }
}

void
panvk_emit_vertex_job(const struct panvk_device *dev,
                      const struct panvk_pipeline *pipeline,
                      const struct panvk_draw_info *draw,
                      void *job)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;
   void *section = pan_section_ptr(job, COMPUTE_JOB, INVOCATION);

   memcpy(section, &draw->invocation, MALI_INVOCATION_LENGTH);

   pan_section_pack(job, COMPUTE_JOB, PARAMETERS, cfg) {
      cfg.job_task_split = 5;
   }

   pan_section_pack(job, COMPUTE_JOB, DRAW, cfg) {
      cfg.draw_descriptor_is_64b = true;
      if (!pan_is_bifrost(pdev))
         cfg.texture_descriptor_is_64b = true;
      cfg.state = pipeline->rsds[MESA_SHADER_VERTEX];
      cfg.attributes = draw->stages[MESA_SHADER_VERTEX].attributes;
      cfg.attribute_buffers = draw->attribute_bufs;
      cfg.varyings = draw->stages[MESA_SHADER_VERTEX].varyings;
      cfg.varying_buffers = draw->varying_bufs;
      cfg.thread_storage = draw->tls;
      cfg.offset_start = draw->offset_start;
      cfg.instance_size = draw->instance_count > 1 ?
                          draw->padded_vertex_count : 1;
      cfg.uniform_buffers = draw->ubos;
      cfg.push_uniforms = draw->stages[PIPE_SHADER_VERTEX].push_constants;
      cfg.textures = draw->textures;
      cfg.samplers = draw->samplers;
   }

   pan_section_pack(job, COMPUTE_JOB, DRAW_PADDING, cfg);
}

void
panvk_emit_tiler_job(const struct panvk_device *dev,
                     const struct panvk_pipeline *pipeline,
                     const struct panvk_draw_info *draw,
                     void *job)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;
   void *section = pan_is_bifrost(pdev) ?
                   pan_section_ptr(job, BIFROST_TILER_JOB, INVOCATION) :
                   pan_section_ptr(job, MIDGARD_TILER_JOB, INVOCATION);

   memcpy(section, &draw->invocation, MALI_INVOCATION_LENGTH);

   section = pan_is_bifrost(pdev) ?
             pan_section_ptr(job, BIFROST_TILER_JOB, PRIMITIVE) :
             pan_section_ptr(job, MIDGARD_TILER_JOB, PRIMITIVE);

   pan_pack(section, PRIMITIVE, cfg) {
      cfg.draw_mode = pipeline->ia.topology;
      if (pipeline->ia.writes_point_size)
         cfg.point_size_array_format = MALI_POINT_SIZE_ARRAY_FORMAT_FP16;

      cfg.first_provoking_vertex = true;
      if (pipeline->ia.primitive_restart)
         cfg.primitive_restart = MALI_PRIMITIVE_RESTART_IMPLICIT;
      cfg.job_task_split = 6;
      /* TODO: indexed draws */
      cfg.index_count = draw->vertex_count;
   }

   section = pan_is_bifrost(pdev) ?
             pan_section_ptr(job, BIFROST_TILER_JOB, PRIMITIVE_SIZE) :
             pan_section_ptr(job, MIDGARD_TILER_JOB, PRIMITIVE_SIZE);
   pan_pack(section, PRIMITIVE_SIZE, cfg) {
      if (pipeline->ia.writes_point_size) {
         cfg.size_array = draw->psiz;
      } else {
         cfg.constant = draw->line_width;
      }
   }

   section = pan_is_bifrost(pdev) ?
             pan_section_ptr(job, BIFROST_TILER_JOB, DRAW) :
             pan_section_ptr(job, MIDGARD_TILER_JOB, DRAW);

   pan_pack(section, DRAW, cfg) {
      cfg.four_components_per_vertex = true;
      cfg.draw_descriptor_is_64b = true;
      if (!pan_is_bifrost(pdev))
         cfg.texture_descriptor_is_64b = true;
      cfg.front_face_ccw = pipeline->rast.front_ccw;
      cfg.cull_front_face = pipeline->rast.cull_front_face;
      cfg.cull_back_face = pipeline->rast.cull_back_face;
      cfg.position = draw->position;
      cfg.state = draw->fs_rsd;
      cfg.attributes = draw->stages[MESA_SHADER_FRAGMENT].attributes;
      cfg.attribute_buffers = draw->attribute_bufs;
      cfg.viewport = draw->viewport;
      cfg.varyings = draw->stages[MESA_SHADER_FRAGMENT].varyings;
      cfg.varying_buffers = cfg.varyings ? draw->varying_bufs : 0;
      if (pan_is_bifrost(pdev))
         cfg.thread_storage = draw->tls;
      else
         cfg.fbd = draw->fb;

      /* For all primitives but lines DRAW.flat_shading_vertex must
       * be set to 0 and the provoking vertex is selected with the
       * PRIMITIVE.first_provoking_vertex field.
       */
      if (pipeline->ia.topology == MALI_DRAW_MODE_LINES ||
          pipeline->ia.topology == MALI_DRAW_MODE_LINE_STRIP ||
          pipeline->ia.topology == MALI_DRAW_MODE_LINE_LOOP) {
         /* The logic is inverted on bifrost. */
         cfg.flat_shading_vertex = pan_is_bifrost(pdev) ?
                                   true : false;
      }

      cfg.offset_start = draw->offset_start;
      cfg.instance_size = draw->instance_count > 1 ?
                         draw->padded_vertex_count : 1;
      cfg.uniform_buffers = draw->ubos;
      cfg.push_uniforms = draw->stages[PIPE_SHADER_FRAGMENT].push_constants;
      cfg.textures = draw->textures;
      cfg.samplers = draw->samplers;

      /* TODO: occlusion queries */
   }

   if (pan_is_bifrost(pdev)) {
      pan_section_pack(job, BIFROST_TILER_JOB, TILER, cfg) {
         cfg.address = draw->tiler_ctx->bifrost;
      }
      pan_section_pack(job, BIFROST_TILER_JOB, DRAW_PADDING, padding);
      pan_section_pack(job, BIFROST_TILER_JOB, PADDING, padding);
   }
}

void
panvk_emit_fragment_job(const struct panvk_device *dev,
                        const struct panvk_framebuffer *fb,
                        mali_ptr fbdesc,
                        void *job)
{
   pan_section_pack(job, FRAGMENT_JOB, HEADER, header) {
      header.type = MALI_JOB_TYPE_FRAGMENT;
      header.index = 1;
   }

   pan_section_pack(job, FRAGMENT_JOB, PAYLOAD, payload) {
      payload.bound_min_x = 0;
      payload.bound_min_y = 0;

      payload.bound_max_x = (fb->width - 1) >> MALI_TILE_SHIFT;
      payload.bound_max_y = (fb->height - 1) >> MALI_TILE_SHIFT;
      payload.framebuffer = fbdesc;
   }
}

void
panvk_emit_viewport(const VkViewport *viewport, const VkRect2D *scissor,
                    void *vpd)
{
   /* The spec says "width must be greater than 0.0" */
   assert(viewport->x >= 0);
   int minx = (int)viewport->x;
   int maxx = (int)(viewport->x + viewport->width);

   /* Viewport height can be negative */
   int miny = MIN2((int)viewport->y, (int)(viewport->y + viewport->height));
   int maxy = MAX2((int)viewport->y, (int)(viewport->y + viewport->height));

   assert(scissor->offset.x >= 0 && scissor->offset.y >= 0);
   miny = MAX2(scissor->offset.x, minx);
   miny = MAX2(scissor->offset.y, miny);
   maxx = MIN2(scissor->offset.x + scissor->extent.width, maxx);
   maxy = MIN2(scissor->offset.y + scissor->extent.height, maxy);

   /* Make sure we don't end up with a max < min when width/height is 0 */
   maxx = maxx > minx ? maxx - 1 : maxx;
   maxy = maxy > miny ? maxy - 1 : maxy;

   assert(viewport->minDepth >= 0.0f && viewport->minDepth <= 1.0f);
   assert(viewport->maxDepth >= 0.0f && viewport->maxDepth <= 1.0f);

   pan_pack(vpd, VIEWPORT, cfg) {
      cfg.scissor_minimum_x = minx;
      cfg.scissor_minimum_y = miny;
      cfg.scissor_maximum_x = maxx;
      cfg.scissor_maximum_y = maxy;
      cfg.minimum_z = MIN2(viewport->minDepth, viewport->maxDepth);
      cfg.maximum_z = MAX2(viewport->minDepth, viewport->maxDepth);
   }
}

void
panvk_sysval_upload_viewport_scale(const VkViewport *viewport,
                                   union panvk_sysval_data *data)
{
   data->f32[0] = 0.5f * viewport->width;
   data->f32[1] = 0.5f * viewport->height;
   data->f32[2] = 0.5f * (viewport->maxDepth - viewport->minDepth);
}

void
panvk_sysval_upload_viewport_offset(const VkViewport *viewport,
                                    union panvk_sysval_data *data)
{
   data->f32[0] = (0.5f * viewport->width) + viewport->x;
   data->f32[1] = (0.5f * viewport->height) + viewport->y;
   data->f32[2] = (0.5f * (viewport->maxDepth - viewport->minDepth)) + viewport->minDepth;
}

static enum mali_bifrost_register_file_format
bifrost_blend_type_from_nir(nir_alu_type nir_type)
{
   switch(nir_type) {
   case 0: /* Render target not in use */
      return 0;
   case nir_type_float16:
      return MALI_BIFROST_REGISTER_FILE_FORMAT_F16;
   case nir_type_float32:
      return MALI_BIFROST_REGISTER_FILE_FORMAT_F32;
   case nir_type_int32:
      return MALI_BIFROST_REGISTER_FILE_FORMAT_I32;
   case nir_type_uint32:
      return MALI_BIFROST_REGISTER_FILE_FORMAT_U32;
   case nir_type_int16:
      return MALI_BIFROST_REGISTER_FILE_FORMAT_I16;
   case nir_type_uint16:
      return MALI_BIFROST_REGISTER_FILE_FORMAT_U16;
   default:
      unreachable("Unsupported blend shader type for NIR alu type");
   }
}

static void
panvk_emit_bifrost_blend(const struct panvk_device *dev,
                         const struct panvk_pipeline *pipeline,
                         unsigned rt, void *bd)
{
   const struct pan_blend_state *blend = &pipeline->blend.state;
   const struct panfrost_device *pdev = &dev->physical_device->pdev;
   const struct pan_blend_rt_state *rts = &blend->rts[rt];

   pan_pack(bd, BLEND, cfg) {
      if (!blend->rt_count || !rts->equation.color_mask) {
         cfg.enable = false;
         cfg.bifrost.internal.mode = MALI_BIFROST_BLEND_MODE_OFF;
         continue;
      }

      cfg.srgb = util_format_is_srgb(rts->format);
      cfg.load_destination = pan_blend_reads_dest(blend->rts[rt].equation);
      cfg.round_to_fb_precision = true;

      const struct util_format_description *format_desc =
         util_format_description(rts->format);
      unsigned chan_size = 0;
      for (unsigned i = 0; i < format_desc->nr_channels; i++)
         chan_size = MAX2(format_desc->channel[0].size, chan_size);

      pan_blend_to_fixed_function_equation(blend->rts[rt].equation,
                                           &cfg.bifrost.equation);

      /* Fixed point constant */
      float fconst =
         pan_blend_get_constant(pan_blend_constant_mask(blend->rts[rt].equation),
                                blend->constants);
      u16 constant = fconst * ((1 << chan_size) - 1);
      constant <<= 16 - chan_size;
      cfg.bifrost.constant = constant;

      if (pan_blend_is_opaque(blend->rts[rt].equation))
         cfg.bifrost.internal.mode = MALI_BIFROST_BLEND_MODE_OPAQUE;
      else
         cfg.bifrost.internal.mode = MALI_BIFROST_BLEND_MODE_FIXED_FUNCTION;

      /* If we want the conversion to work properly,
       * num_comps must be set to 4
       */
      cfg.bifrost.internal.fixed_function.num_comps = 4;
      cfg.bifrost.internal.fixed_function.conversion.memory_format =
         panfrost_format_to_bifrost_blend(pdev, rts->format);
      cfg.bifrost.internal.fixed_function.conversion.register_format =
         bifrost_blend_type_from_nir(pipeline->fs.info.bifrost.blend[rt].type);
      cfg.bifrost.internal.fixed_function.rt = rt;
   }
}

static void
panvk_emit_midgard_blend(const struct panvk_device *dev,
                         const struct panvk_pipeline *pipeline,
                         unsigned rt, void *bd)
{
   const struct pan_blend_state *blend = &pipeline->blend.state;
   const struct pan_blend_rt_state *rts = &blend->rts[rt];

   pan_pack(bd, BLEND, cfg) {
      if (!blend->rt_count || !rts->equation.color_mask) {
         cfg.enable = false;
         continue;
      }

      cfg.srgb = util_format_is_srgb(rts->format);
      cfg.load_destination = pan_blend_reads_dest(blend->rts[rt].equation);
      cfg.round_to_fb_precision = true;
      cfg.midgard.blend_shader = false;
      pan_blend_to_fixed_function_equation(blend->rts[rt].equation,
                                           &cfg.midgard.equation);
      cfg.midgard.constant =
         pan_blend_get_constant(pan_blend_constant_mask(blend->rts[rt].equation),
                                blend->constants);
   }
}

void
panvk_emit_blend(const struct panvk_device *dev,
                 const struct panvk_pipeline *pipeline,
                 unsigned rt, void *bd)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;

   if (pan_is_bifrost(pdev))
      panvk_emit_bifrost_blend(dev, pipeline, rt, bd);
   else
      panvk_emit_midgard_blend(dev, pipeline, rt, bd);
}

void
panvk_emit_blend_constant(const struct panvk_device *dev,
                          const struct panvk_pipeline *pipeline,
                          unsigned rt, const float *constants, void *bd)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;
   float constant = constants[pipeline->blend.constant[rt].index];

   pan_pack(bd, BLEND, cfg) {
      cfg.enable = false;
      if (pan_is_bifrost(pdev)) {
         cfg.bifrost.constant = constant * pipeline->blend.constant[rt].bifrost_factor;
      } else {
         cfg.midgard.constant = constant;
      }
   }
}

void
panvk_emit_dyn_fs_rsd(const struct panvk_device *dev,
                      const struct panvk_pipeline *pipeline,
                      const struct panvk_cmd_state *state,
                      void *rsd)
{
   pan_pack(rsd, RENDERER_STATE, cfg) {
      if (pipeline->dynamic_state_mask & (1 << VK_DYNAMIC_STATE_DEPTH_BIAS)) {
         cfg.depth_units = state->rast.depth_bias.constant_factor * 2.0f;
         cfg.depth_factor = state->rast.depth_bias.slope_factor;
         cfg.depth_bias_clamp = state->rast.depth_bias.clamp;
      }

      if (pipeline->dynamic_state_mask & (1 << VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK)) {
         cfg.stencil_front.mask = state->zs.s_front.compare_mask;
         cfg.stencil_back.mask = state->zs.s_back.compare_mask;
      }

      if (pipeline->dynamic_state_mask & (1 << VK_DYNAMIC_STATE_STENCIL_WRITE_MASK)) {
         cfg.stencil_mask_misc.stencil_mask_front = state->zs.s_front.write_mask;
         cfg.stencil_mask_misc.stencil_mask_back = state->zs.s_back.write_mask;
      }

      if (pipeline->dynamic_state_mask & (1 << VK_DYNAMIC_STATE_STENCIL_REFERENCE)) {
         cfg.stencil_front.reference_value = state->zs.s_front.ref;
         cfg.stencil_back.reference_value = state->zs.s_back.ref;
      }
   }
}

void
panvk_emit_base_fs_rsd(const struct panvk_device *dev,
                       const struct panvk_pipeline *pipeline,
                       void *rsd)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;
   const struct pan_shader_info *info = &pipeline->fs.info;

   pan_pack(rsd, RENDERER_STATE, cfg) {
      if (pipeline->fs.required) {
         pan_shader_prepare_rsd(pdev, info, pipeline->fs.address, &cfg);
         if (pan_is_bifrost(pdev)) {
            cfg.properties.bifrost.allow_forward_pixel_to_kill = info->fs.can_fpk;
         } else {
            /* If either depth or stencil is enabled, discard matters */
            bool zs_enabled =
               (pipeline->zs.z_test && pipeline->zs.z_compare_func != MALI_FUNC_ALWAYS) ||
               pipeline->zs.s_test;

            cfg.properties.midgard.work_register_count = info->work_reg_count;
            cfg.properties.midgard.force_early_z =
               info->fs.can_early_z && !pipeline->ms.alpha_to_coverage &&
               pipeline->zs.z_compare_func == MALI_FUNC_ALWAYS;


            /* Workaround a hardware errata where early-z cannot be enabled
             * when discarding even when the depth buffer is read-only, by
             * lying to the hardware about the discard and setting the
             * reads tilebuffer? flag to compensate */
            cfg.properties.midgard.shader_reads_tilebuffer =
               info->fs.outputs_read ||
               (!zs_enabled && info->fs.can_discard);
            cfg.properties.midgard.shader_contains_discard =
               zs_enabled && info->fs.can_discard;
         }
      } else {
         if (pan_is_bifrost(pdev)) {
            cfg.properties.bifrost.shader_modifies_coverage = true;
            cfg.properties.bifrost.allow_forward_pixel_to_kill = true;
            cfg.properties.bifrost.allow_forward_pixel_to_be_killed = true;
            cfg.properties.bifrost.zs_update_operation = MALI_PIXEL_KILL_STRONG_EARLY;
         } else {
            cfg.shader.shader = 0x1;
            cfg.properties.midgard.work_register_count = 1;
            cfg.properties.depth_source = MALI_DEPTH_SOURCE_FIXED_FUNCTION;
            cfg.properties.midgard.force_early_z = true;
         }
      }

      bool msaa = pipeline->ms.rast_samples > 1;
      cfg.multisample_misc.multisample_enable = msaa;
      cfg.multisample_misc.sample_mask =
         msaa ? pipeline->ms.sample_mask : UINT16_MAX;

      cfg.multisample_misc.depth_function =
         pipeline->zs.z_test ? pipeline->zs.z_compare_func : MALI_FUNC_ALWAYS;

      cfg.multisample_misc.depth_write_mask = pipeline->zs.z_write;
      cfg.multisample_misc.fixed_function_near_discard = !pipeline->rast.clamp_depth;
      cfg.multisample_misc.fixed_function_far_discard = !pipeline->rast.clamp_depth;
      cfg.multisample_misc.shader_depth_range_fixed = true;

      cfg.stencil_mask_misc.stencil_enable = pipeline->zs.s_test;
      cfg.stencil_mask_misc.alpha_to_coverage = pipeline->ms.alpha_to_coverage;
      cfg.stencil_mask_misc.alpha_test_compare_function = MALI_FUNC_ALWAYS;
      cfg.stencil_mask_misc.depth_range_1 = pipeline->rast.depth_bias.enable;
      cfg.stencil_mask_misc.depth_range_2 = pipeline->rast.depth_bias.enable;
      cfg.stencil_mask_misc.single_sampled_lines = pipeline->ms.rast_samples <= 1;

      if (!(pipeline->dynamic_state_mask & (1 << VK_DYNAMIC_STATE_DEPTH_BIAS))) {
         cfg.depth_units = pipeline->rast.depth_bias.constant_factor * 2.0f;
         cfg.depth_factor = pipeline->rast.depth_bias.slope_factor;
         cfg.depth_bias_clamp = pipeline->rast.depth_bias.clamp;
      }

      if (!(pipeline->dynamic_state_mask & (1 << VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK))) {
         cfg.stencil_front.mask = pipeline->zs.s_front.compare_mask;
         cfg.stencil_back.mask = pipeline->zs.s_back.compare_mask;
      }

      if (!(pipeline->dynamic_state_mask & (1 << VK_DYNAMIC_STATE_STENCIL_WRITE_MASK))) {
         cfg.stencil_mask_misc.stencil_mask_front = pipeline->zs.s_front.write_mask;
         cfg.stencil_mask_misc.stencil_mask_back = pipeline->zs.s_back.write_mask;
      }

      if (!(pipeline->dynamic_state_mask & (1 << VK_DYNAMIC_STATE_STENCIL_REFERENCE))) {
         cfg.stencil_front.reference_value = pipeline->zs.s_front.ref;
         cfg.stencil_back.reference_value = pipeline->zs.s_back.ref;
      }

      cfg.stencil_front.compare_function = pipeline->zs.s_front.compare_func;
      cfg.stencil_front.stencil_fail = pipeline->zs.s_front.fail_op;
      cfg.stencil_front.depth_fail = pipeline->zs.s_front.z_fail_op;
      cfg.stencil_front.depth_pass = pipeline->zs.s_front.pass_op;
      cfg.stencil_back.compare_function = pipeline->zs.s_back.compare_func;
      cfg.stencil_back.stencil_fail = pipeline->zs.s_back.fail_op;
      cfg.stencil_back.depth_fail = pipeline->zs.s_back.z_fail_op;
      cfg.stencil_back.depth_pass = pipeline->zs.s_back.pass_op;
   }
}

void
panvk_emit_non_fs_rsd(const struct panvk_device *dev,
                      const struct pan_shader_info *shader_info,
                      mali_ptr shader_ptr,
                      void *rsd)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;

   assert(shader_info->stage != MESA_SHADER_FRAGMENT);

   pan_pack(rsd, RENDERER_STATE, cfg) {
      pan_shader_prepare_rsd(pdev, shader_info, shader_ptr, &cfg);
   }
}

void
panvk_emit_bifrost_tiler_context(const struct panvk_device *dev,
                                 unsigned width, unsigned height,
                                 const struct panfrost_ptr *descs)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;

   pan_pack(descs->cpu + MALI_BIFROST_TILER_LENGTH, BIFROST_TILER_HEAP, cfg) {
      cfg.size = pdev->tiler_heap->size;
      cfg.base = pdev->tiler_heap->ptr.gpu;
      cfg.bottom = pdev->tiler_heap->ptr.gpu;
      cfg.top = pdev->tiler_heap->ptr.gpu + pdev->tiler_heap->size;
   }

   pan_pack(descs->cpu, BIFROST_TILER, cfg) {
      cfg.hierarchy_mask = 0x28;
      cfg.fb_width = width;
      cfg.fb_height = height;
      cfg.heap = descs->gpu + MALI_BIFROST_TILER_LENGTH;
   }
}

unsigned
panvk_emit_fb(const struct panvk_device *dev,
              const struct panvk_batch *batch,
              const struct panvk_subpass *subpass,
              const struct panvk_pipeline *pipeline,
              const struct panvk_framebuffer *fb,
              const struct panvk_clear_value *clears,
              const struct pan_tls_info *tlsinfo,
              const struct pan_tiler_context *tilerctx,
              void *desc)
{
   const struct panfrost_device *pdev = &dev->physical_device->pdev;
   struct panvk_image_view *view;
   bool crc_valid[8] = { false };
   struct pan_fb_info fbinfo = {
      .width = fb->width,
      .height = fb->height,
      .extent.maxx = fb->width - 1,
      .extent.maxy = fb->height - 1,
      .nr_samples = 1,
   };

   for (unsigned cb = 0; cb < subpass->color_count; cb++) {
      int idx = subpass->color_attachments[cb].idx;
      view = idx != VK_ATTACHMENT_UNUSED ?
             fb->attachments[idx].iview : NULL;
      if (!view)
         continue;
      fbinfo.rts[cb].view = &view->pview;
      fbinfo.rts[cb].clear = subpass->color_attachments[idx].clear;
      fbinfo.rts[cb].crc_valid = &crc_valid[cb];

      memcpy(fbinfo.rts[cb].clear_value, clears[idx].color,
             sizeof(fbinfo.rts[cb].clear_value));
      fbinfo.nr_samples =
         MAX2(fbinfo.nr_samples, view->pview.image->layout.nr_samples);
   }

   if (subpass->zs_attachment.idx != VK_ATTACHMENT_UNUSED) {
      view = fb->attachments[subpass->zs_attachment.idx].iview;
      const struct util_format_description *fdesc =
         util_format_description(view->pview.format);

      fbinfo.nr_samples =
         MAX2(fbinfo.nr_samples, view->pview.image->layout.nr_samples);

      if (util_format_has_depth(fdesc)) {
         fbinfo.zs.clear.z = subpass->zs_attachment.clear;
         fbinfo.zs.clear_value.depth = clears[subpass->zs_attachment.idx].depth;
         fbinfo.zs.view.zs = &view->pview;
      }

      if (util_format_has_depth(fdesc)) {
         fbinfo.zs.clear.s = subpass->zs_attachment.clear;
         fbinfo.zs.clear_value.stencil = clears[subpass->zs_attachment.idx].depth;
         if (!fbinfo.zs.view.zs)
            fbinfo.zs.view.s = &view->pview;
      }
   }

   return pan_emit_fbd(pdev, &fbinfo, tlsinfo, tilerctx, desc);
}
