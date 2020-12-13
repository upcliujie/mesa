/*
 * Copyright 2018 Collabora Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "zink_context.h"

#include "zink_batch.h"
#include "zink_compiler.h"
#include "zink_fence.h"
#include "zink_framebuffer.h"
#include "zink_helpers.h"
#include "zink_program.h"
#include "zink_pipeline.h"
#include "zink_query.h"
#include "zink_render_pass.h"
#include "zink_resource.h"
#include "zink_screen.h"
#include "zink_state.h"
#include "zink_surface.h"

#include "indices/u_primconvert.h"
#include "util/u_blitter.h"
#include "util/u_debug.h"
#include "util/format_srgb.h"
#include "util/format/u_format.h"
#include "util/u_framebuffer.h"
#include "util/u_helpers.h"
#include "util/u_inlines.h"

#include "nir.h"

#include "util/u_memory.h"
#include "util/u_upload_mgr.h"

#define XXH_INLINE_ALL
#include "util/xxhash.h"

static void
incr_curr_batch(struct zink_context *ctx)
{
   ctx->curr_batch++;
   if (!ctx->curr_batch)
      ctx->curr_batch = 1;
}

static inline uint32_t
maybe_hash_u32(uint32_t val, uint32_t hash)
{
   if (!hash)
      return val;
   return XXH32(&val, sizeof(uint32_t), hash);
}

static struct zink_resource *
get_resource_for_descriptor(struct zink_context *ctx, enum zink_descriptor_type type, enum pipe_shader_type shader, int idx)
{
   switch (type) {
   case ZINK_DESCRIPTOR_TYPE_UBO:
      return zink_resource(ctx->ubos[shader][idx].buffer);
   case ZINK_DESCRIPTOR_TYPE_SSBO:
      return zink_resource(ctx->ssbos[shader][idx].buffer);
   case ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW:
      return zink_resource(ctx->sampler_views[shader][idx]->texture);
   case ZINK_DESCRIPTOR_TYPE_IMAGE:
      return zink_resource(ctx->image_views[shader][idx].base.resource);
   default:
      break;
   }
   unreachable("unknown descriptor type!");
   return NULL;
}

static uint32_t
calc_descriptor_state_hash_ubo(struct zink_context *ctx, struct zink_shader *zs, enum pipe_shader_type shader, int i, int idx, uint32_t hash)
{
   struct zink_resource *res = get_resource_for_descriptor(ctx, ZINK_DESCRIPTOR_TYPE_UBO, shader, idx);
   struct zink_resource_object *obj = res ? res->obj : NULL;
   hash = XXH32(&obj, sizeof(void*), hash);
   void *hash_data = &ctx->ubos[shader][idx].buffer_size;
   size_t data_size = sizeof(unsigned);
   hash = XXH32(hash_data, data_size, hash);
   if (zs->bindings[ZINK_DESCRIPTOR_TYPE_UBO][i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      hash = XXH32(&ctx->ubos[shader][idx].buffer_offset, sizeof(unsigned), hash);
   return hash;
}

static uint32_t
calc_descriptor_state_hash_ssbo(struct zink_context *ctx, struct zink_shader *zs, enum pipe_shader_type shader, int i, int idx, uint32_t hash)
{
   struct zink_resource *res = get_resource_for_descriptor(ctx, ZINK_DESCRIPTOR_TYPE_SSBO, shader, idx);
   struct zink_resource_object *obj = res ? res->obj : NULL;
   hash = XXH32(&obj, sizeof(void*), hash);
   if (obj) {
      struct pipe_shader_buffer *ssbo = &ctx->ssbos[shader][idx];
      hash = XXH32(&ssbo->buffer_offset, sizeof(ssbo->buffer_offset), hash);
      hash = XXH32(&ssbo->buffer_size, sizeof(ssbo->buffer_size), hash);
   }
   return hash;
}

static inline uint32_t
get_sampler_view_hash(const struct zink_sampler_view *sampler_view)
{
   if (!sampler_view)
      return 0;
   return sampler_view->base.target == PIPE_BUFFER ?
          sampler_view->buffer_view->hash : sampler_view->image_view->hash;
}

static inline uint32_t
get_image_view_hash(const struct zink_image_view *image_view)
{
   if (!image_view || !image_view->base.resource)
      return 0;
   return image_view->base.resource->target == PIPE_BUFFER ?
          image_view->buffer_view->hash : image_view->surface->hash;
}

uint32_t
zink_get_sampler_view_hash(struct zink_context *ctx, struct zink_sampler_view *sampler_view, bool is_buffer)
{
   return get_sampler_view_hash(sampler_view) ? get_sampler_view_hash(sampler_view) :
          (is_buffer ? zink_screen(ctx->base.screen)->null_descriptor_hashes.buffer_view :
                       zink_screen(ctx->base.screen)->null_descriptor_hashes.image_view);
}

uint32_t
zink_get_image_view_hash(struct zink_context *ctx, struct zink_image_view *image_view, bool is_buffer)
{
   return get_image_view_hash(image_view) ? get_image_view_hash(image_view) :
          (is_buffer ? zink_screen(ctx->base.screen)->null_descriptor_hashes.buffer_view :
                       zink_screen(ctx->base.screen)->null_descriptor_hashes.image_view);
}

static uint32_t
calc_descriptor_state_hash_sampler(struct zink_context *ctx, struct zink_shader *zs, enum pipe_shader_type shader, int i, int idx, uint32_t hash)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);

   for (unsigned k = 0; k < zs->bindings[ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW][i].size; k++) {
      struct zink_sampler_view *sampler_view = zink_sampler_view(ctx->sampler_views[shader][idx + k]);
      bool is_buffer = zink_shader_descriptor_is_buffer(zs, ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW, i);
      hash = maybe_hash_u32(zink_get_sampler_view_hash(ctx, sampler_view, is_buffer), hash);
      if (is_buffer)
         continue;

      struct zink_sampler_state *sampler_state = ctx->sampler_states[shader][idx + k];
      struct zink_resource *res = get_resource_for_descriptor(ctx, ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW, shader, idx + k);

      if (sampler_state) {
         VkFormatProperties props = screen->format_props[res->base.b.format];
         bool can_linear = (res->optimal_tiling && props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ||
                           (!res->optimal_tiling && props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
         struct zink_sampler *sampler;
         if (can_linear)
            sampler = sampler_state->samplers[0];
         else
            sampler = sampler_state->samplers[1] ? sampler_state->samplers[1] : sampler_state->samplers[0];
         hash = XXH32(&sampler, sizeof(void*), hash);
      }
   }
   return hash;
}

static uint32_t
calc_descriptor_state_hash_image(struct zink_context *ctx, struct zink_shader *zs, enum pipe_shader_type shader, int i, int idx, uint32_t hash)
{
   for (unsigned k = 0; k < zs->bindings[ZINK_DESCRIPTOR_TYPE_IMAGE][i].size; k++) {
      hash = maybe_hash_u32(zink_get_image_view_hash(ctx, &ctx->image_views[shader][idx + k],
                                                     zink_shader_descriptor_is_buffer(zs, ZINK_DESCRIPTOR_TYPE_IMAGE, i)),
                            hash);
   }
   return hash;
}

static uint32_t
update_descriptor_stage_state(struct zink_context *ctx, enum pipe_shader_type shader, enum zink_descriptor_type type)
{
   struct zink_shader *zs = shader == PIPE_SHADER_COMPUTE ? ctx->compute_stage : ctx->gfx_stages[shader];

   if (!zink_program_get_descriptor_usage(ctx, shader, type))
      return 0;

   uint32_t hash = 0;
   for (int i = 0; i < zs->num_bindings[type]; i++) {
      int idx = zs->bindings[type][i].index;
      switch (type) {
      case ZINK_DESCRIPTOR_TYPE_UBO:
         hash = calc_descriptor_state_hash_ubo(ctx, zs, shader, i, idx, hash);
         break;
      case ZINK_DESCRIPTOR_TYPE_SSBO:
         hash = calc_descriptor_state_hash_ssbo(ctx, zs, shader, i, idx, hash);
         break;
      case ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW:
         hash = calc_descriptor_state_hash_sampler(ctx, zs, shader, i, idx, hash);
         break;
      case ZINK_DESCRIPTOR_TYPE_IMAGE:
         hash = calc_descriptor_state_hash_image(ctx, zs, shader, i, idx, hash);
         break;
      default:
         unreachable("unknown descriptor type");
      }
   }
   return hash;
}

static void
update_descriptor_state(struct zink_context *ctx, enum zink_descriptor_type type, bool is_compute)
{
   /* we shouldn't be calling this if we don't have to */
   assert(!ctx->descriptor_states[is_compute].state[type]);

   if (is_compute)
      /* just update compute state */
      ctx->descriptor_states[is_compute].state[type] = update_descriptor_stage_state(ctx, PIPE_SHADER_COMPUTE, type);
   else {
      /* update all gfx states */
      for (unsigned i = 0; i < ZINK_SHADER_COUNT; i++) {
         /* this is the incremental update for the shader stage */
         if (!ctx->gfx_descriptor_states[i].state[type])
            ctx->gfx_descriptor_states[i].state[type] = update_descriptor_stage_state(ctx, i, type);
         if (ctx->gfx_descriptor_states[i].state[type])
            /* this is the overall state update for the descriptor set hash */
            ctx->descriptor_states[is_compute].state[type] = maybe_hash_u32(ctx->gfx_descriptor_states[i].state[type], ctx->descriptor_states[is_compute].state[type]);
      }
   }
}

void
zink_context_update_descriptor_states(struct zink_context *ctx, bool is_compute)
{
   for (unsigned i = 0; i < ZINK_DESCRIPTOR_TYPES; i++) {
      if (!ctx->descriptor_states[is_compute].state[i])
         update_descriptor_state(ctx, i, is_compute);
   }
}

static void
invalidate_descriptor_state(struct zink_context *ctx, enum pipe_shader_type shader, enum zink_descriptor_type type)
{
   ctx->gfx_descriptor_states[shader].state[type] = 0;
   ctx->descriptor_states[shader == PIPE_SHADER_COMPUTE].state[type] = 0;
}

void
debug_describe_zink_sampler(char *buf, const struct zink_sampler *ptr)
{
   sprintf(buf, "zink_sampler");
}

void
debug_describe_zink_buffer_view(char *buf, const struct zink_buffer_view *ptr)
{
   sprintf(buf, "zink_buffer_view");
}

static void
zink_context_destroy(struct pipe_context *pctx)
{
   struct zink_context *ctx = zink_context(pctx);

   if (ctx->batch.queue && vkQueueWaitIdle(ctx->batch.queue) != VK_SUCCESS)
      debug_printf("vkQueueWaitIdle failed\n");

   if (ctx->tc)
      util_queue_destroy(&ctx->batch.flush_queue);

   for (unsigned i = 0; i < ARRAY_SIZE(ctx->null_buffers); i++)
      pipe_resource_reference(&ctx->null_buffers[i], NULL);

   zink_clear_batch_state(ctx, ctx->batch.state);
   zink_batch_state_reference(zink_screen(pctx->screen), &ctx->batch.state, NULL);
   hash_table_foreach(&ctx->batch_states, entry) {
      struct zink_batch_state *bs = entry->data;
      zink_clear_batch_state(ctx, bs);
      zink_batch_state_reference(zink_screen(pctx->screen), &bs, NULL);
   }
   util_dynarray_foreach(&ctx->free_batch_states, struct zink_batch_state*, bs) {
      zink_clear_batch_state(ctx, *bs);
      zink_batch_state_reference(zink_screen(pctx->screen), bs, NULL);
   }

   hash_table_foreach(&ctx->surface_cache, entry) {
      struct pipe_surface* sf = (struct pipe_surface*)entry->data;
      pipe_resource_reference(&sf->texture, NULL);
      pipe_surface_reference(&sf, NULL);
   }

   util_primconvert_destroy(ctx->primconvert);
   u_upload_destroy(pctx->stream_uploader);
   slab_destroy_child(&ctx->transfer_pool);
   slab_destroy_child(&ctx->transfer_pool_unsync);
   util_blitter_destroy(ctx->blitter);

   zink_descriptor_pool_deinit(ctx);

   simple_mtx_destroy(&ctx->surface_mtx);
   simple_mtx_destroy(&ctx->bufferview_mtx);

   ralloc_free(ctx);
}

static enum pipe_reset_status
zink_get_device_reset_status(struct pipe_context *pctx)
{
   struct zink_context *ctx = zink_context(pctx);

   enum pipe_reset_status status = PIPE_NO_RESET;

   if (ctx->is_device_lost) {
      // Since we don't know what really happened to the hardware, just
      // assume that we are in the wrong
      status = PIPE_GUILTY_CONTEXT_RESET;

      debug_printf("ZINK: device lost detected!\n");

      if (ctx->reset.reset)
         ctx->reset.reset(ctx->reset.data, status);
   }

   return status;
}

static void
zink_set_device_reset_callback(struct pipe_context *pctx,
                               const struct pipe_device_reset_callback *cb)
{
   struct zink_context *ctx = zink_context(pctx);

   if (cb)
      ctx->reset = *cb;
   else
      memset(&ctx->reset, 0, sizeof(ctx->reset));
}

static VkSamplerMipmapMode
sampler_mipmap_mode(enum pipe_tex_mipfilter filter)
{
   switch (filter) {
   case PIPE_TEX_MIPFILTER_NEAREST: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
   case PIPE_TEX_MIPFILTER_LINEAR: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
   case PIPE_TEX_MIPFILTER_NONE:
      unreachable("PIPE_TEX_MIPFILTER_NONE should be dealt with earlier");
   }
   unreachable("unexpected filter");
}

static VkSamplerAddressMode
sampler_address_mode(enum pipe_tex_wrap filter)
{
   switch (filter) {
   case PIPE_TEX_WRAP_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
   case PIPE_TEX_WRAP_CLAMP: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; /* not technically correct, but kinda works */
   case PIPE_TEX_WRAP_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   case PIPE_TEX_WRAP_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
   case PIPE_TEX_WRAP_MIRROR_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
   case PIPE_TEX_WRAP_MIRROR_CLAMP: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE; /* not technically correct, but kinda works */
   case PIPE_TEX_WRAP_MIRROR_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
   case PIPE_TEX_WRAP_MIRROR_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE; /* not technically correct, but kinda works */
   }
   unreachable("unexpected wrap");
}

static VkCompareOp
compare_op(enum pipe_compare_func op)
{
   switch (op) {
      case PIPE_FUNC_NEVER: return VK_COMPARE_OP_NEVER;
      case PIPE_FUNC_LESS: return VK_COMPARE_OP_LESS;
      case PIPE_FUNC_EQUAL: return VK_COMPARE_OP_EQUAL;
      case PIPE_FUNC_LEQUAL: return VK_COMPARE_OP_LESS_OR_EQUAL;
      case PIPE_FUNC_GREATER: return VK_COMPARE_OP_GREATER;
      case PIPE_FUNC_NOTEQUAL: return VK_COMPARE_OP_NOT_EQUAL;
      case PIPE_FUNC_GEQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
      case PIPE_FUNC_ALWAYS: return VK_COMPARE_OP_ALWAYS;
   }
   unreachable("unexpected compare");
}

static inline bool
wrap_needs_border_color(unsigned wrap)
{
   return wrap == PIPE_TEX_WRAP_CLAMP || wrap == PIPE_TEX_WRAP_CLAMP_TO_BORDER ||
          wrap == PIPE_TEX_WRAP_MIRROR_CLAMP || wrap == PIPE_TEX_WRAP_MIRROR_CLAMP_TO_BORDER;
}

static VkSampler
create_sampler(struct pipe_context *pctx, const struct pipe_sampler_state *state, bool *custom_border_color)
{
   struct zink_screen *screen = zink_screen(pctx->screen);
   bool need_custom = false;
   VkSampler sampler = VK_NULL_HANDLE;

   VkSamplerCreateInfo sci = {};
   VkSamplerCustomBorderColorCreateInfoEXT cbci = {};
   sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   sci.magFilter = zink_filter(state->mag_img_filter);
   sci.minFilter = zink_filter(state->min_img_filter);

   if (state->min_mip_filter != PIPE_TEX_MIPFILTER_NONE) {
      sci.mipmapMode = sampler_mipmap_mode(state->min_mip_filter);
      sci.minLod = state->min_lod;
      sci.maxLod = state->max_lod;
   } else {
      sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
      sci.minLod = 0;
      sci.maxLod = 0;
   }

   sci.addressModeU = sampler_address_mode(state->wrap_s);
   sci.addressModeV = sampler_address_mode(state->wrap_t);
   sci.addressModeW = sampler_address_mode(state->wrap_r);
   sci.mipLodBias = state->lod_bias;

   need_custom |= wrap_needs_border_color(state->wrap_s);
   need_custom |= wrap_needs_border_color(state->wrap_t);
   need_custom |= wrap_needs_border_color(state->wrap_r);

   if (state->compare_mode == PIPE_TEX_COMPARE_NONE)
      sci.compareOp = VK_COMPARE_OP_NEVER;
   else {
      sci.compareOp = compare_op(state->compare_func);
      sci.compareEnable = VK_TRUE;
   }

   if (screen->info.have_EXT_custom_border_color &&
       screen->info.border_color_feats.customBorderColorWithoutFormat && need_custom) {
      cbci.sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT;
      cbci.format = VK_FORMAT_UNDEFINED;
      /* these are identical unions */
      memcpy(&cbci.customBorderColor, &state->border_color, sizeof(union pipe_color_union));
      sci.pNext = &cbci;
      sci.borderColor = VK_BORDER_COLOR_INT_CUSTOM_EXT;
      UNUSED uint32_t check = p_atomic_inc_return(&screen->cur_custom_border_color_samplers);
      assert(check <= screen->info.border_color_props.maxCustomBorderColorSamplers);
      *custom_border_color = true;
   } else
      sci.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK; // TODO with custom shader if we're super interested?
   sci.unnormalizedCoordinates = !state->normalized_coords;

   if (state->max_anisotropy > 1) {
      sci.maxAnisotropy = state->max_anisotropy;
      sci.anisotropyEnable = VK_TRUE;
   }
   vkCreateSampler(screen->dev, &sci, NULL, &sampler);
   return sampler;
}

static struct zink_sampler *
get_sampler(struct pipe_context *pctx, const struct pipe_sampler_state *state)
{
   struct zink_sampler *sampler = ralloc(NULL, struct zink_sampler);
   if (!sampler)
      return NULL;
   pipe_reference_init(&sampler->reference, 1);
   sampler->sampler = create_sampler(pctx, state, &sampler->custom_border_color);
   sampler->custom_border_color = false;
   util_dynarray_init(&sampler->desc_set_refs.refs, NULL);
   if (!sampler->sampler) {
       ralloc_free(sampler);
       return NULL;
   }
   return sampler;
}

static void
zink_delete_sampler_state(struct pipe_context *pctx,
                          void *sampler_state)
{
   struct zink_sampler_state *sampler = sampler_state;
   zink_sampler_reference(zink_context(pctx), &sampler->samplers[0], NULL);
   zink_sampler_reference(zink_context(pctx), &sampler->samplers[1], NULL);
   free(sampler_state);
}

static void *
zink_create_sampler_state(struct pipe_context *pctx,
                          const struct pipe_sampler_state *state)
{
   struct zink_sampler_state *sampler_state = CALLOC(1, sizeof(struct zink_sampler_state));
   if (!sampler_state)
      return NULL;

   sampler_state->samplers[0] = get_sampler(pctx, state);
   if (!sampler_state->samplers[0]) {
      FREE(sampler_state);
      return NULL;
   }
   /* If filter is VK_FILTER_LINEAR, then the format features of srcImage
    * must contain VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
    *
    * we need 2 samplers to ensure this is possible
    */
   if (state->mag_img_filter == PIPE_TEX_FILTER_LINEAR ||
       state->min_img_filter == PIPE_TEX_FILTER_LINEAR ||
       state->min_mip_filter == PIPE_TEX_MIPFILTER_LINEAR) {
        struct pipe_sampler_state nearest = *state;
        nearest.mag_img_filter = nearest.min_img_filter = PIPE_TEX_FILTER_NEAREST;
        if (state->min_mip_filter != PIPE_TEX_MIPFILTER_NONE)
           nearest.min_mip_filter = PIPE_TEX_MIPFILTER_NEAREST;

      sampler_state->samplers[1] = get_sampler(pctx, &nearest);
      if (!sampler_state->samplers[1]) {
         zink_delete_sampler_state(pctx, sampler_state);
         return NULL;
      }
   }

   return sampler_state;
}

static void
zink_bind_sampler_states(struct pipe_context *pctx,
                         enum pipe_shader_type shader,
                         unsigned start_slot,
                         unsigned num_samplers,
                         void **samplers)
{
   struct zink_context *ctx = zink_context(pctx);
   uint32_t usage = zink_program_get_descriptor_usage(ctx, shader, ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW);
   bool update = false;
   for (unsigned i = 0; i < num_samplers; ++i) {
      struct zink_sampler_state *a = ctx->sampler_states[shader][start_slot + i];
      struct zink_sampler_state *b = samplers[i];
      if (usage & BITFIELD64_BIT(start_slot + i))
         update |= !!a != !!b ||
                   (a && ((a->samplers[0] != b->samplers[0]) ||
                          (a->samplers[1] != b->samplers[1])));
      ctx->sampler_states[shader][start_slot + i] = samplers[i];
   }
   ctx->num_samplers[shader] = start_slot + num_samplers;
   if (update)
      invalidate_descriptor_state(ctx, shader, ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW);
}

void
zink_destroy_sampler(struct zink_context *ctx, struct zink_sampler *sampler)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   zink_descriptor_set_refs_clear(&sampler->desc_set_refs, sampler);
   if (sampler->custom_border_color)
      p_atomic_dec(&screen->cur_custom_border_color_samplers);
   vkDestroySampler(screen->dev, sampler->sampler, NULL);
   ralloc_free(sampler);
}

static VkImageViewType
image_view_type(enum pipe_texture_target target)
{
   switch (target) {
   case PIPE_TEXTURE_1D: return VK_IMAGE_VIEW_TYPE_1D;
   case PIPE_TEXTURE_1D_ARRAY: return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
   case PIPE_TEXTURE_2D: return VK_IMAGE_VIEW_TYPE_2D;
   case PIPE_TEXTURE_2D_ARRAY: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
   case PIPE_TEXTURE_CUBE: return VK_IMAGE_VIEW_TYPE_CUBE;
   case PIPE_TEXTURE_CUBE_ARRAY: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
   case PIPE_TEXTURE_3D: return VK_IMAGE_VIEW_TYPE_3D;
   case PIPE_TEXTURE_RECT: return VK_IMAGE_VIEW_TYPE_2D;
   default:
      unreachable("unexpected target");
   }
}

static VkComponentSwizzle
component_mapping(enum pipe_swizzle swizzle)
{
   switch (swizzle) {
   case PIPE_SWIZZLE_X: return VK_COMPONENT_SWIZZLE_R;
   case PIPE_SWIZZLE_Y: return VK_COMPONENT_SWIZZLE_G;
   case PIPE_SWIZZLE_Z: return VK_COMPONENT_SWIZZLE_B;
   case PIPE_SWIZZLE_W: return VK_COMPONENT_SWIZZLE_A;
   case PIPE_SWIZZLE_0: return VK_COMPONENT_SWIZZLE_ZERO;
   case PIPE_SWIZZLE_1: return VK_COMPONENT_SWIZZLE_ONE;
   case PIPE_SWIZZLE_NONE: return VK_COMPONENT_SWIZZLE_IDENTITY; // ???
   default:
      unreachable("unexpected swizzle");
   }
}

static VkImageAspectFlags
sampler_aspect_from_format(enum pipe_format fmt)
{
   if (util_format_is_depth_or_stencil(fmt)) {
      const struct util_format_description *desc = util_format_description(fmt);
      if (util_format_has_depth(desc))
         return VK_IMAGE_ASPECT_DEPTH_BIT;
      assert(util_format_has_stencil(desc));
      return VK_IMAGE_ASPECT_STENCIL_BIT;
   } else
     return VK_IMAGE_ASPECT_COLOR_BIT;
}

static uint32_t
hash_bufferview(void *bvci)
{
   size_t offset = offsetof(VkBufferViewCreateInfo, flags);
   return _mesa_hash_data((char*)bvci + offset, sizeof(VkBufferViewCreateInfo) - offset);
}

static struct zink_buffer_view *
get_buffer_view(struct zink_context *ctx, struct zink_resource *res, enum pipe_format format, uint32_t offset, uint32_t range)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   struct zink_buffer_view *buffer_view;
   VkBufferViewCreateInfo bvci = {};
   bvci.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
   if (res->bind_history & BITFIELD64_BIT(ZINK_DESCRIPTOR_TYPE_IMAGE))
      bvci.buffer = res->obj->sbuffer;
   if (!bvci.buffer)
      bvci.buffer = res->obj->buffer;
   bvci.format = zink_get_format(screen, format);
   assert(bvci.format);
   bvci.offset = offset;
   bvci.range = range;

   uint32_t hash = hash_bufferview(&bvci);
   simple_mtx_lock(&ctx->bufferview_mtx);
   struct hash_entry *he = _mesa_hash_table_search_pre_hashed(&ctx->bufferview_cache, hash, &bvci);
   simple_mtx_unlock(&ctx->bufferview_mtx);
   if (he) {
      buffer_view = he->data;
      p_atomic_inc(&buffer_view->reference.count);
   } else {
      VkBufferView view;
      if (vkCreateBufferView(screen->dev, &bvci, NULL, &view) != VK_SUCCESS)
         return NULL;
      buffer_view = CALLOC_STRUCT(zink_buffer_view);
      if (!buffer_view) {
         vkDestroyBufferView(screen->dev, view, NULL);
         return NULL;
      }
      pipe_reference_init(&buffer_view->reference, 1);
      buffer_view->bvci = bvci;
      buffer_view->buffer_view = view;
      buffer_view->hash = hash;
      simple_mtx_lock(&ctx->bufferview_mtx);
      _mesa_hash_table_insert_pre_hashed(&ctx->bufferview_cache, hash, &buffer_view->bvci, buffer_view);
      simple_mtx_unlock(&ctx->bufferview_mtx);
   }
   return buffer_view;
}

static struct pipe_sampler_view *
zink_create_sampler_view(struct pipe_context *pctx, struct pipe_resource *pres,
                         const struct pipe_sampler_view *state)
{
   struct zink_screen *screen = zink_screen(pctx->screen);
   struct zink_resource *res = zink_resource(pres);
   struct zink_sampler_view *sampler_view = CALLOC_STRUCT(zink_sampler_view);
   bool err;

   sampler_view->base = *state;
   sampler_view->base.texture = NULL;
   pipe_resource_reference(&sampler_view->base.texture, pres);
   sampler_view->base.reference.count = 1;
   sampler_view->base.context = pctx;

   if (state->target != PIPE_BUFFER) {
      VkImageViewCreateInfo ivci = {};
      ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      ivci.image = res->obj->image;
      ivci.viewType = image_view_type(state->target);

      ivci.components.r = component_mapping(state->swizzle_r);
      ivci.components.g = component_mapping(state->swizzle_g);
      ivci.components.b = component_mapping(state->swizzle_b);
      ivci.components.a = component_mapping(state->swizzle_a);
      ivci.subresourceRange.aspectMask = sampler_aspect_from_format(state->format);
      /* samplers for stencil aspects of packed formats need to always use stencil type */
      if (ivci.subresourceRange.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT) {
         ivci.format = VK_FORMAT_S8_UINT;
         ivci.components.g = VK_COMPONENT_SWIZZLE_R;
      } else {
         ivci.format = zink_get_format(screen, state->format);
         /* if we have e.g., R8G8B8X8, then we have to ignore alpha since we're just emulating
          * these formats
          */
         const struct util_format_description *desc = util_format_description(state->format);
         if (ivci.subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT &&
             desc->layout == UTIL_FORMAT_LAYOUT_PLAIN) {
            VkComponentSwizzle *swizz = &ivci.components.r;
            for (unsigned i = 0; i < desc->nr_channels; i++) {
               if (desc->channel[i].type == UTIL_FORMAT_TYPE_VOID)
                  swizz[i] = VK_COMPONENT_SWIZZLE_ONE;
            }
         }
      }
      assert(ivci.format);

      ivci.subresourceRange.baseMipLevel = state->u.tex.first_level;
      ivci.subresourceRange.levelCount = 1;
      ivci.subresourceRange.baseArrayLayer = state->u.tex.first_layer;
      ivci.subresourceRange.levelCount = state->u.tex.last_level - state->u.tex.first_level + 1;
      ivci.subresourceRange.layerCount = state->u.tex.last_layer - state->u.tex.first_layer + 1;
      if (pres->target == PIPE_TEXTURE_CUBE ||
          pres->target == PIPE_TEXTURE_CUBE_ARRAY) {
         if (ivci.subresourceRange.layerCount != 6)
            ivci.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
      }

      struct pipe_surface templ = {};
      templ.u.tex.level = state->u.tex.first_level;
      templ.format = state->format;
      templ.u.tex.first_layer = state->u.tex.first_layer;
      templ.u.tex.last_layer = state->u.tex.last_layer;
      sampler_view->image_view = (struct zink_surface*)zink_get_surface(zink_context(pctx), pres, &templ, &ivci);
      err = !sampler_view->image_view;
   } else {
      sampler_view->buffer_view = get_buffer_view(zink_context(pctx), res, state->format, state->u.buf.offset, state->u.buf.size);
      err = !sampler_view->buffer_view;
   }
   if (err) {
      FREE(sampler_view);
      return NULL;
   }
   util_dynarray_init(&sampler_view->desc_set_refs.refs, NULL);
   return &sampler_view->base;
}

void
zink_destroy_buffer_view(struct zink_context *ctx, struct zink_buffer_view *buffer_view)
{
   simple_mtx_lock(&ctx->bufferview_mtx);
   struct hash_entry *he = _mesa_hash_table_search_pre_hashed(&ctx->bufferview_cache, buffer_view->hash, &buffer_view->bvci);
   assert(he);
   _mesa_hash_table_remove(&ctx->bufferview_cache, he);
   simple_mtx_unlock(&ctx->bufferview_mtx);
   vkDestroyBufferView(zink_screen(ctx->base.screen)->dev, buffer_view->buffer_view, NULL);
   FREE(buffer_view);
}

static void
zink_sampler_view_destroy(struct pipe_context *pctx,
                          struct pipe_sampler_view *pview)
{
   struct zink_sampler_view *view = zink_sampler_view(pview);
   zink_descriptor_set_refs_clear(&view->desc_set_refs, view);
   if (pview->texture->target == PIPE_BUFFER)
      zink_buffer_view_reference(zink_context(pctx), &view->buffer_view, NULL);
   else {
      struct pipe_surface *psurf = &view->image_view->base;
      pipe_surface_reference(&psurf, NULL);
   }
   pipe_resource_reference(&pview->texture, NULL);
   FREE(view);
}

static void
zink_get_sample_position(struct pipe_context *ctx,
                         unsigned sample_count,
                         unsigned sample_index,
                         float *out_value)
{
   /* TODO: handle this I guess */
   assert(zink_screen(ctx->screen)->info.props.limits.standardSampleLocations);
   /* from 26.4. Multisampling */
   switch (sample_count) {
   case 0:
   case 1: {
      float pos[][2] = { {0.5,0.5}, };
      out_value[0] = pos[sample_index][0];
      out_value[1] = pos[sample_index][1];
      break;
   }
   case 2: {
      float pos[][2] = { {0.75,0.75},
                        {0.25,0.25}, };
      out_value[0] = pos[sample_index][0];
      out_value[1] = pos[sample_index][1];
      break;
   }
   case 4: {
      float pos[][2] = { {0.375, 0.125},
                        {0.875, 0.375},
                        {0.125, 0.625},
                        {0.625, 0.875}, };
      out_value[0] = pos[sample_index][0];
      out_value[1] = pos[sample_index][1];
      break;
   }
   case 8: {
      float pos[][2] = { {0.5625, 0.3125},
                        {0.4375, 0.6875},
                        {0.8125, 0.5625},
                        {0.3125, 0.1875},
                        {0.1875, 0.8125},
                        {0.0625, 0.4375},
                        {0.6875, 0.9375},
                        {0.9375, 0.0625}, };
      out_value[0] = pos[sample_index][0];
      out_value[1] = pos[sample_index][1];
      break;
   }
   case 16: {
      float pos[][2] = { {0.5625, 0.5625},
                        {0.4375, 0.3125},
                        {0.3125, 0.625},
                        {0.75, 0.4375},
                        {0.1875, 0.375},
                        {0.625, 0.8125},
                        {0.8125, 0.6875},
                        {0.6875, 0.1875},
                        {0.375, 0.875},
                        {0.5, 0.0625},
                        {0.25, 0.125},
                        {0.125, 0.75},
                        {0.0, 0.5},
                        {0.9375, 0.25},
                        {0.875, 0.9375},
                        {0.0625, 0.0}, };
      out_value[0] = pos[sample_index][0];
      out_value[1] = pos[sample_index][1];
      break;
   }
   default:
      unreachable("unhandled sample count!");
   }
}

static void
zink_set_polygon_stipple(struct pipe_context *pctx,
                         const struct pipe_poly_stipple *ps)
{
}

static void
zink_set_vertex_buffers(struct pipe_context *pctx,
                        unsigned start_slot,
                        unsigned num_buffers,
                        const struct pipe_vertex_buffer *buffers)
{
   struct zink_context *ctx = zink_context(pctx);

   if (buffers && !zink_screen(pctx->screen)->info.have_EXT_extended_dynamic_state) {
      for (int i = 0; i < num_buffers; ++i) {
         const struct pipe_vertex_buffer *vb = buffers + i;
         if (ctx->gfx_pipeline_state.bindings[start_slot + i].stride != vb->stride)
            ctx->gfx_pipeline_state.hash = 0;
         ctx->gfx_pipeline_state.bindings[start_slot + i].stride = vb->stride;
      }
   }

   util_set_vertex_buffers_mask(ctx->vertex_buffers, &ctx->gfx_pipeline_state.vertex_buffers_enabled_mask,
                                buffers, start_slot, num_buffers);
}

static void
zink_set_viewport_states(struct pipe_context *pctx,
                         unsigned start_slot,
                         unsigned num_viewports,
                         const struct pipe_viewport_state *state)
{
   struct zink_context *ctx = zink_context(pctx);

   for (unsigned i = 0; i < num_viewports; ++i)
      ctx->vp_state.viewport_states[start_slot + i] = state[i];
   ctx->vp_state.num_viewports = start_slot + num_viewports;

   if (!zink_screen(pctx->screen)->info.have_EXT_extended_dynamic_state) {
      if (ctx->gfx_pipeline_state.num_viewports != ctx->vp_state.num_viewports)
         ctx->gfx_pipeline_state.hash = 0;
      ctx->gfx_pipeline_state.num_viewports = ctx->vp_state.num_viewports;
   }
}

static void
zink_set_scissor_states(struct pipe_context *pctx,
                        unsigned start_slot, unsigned num_scissors,
                        const struct pipe_scissor_state *states)
{
   struct zink_context *ctx = zink_context(pctx);

   for (unsigned i = 0; i < num_scissors; i++)
      ctx->vp_state.scissor_states[start_slot + i] = states[i];
}

static void
zink_set_constant_buffer(struct pipe_context *pctx,
                         enum pipe_shader_type shader, uint index,
                         const struct pipe_constant_buffer *cb)
{
   struct zink_context *ctx = zink_context(pctx);
   uint32_t usage = zink_program_get_descriptor_usage(ctx, shader, ZINK_DESCRIPTOR_TYPE_UBO);
   bool update = false;

   if (cb) {
      struct pipe_resource *buffer = cb->buffer;
      unsigned offset = cb->buffer_offset;
      if (cb->user_buffer) {
         struct zink_screen *screen = zink_screen(pctx->screen);
         u_upload_data(ctx->base.const_uploader, 0, cb->buffer_size,
                       screen->info.props.limits.minUniformBufferOffsetAlignment,
                       cb->user_buffer, &offset, &buffer);
      }
      struct zink_resource *res = zink_resource(ctx->ubos[shader][index].buffer);
      struct zink_resource *new_res = zink_resource(buffer);
      if (new_res) {
         new_res->bind_history |= BITFIELD64_BIT(ZINK_DESCRIPTOR_TYPE_UBO);
         new_res->bind_stages |= 1 << shader;
      }
      update |= (index && ctx->ubos[shader][index].buffer_offset != offset) ||
                !!res != !!buffer || (res && res->obj->buffer != new_res->obj->buffer) ||
                ctx->ubos[shader][index].buffer_size != cb->buffer_size;

      pipe_resource_reference(&ctx->ubos[shader][index].buffer, buffer);
      ctx->ubos[shader][index].buffer_offset = offset;
      ctx->ubos[shader][index].buffer_size = cb->buffer_size;
      ctx->ubos[shader][index].user_buffer = NULL;

      if (cb->user_buffer)
         pipe_resource_reference(&buffer, NULL);
   } else {
      pipe_resource_reference(&ctx->ubos[shader][index].buffer, NULL);
      ctx->ubos[shader][index].buffer_offset = 0;
      ctx->ubos[shader][index].buffer_size = 0;
      ctx->ubos[shader][index].user_buffer = NULL;

      update = (usage & BITFIELD64_BIT(index));
   }
   if (update)
      invalidate_descriptor_state(ctx, shader, ZINK_DESCRIPTOR_TYPE_UBO);
}

static void
zink_set_shader_buffers(struct pipe_context *pctx,
                        enum pipe_shader_type p_stage,
                        unsigned start_slot, unsigned count,
                        const struct pipe_shader_buffer *buffers,
                        unsigned writable_bitmask)
{
   struct zink_context *ctx = zink_context(pctx);
   uint32_t usage = zink_program_get_descriptor_usage(ctx, p_stage, ZINK_DESCRIPTOR_TYPE_SSBO);
   bool update = false;

   unsigned modified_bits = u_bit_consecutive(start_slot, count);
   ctx->writable_ssbos &= ~modified_bits;
   ctx->writable_ssbos |= writable_bitmask << start_slot;

   for (unsigned i = 0; i < count; i++) {
      struct pipe_shader_buffer *ssbo = &ctx->ssbos[p_stage][start_slot + i];
      if (buffers && buffers[i].buffer) {
         struct zink_resource *res = (void *) buffers[i].buffer;
         res->bind_history |= BITFIELD64_BIT(ZINK_DESCRIPTOR_TYPE_SSBO);
         res->bind_stages |= 1 << p_stage;
         pipe_resource_reference(&ssbo->buffer, &res->base.b);
         ssbo->buffer_offset = buffers[i].buffer_offset;
         ssbo->buffer_size = MIN2(buffers[i].buffer_size, res->obj->size - ssbo->buffer_offset);
         util_range_add(&res->base.b, &res->valid_buffer_range, ssbo->buffer_offset,
                        ssbo->buffer_offset + ssbo->buffer_size);
         update = true;
      } else {
         pipe_resource_reference(&ssbo->buffer, NULL);
         ssbo->buffer_offset = 0;
         ssbo->buffer_size = 0;
         update |= (usage & BITFIELD64_BIT(start_slot + i));
      }
   }
   if (update)
      invalidate_descriptor_state(ctx, p_stage, ZINK_DESCRIPTOR_TYPE_SSBO);
}

static void
zink_set_shader_images(struct pipe_context *pctx,
                       enum pipe_shader_type p_stage,
                       unsigned start_slot, unsigned count,
                       const struct pipe_image_view *images)
{
   struct zink_context *ctx = zink_context(pctx);
   uint32_t usage = zink_program_get_descriptor_usage(ctx, p_stage, ZINK_DESCRIPTOR_TYPE_IMAGE);
   bool update = false;
   for (unsigned i = 0; i < count; i++) {
      struct zink_image_view *image_view = &ctx->image_views[p_stage][start_slot + i];
      if (images && images[i].resource) {
         util_dynarray_init(&image_view->desc_set_refs.refs, NULL);
         struct zink_resource *res = (void *) images[i].resource;
         if (!zink_resource_object_init_storage(zink_screen(pctx->screen), res)) {
            debug_printf("couldn't create storage image!");
            continue;
         }
         res->bind_history |= BITFIELD64_BIT(ZINK_DESCRIPTOR_TYPE_IMAGE);
         res->bind_stages |= 1 << p_stage;
         util_copy_image_view(&image_view->base, images + i);
         if (images[i].resource->target == PIPE_BUFFER) {
            image_view->buffer_view = get_buffer_view(ctx, res, images[i].format, images[i].u.buf.offset, images[i].u.buf.size);
            assert(image_view->buffer_view);
            util_range_add(&res->base.b, &res->valid_buffer_range, images[i].u.buf.offset,
                           images[i].u.buf.offset + images[i].u.buf.size);
         } else {
            struct pipe_surface tmpl = {};
            tmpl.format = images[i].format;
            tmpl.nr_samples = 1;
            tmpl.u.tex.level = images[i].u.tex.level;
            tmpl.u.tex.first_layer = images[i].u.tex.first_layer;
            tmpl.u.tex.last_layer = images[i].u.tex.last_layer;
            image_view->surface = (void*)pctx->create_surface(pctx, &res->base.b, &tmpl);
            assert(image_view->surface);
         }
         update = true;
      } else if (image_view->base.resource) {
         zink_descriptor_set_refs_clear(&image_view->desc_set_refs, image_view);
         if (image_view->base.resource->target == PIPE_BUFFER)
            zink_buffer_view_reference(ctx, &image_view->buffer_view, NULL);
         else
            pipe_surface_reference((struct pipe_surface**)&image_view->surface, NULL);
         pipe_resource_reference(&image_view->base.resource, NULL);
         image_view->base.resource = NULL;
         image_view->surface = NULL;
         update |= (usage & BITFIELD64_BIT(start_slot + i));
      }
   }
   if (update)
      invalidate_descriptor_state(ctx, p_stage, ZINK_DESCRIPTOR_TYPE_IMAGE);
}

static void
sampler_view_buffer_clear(struct zink_context *ctx, struct zink_sampler_view *sampler_view)
{
   zink_descriptor_set_refs_clear(&sampler_view->desc_set_refs, sampler_view);
   zink_buffer_view_reference(ctx, &sampler_view->buffer_view, NULL);
}

static void
zink_set_sampler_views(struct pipe_context *pctx,
                       enum pipe_shader_type shader_type,
                       unsigned start_slot,
                       unsigned num_views,
                       struct pipe_sampler_view **views)
{
   struct zink_context *ctx = zink_context(pctx);
   assert(views);
   uint32_t usage = zink_program_get_descriptor_usage(ctx, shader_type, ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW);
   bool update = false;
   for (unsigned i = 0; i < num_views; ++i) {
      struct zink_sampler_view *a = zink_sampler_view(ctx->sampler_views[shader_type][start_slot + i]);
      struct zink_sampler_view *b = zink_sampler_view(views[i]);
      if (b && b->base.texture) {
         struct zink_resource *res = zink_resource(b->base.texture);
         if (res->base.b.target == PIPE_BUFFER &&
             res->bind_history & BITFIELD64_BIT(ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW)) {
            /* if this resource has been rebound while it wasn't set here,
             * its backing resource will have changed and thus we need to update
             * the bufferview
             */
            struct zink_buffer_view *buffer_view = get_buffer_view(ctx, res, b->base.format, b->base.u.buf.offset, b->base.u.buf.size);
            if (buffer_view == b->buffer_view)
               p_atomic_dec(&buffer_view->reference.count);
            else {
               sampler_view_buffer_clear(ctx, b);
               b->buffer_view = buffer_view;
            }
         }
         res->bind_history |= BITFIELD64_BIT(ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW);
         res->bind_stages |= 1 << shader_type;
      }
      bool is_buffer = zink_program_descriptor_is_buffer(ctx, shader_type, ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW, start_slot + i);
      uint32_t hash_a = zink_get_sampler_view_hash(ctx, a, is_buffer);
      uint32_t hash_b = zink_get_sampler_view_hash(ctx, b, is_buffer);
      if (usage & BITFIELD64_BIT(start_slot + i))
         update |= !!a != !!b || hash_a != hash_b;
      pipe_sampler_view_reference(&ctx->sampler_views[shader_type][start_slot + i], views[i]);
   }
   ctx->num_sampler_views[shader_type] = start_slot + num_views;
   if (update)
      invalidate_descriptor_state(ctx, shader_type, ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW);
}

static void
zink_set_stencil_ref(struct pipe_context *pctx,
                     const struct pipe_stencil_ref *ref)
{
   struct zink_context *ctx = zink_context(pctx);
   ctx->stencil_ref = *ref;
}

static void
zink_set_clip_state(struct pipe_context *pctx,
                    const struct pipe_clip_state *pcs)
{
}

static uint32_t
hash_render_pass_state(const void *key)
{
   return _mesa_hash_data(key, sizeof(struct zink_render_pass_state));
}

static bool
equals_render_pass_state(const void *a, const void *b)
{
   return memcmp(a, b, sizeof(struct zink_render_pass_state)) == 0;
}

static void
zink_set_tess_state(struct pipe_context *pctx,
                    const float default_outer_level[4],
                    const float default_inner_level[2])
{
   struct zink_context *ctx = zink_context(pctx);
   memcpy(&ctx->default_inner_level, default_inner_level, sizeof(ctx->default_inner_level));
   memcpy(&ctx->default_outer_level, default_outer_level, sizeof(ctx->default_outer_level));
}

static struct zink_render_pass *
get_render_pass(struct zink_context *ctx)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   const struct pipe_framebuffer_state *fb = &ctx->fb_state;
   struct zink_render_pass_state state = { 0 };
   uint32_t clears = 0;

   for (int i = 0; i < fb->nr_cbufs; i++) {
      struct pipe_surface *surf = fb->cbufs[i];
      if (surf) {
         state.rts[i].format = zink_get_format(screen, surf->format);
         state.rts[i].samples = surf->texture->nr_samples > 0 ? surf->texture->nr_samples :
                                                       VK_SAMPLE_COUNT_1_BIT;
         state.rts[i].clear_color = zink_fb_clear_enabled(ctx, i) && !zink_fb_clear_first_needs_explicit(&ctx->fb_clears[i]);
         clears |= !!state.rts[i].clear_color ? BITFIELD64_BIT(i) : 0;
      } else {
         state.rts[i].format = VK_FORMAT_R8_UINT;
         state.rts[i].samples = MAX2(fb->samples, 1);
      }
   }
   state.num_cbufs = fb->nr_cbufs;

   if (fb->zsbuf) {
      struct zink_resource *zsbuf = zink_resource(fb->zsbuf->texture);
      struct zink_framebuffer_clear *fb_clear = &ctx->fb_clears[PIPE_MAX_COLOR_BUFS];
      state.rts[fb->nr_cbufs].format = zsbuf->format;
      state.rts[fb->nr_cbufs].samples = zsbuf->base.b.nr_samples > 0 ? zsbuf->base.b.nr_samples : VK_SAMPLE_COUNT_1_BIT;
      state.rts[fb->nr_cbufs].clear_color = zink_fb_clear_enabled(ctx, PIPE_MAX_COLOR_BUFS) &&
                                            !zink_fb_clear_first_needs_explicit(fb_clear) &&
                                            (zink_fb_clear_element(fb_clear, 0)->zs.bits & PIPE_CLEAR_DEPTH);
      state.rts[fb->nr_cbufs].clear_stencil = zink_fb_clear_enabled(ctx, PIPE_MAX_COLOR_BUFS) &&
                                              !zink_fb_clear_first_needs_explicit(fb_clear) &&
                                              (zink_fb_clear_element(fb_clear, 0)->zs.bits & PIPE_CLEAR_STENCIL);
      clears |= state.rts[fb->nr_cbufs].clear_color || state.rts[fb->nr_cbufs].clear_stencil ? BITFIELD64_BIT(fb->nr_cbufs) : 0;;
   }
   state.have_zsbuf = fb->zsbuf != NULL;
#ifndef NDEBUG
      state.clears = clears;
#endif
   uint32_t hash = hash_render_pass_state(&state);
   struct hash_entry *entry = _mesa_hash_table_search_pre_hashed(ctx->render_pass_cache, hash,
                                                                 &state);
   struct zink_render_pass *rp;
   if (entry) {
      rp = entry->data;
      assert(rp->state.clears == clears);
   } else {
      rp = zink_create_render_pass(screen, &state);
      if (!_mesa_hash_table_insert_pre_hashed(ctx->render_pass_cache, hash, &rp->state, rp))
         return NULL;
   }
   return rp;
}

static bool
equals_ivci(const void *a, const void *b)
{
   return memcmp(a, b, sizeof(VkImageViewCreateInfo)) == 0;
}

static bool
equals_bvci(const void *a, const void *b)
{
   return memcmp(a, b, sizeof(VkBufferViewCreateInfo)) == 0;
}

static struct zink_framebuffer *
get_framebuffer(struct zink_context *ctx)
{
   struct zink_batch *batch = zink_batch_no_rp(ctx);
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   struct pipe_surface *attachments[PIPE_MAX_COLOR_BUFS + 1] = {};

   struct zink_framebuffer_state state = {};
   state.rp = get_render_pass(ctx);
   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
      struct pipe_surface *psurf = ctx->fb_state.cbufs[i];
      state.attachments[i] = psurf ? zink_surface(psurf)->image_view : VK_NULL_HANDLE;
      attachments[i] = psurf;
   }

   state.num_attachments = ctx->fb_state.nr_cbufs;
   if (ctx->fb_state.zsbuf) {
      struct pipe_surface *psurf = ctx->fb_state.zsbuf;
      state.attachments[state.num_attachments] = psurf ? zink_surface(psurf)->image_view : VK_NULL_HANDLE;;
      attachments[state.num_attachments++] = psurf;
   }

   state.width = MAX2(ctx->fb_state.width, 1);
   state.height = MAX2(ctx->fb_state.height, 1);
   state.layers = MAX2(util_framebuffer_get_num_layers(&ctx->fb_state), 1);
   state.samples = ctx->fb_state.samples;

   struct hash_entry *entry = _mesa_hash_table_search(batch->state->framebuffer_cache, &state);

   if (!entry) {
      struct zink_framebuffer *fb = zink_create_framebuffer(ctx, screen, &state, &attachments[0]);
      entry = _mesa_hash_table_insert(batch->state->framebuffer_cache, &fb->state, fb);
      if (!entry)
         return NULL;
   }

   return entry->data;
}

static void
framebuffer_state_buffer_barriers_setup(struct zink_context *ctx,
                                        const struct pipe_framebuffer_state *state, struct zink_batch *batch)
{
   for (int i = 0; i < state->nr_cbufs; i++) {
      struct pipe_surface *surf = state->cbufs[i];
      if (!surf)
         surf = ctx->framebuffer->null_surface;
      struct zink_resource *res = zink_resource(surf->texture);
      zink_resource_image_barrier(ctx, NULL, res, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 0);
   }

   if (state->zsbuf) {
      struct zink_resource *res = zink_resource(state->zsbuf->texture);
      zink_resource_image_barrier(ctx, NULL, res, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0);
   }
}

static void
setup_framebuffer(struct zink_context *ctx)
{
   struct zink_framebuffer *fb = get_framebuffer(ctx);
   struct zink_screen *screen = zink_screen(ctx->base.screen);

   zink_framebuffer_reference(screen, &ctx->framebuffer, fb);
   if (fb->rp != ctx->gfx_pipeline_state.render_pass)
      ctx->gfx_pipeline_state.hash = 0;
   zink_render_pass_reference(screen, &ctx->gfx_pipeline_state.render_pass, fb->rp);
}

void
zink_begin_render_pass(struct zink_context *ctx, struct zink_batch *batch)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);

   setup_framebuffer(ctx);
   assert(ctx->gfx_pipeline_state.render_pass);
   struct pipe_framebuffer_state *fb_state = &ctx->fb_state;

   VkRenderPassBeginInfo rpbi = {};
   rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   rpbi.renderPass = ctx->gfx_pipeline_state.render_pass->render_pass;
   rpbi.renderArea.offset.x = 0;
   rpbi.renderArea.offset.y = 0;
   rpbi.renderArea.extent.width = fb_state->width;
   rpbi.renderArea.extent.height = fb_state->height;

   VkClearValue clears[PIPE_MAX_COLOR_BUFS + 1] = {};
   unsigned clear_buffers = 0;
   uint32_t clear_validate = 0;
   for (int i = 0; i < fb_state->nr_cbufs; i++) {
      /* these are no-ops */
      if (!fb_state->cbufs[i] || !zink_fb_clear_enabled(ctx, i))
         continue;
      /* these need actual clear calls inside the rp */
      struct zink_framebuffer_clear_data *clear = zink_fb_clear_element(&ctx->fb_clears[i], 0);
      if (zink_fb_clear_needs_explicit(&ctx->fb_clears[i])) {
         clear_buffers |= (PIPE_CLEAR_COLOR0 << i);
         if (zink_fb_clear_count(&ctx->fb_clears[i]) < 2 ||
             zink_fb_clear_element_needs_explicit(clear))
            continue;
      }
      /* we now know there's one clear that can be done here */
      if (clear->color.srgb) {
         clears[i].color.float32[0] = util_format_srgb_to_linear_float(clear->color.color.f[0]);
         clears[i].color.float32[1] = util_format_srgb_to_linear_float(clear->color.color.f[1]);
         clears[i].color.float32[2] = util_format_srgb_to_linear_float(clear->color.color.f[2]);
      } else {
         clears[i].color.float32[0] = clear->color.color.f[0];
         clears[i].color.float32[1] = clear->color.color.f[1];
         clears[i].color.float32[2] = clear->color.color.f[2];
      }
      clears[i].color.float32[3] = clear->color.color.f[3];
      rpbi.clearValueCount = i + 1;
      clear_validate |= BITFIELD64_BIT(i);
      assert(ctx->framebuffer->rp->state.clears);
   }
   if (fb_state->zsbuf && zink_fb_clear_enabled(ctx, PIPE_MAX_COLOR_BUFS)) {
      struct zink_framebuffer_clear *fb_clear = &ctx->fb_clears[PIPE_MAX_COLOR_BUFS];
      struct zink_framebuffer_clear_data *clear = zink_fb_clear_element(fb_clear, 0);
      if (!zink_fb_clear_element_needs_explicit(clear)) {
         clears[fb_state->nr_cbufs].depthStencil.depth = clear->zs.depth;
         clears[fb_state->nr_cbufs].depthStencil.stencil = clear->zs.stencil;
         rpbi.clearValueCount = fb_state->nr_cbufs + 1;
         clear_validate |= BITFIELD64_BIT(fb_state->nr_cbufs);
         assert(ctx->framebuffer->rp->state.clears);
      }
      if (zink_fb_clear_needs_explicit(fb_clear)) {
         for (int j = !zink_fb_clear_element_needs_explicit(clear); j < zink_fb_clear_count(fb_clear); j++)
            clear_buffers |= zink_fb_clear_element(fb_clear, j)->zs.bits;
      }
   }
   assert(clear_validate == ctx->framebuffer->rp->state.clears);
   rpbi.pClearValues = &clears[0];
   rpbi.framebuffer = ctx->framebuffer->fb;

   assert(ctx->gfx_pipeline_state.render_pass && ctx->framebuffer);

   framebuffer_state_buffer_barriers_setup(ctx, fb_state, batch);

   zink_render_pass_reference(screen, &batch->state->rp, ctx->gfx_pipeline_state.render_pass);
   zink_framebuffer_reference(screen, &batch->state->fb, ctx->framebuffer);
   for (int i = 0; i < ARRAY_SIZE(batch->state->fb->surfaces); i++) {
      if (!batch->state->fb->surfaces[i])
         break;
      zink_batch_reference_resource_rw(batch, zink_resource(batch->state->fb->surfaces[i]->texture), true);
   }

   vkCmdBeginRenderPass(batch->state->cmdbuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
   batch->in_rp = true;

   zink_clear_framebuffer(ctx, clear_buffers);
}

static void
zink_end_render_pass(struct zink_context *ctx, struct zink_batch *batch)
{
   if (batch->in_rp)
      vkCmdEndRenderPass(batch->state->cmdbuf);
   batch->in_rp = false;
}

static void
sync_flush(struct zink_context *ctx, struct zink_batch_state *bs)
{
   if (util_queue_is_initialized(&ctx->batch.flush_queue))
      util_queue_fence_wait(&bs->flush_completed);
}

static void
flush_batch(struct zink_context *ctx, bool sync)
{
   struct zink_batch *batch = &ctx->batch;
   zink_end_render_pass(ctx, batch);
   zink_end_batch(ctx, batch);

   if (sync)
      sync_flush(ctx, ctx->batch.state);

   if (ctx->batch.state->is_device_lost && ctx->reset.reset) {
      ctx->is_device_lost = true;
      ctx->reset.reset(ctx->reset.data, PIPE_GUILTY_CONTEXT_RESET);
   } else {
      incr_curr_batch(ctx);

      zink_start_batch(ctx, batch);
      if (zink_screen(ctx->base.screen)->info.have_EXT_transform_feedback && ctx->num_so_targets)
         ctx->dirty_so_targets = true;
   }
}

struct zink_batch *
zink_batch_rp(struct zink_context *ctx)
{
   struct zink_batch *batch = &ctx->batch;
   if (!batch->in_rp) {
      zink_begin_render_pass(ctx, batch);
      assert(batch->state->rp);
   }
   return batch;
}

struct zink_batch *
zink_batch_no_rp(struct zink_context *ctx)
{
   struct zink_batch *batch = &ctx->batch;
   zink_end_render_pass(ctx, batch);
   assert(!batch->in_rp);
   return batch;
}

void
zink_flush_queue(struct zink_context *ctx)
{
   flush_batch(ctx, true);
}

static void
zink_set_framebuffer_state(struct pipe_context *pctx,
                           const struct pipe_framebuffer_state *state)
{
   struct zink_context *ctx = zink_context(pctx);

   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
      struct pipe_surface *surf = ctx->fb_state.cbufs[i];
      if (surf &&
          (!state->cbufs[i] || i >= state->nr_cbufs ||
           surf->texture != state->cbufs[i]->texture ||
           surf->format != state->cbufs[i]->format ||
           memcmp(&surf->u, &state->cbufs[i]->u, sizeof(union pipe_surface_desc))))
         zink_fb_clears_apply(ctx, surf->texture);
   }
   if (ctx->fb_state.zsbuf) {
      struct pipe_surface *surf = ctx->fb_state.zsbuf;
      if (!state->zsbuf || surf->texture != state->zsbuf->texture ||
          memcmp(&surf->u, &state->zsbuf->u, sizeof(union pipe_surface_desc)))
      zink_fb_clears_apply(ctx, ctx->fb_state.zsbuf->texture);
   }

   util_copy_framebuffer_state(&ctx->fb_state, state);

   uint8_t rast_samples = util_framebuffer_get_num_samples(state);
   /* in vulkan, gl_SampleMask needs to be explicitly ignored for sampleCount == 1 */
   if ((ctx->gfx_pipeline_state.rast_samples > 1) != (rast_samples > 1))
      ctx->dirty_shader_stages |= 1 << PIPE_SHADER_FRAGMENT;
   if (ctx->gfx_pipeline_state.rast_samples != rast_samples)
      ctx->gfx_pipeline_state.hash = 0;
   ctx->gfx_pipeline_state.rast_samples = rast_samples;
   if (ctx->gfx_pipeline_state.num_attachments != state->nr_cbufs)
      ctx->gfx_pipeline_state.hash = 0;
   ctx->gfx_pipeline_state.num_attachments = state->nr_cbufs;

   /* need to ensure we start a new rp on next draw */
   zink_batch_no_rp(ctx);
}

static void
zink_set_blend_color(struct pipe_context *pctx,
                     const struct pipe_blend_color *color)
{
   struct zink_context *ctx = zink_context(pctx);
   memcpy(ctx->blend_constants, color->color, sizeof(float) * 4);
}

static void
zink_set_sample_mask(struct pipe_context *pctx, unsigned sample_mask)
{
   struct zink_context *ctx = zink_context(pctx);
   ctx->gfx_pipeline_state.sample_mask = sample_mask;
   ctx->gfx_pipeline_state.hash = 0;
}

static VkAccessFlags
access_src_flags(VkImageLayout layout)
{
   switch (layout) {
   case VK_IMAGE_LAYOUT_UNDEFINED:
      return 0;

   case VK_IMAGE_LAYOUT_GENERAL:
      return VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

   case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
   case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_ACCESS_SHADER_READ_BIT;

   case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_ACCESS_TRANSFER_READ_BIT;

   case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_ACCESS_TRANSFER_WRITE_BIT;

   case VK_IMAGE_LAYOUT_PREINITIALIZED:
      return VK_ACCESS_HOST_WRITE_BIT;

   case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return 0;

   default:
      unreachable("unexpected layout");
   }
}

static VkAccessFlags
access_dst_flags(VkImageLayout layout)
{
   switch (layout) {
   case VK_IMAGE_LAYOUT_UNDEFINED:
      return 0;

   case VK_IMAGE_LAYOUT_GENERAL:
      return VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

   case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

   case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_ACCESS_TRANSFER_READ_BIT;

   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
   case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_ACCESS_SHADER_READ_BIT;

   case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_ACCESS_TRANSFER_WRITE_BIT;

   case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return 0;

   default:
      unreachable("unexpected layout");
   }
}

static VkPipelineStageFlags
pipeline_dst_stage(VkImageLayout layout)
{
   switch (layout) {
   case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

   case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;
   case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;

   case VK_IMAGE_LAYOUT_GENERAL:
      return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

   default:
      return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
   }
}

#define ALL_READ_ACCESS_FLAGS \
    (VK_ACCESS_INDIRECT_COMMAND_READ_BIT | \
    VK_ACCESS_INDEX_READ_BIT | \
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | \
    VK_ACCESS_UNIFORM_READ_BIT | \
    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | \
    VK_ACCESS_SHADER_READ_BIT | \
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | \
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | \
    VK_ACCESS_TRANSFER_READ_BIT |\
    VK_ACCESS_HOST_READ_BIT |\
    VK_ACCESS_MEMORY_READ_BIT |\
    VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT |\
    VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT |\
    VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT |\
    VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR |\
    VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV |\
    VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT |\
    VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV |\
    VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV |\
    VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV)


bool
zink_resource_access_is_write(VkAccessFlags flags)
{
   return (flags & ALL_READ_ACCESS_FLAGS) != flags;
}

bool
zink_resource_image_needs_barrier(struct zink_resource *res, VkImageLayout new_layout, VkAccessFlags flags, VkPipelineStageFlags pipeline)
{
   if (!pipeline)
      pipeline = pipeline_dst_stage(new_layout);
   if (!flags)
      flags = access_dst_flags(new_layout);
   return res->layout != new_layout || (res->access_stage & pipeline) != pipeline ||
          (res->access & flags) != flags ||
          (zink_resource_access_is_write(flags) && util_bitcount(flags) > 1);
}

bool
zink_resource_image_barrier_init(VkImageMemoryBarrier *imb, struct zink_resource *res, VkImageLayout new_layout, VkAccessFlags flags, VkPipelineStageFlags pipeline)
{
   if (!pipeline)
      pipeline = pipeline_dst_stage(new_layout);
   if (!flags)
      flags = access_dst_flags(new_layout);

   VkImageSubresourceRange isr = {
      res->aspect,
      0, VK_REMAINING_MIP_LEVELS,
      0, VK_REMAINING_ARRAY_LAYERS
   };
   *imb = (VkImageMemoryBarrier){
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      NULL,
      res->access ? res->access : access_src_flags(res->layout),
      flags,
      res->layout,
      new_layout,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      res->obj->image,
      isr
   };
   return zink_resource_image_needs_barrier(res, new_layout, flags, pipeline);
}

void
zink_resource_image_barrier(struct zink_context *ctx, struct zink_batch *batch, struct zink_resource *res,
                      VkImageLayout new_layout, VkAccessFlags flags, VkPipelineStageFlags pipeline)
{
   VkImageMemoryBarrier imb[2];
   unsigned num_barriers = 1;
   if (!zink_resource_image_barrier_init(&imb[0], res, new_layout, flags, pipeline))
      return;
   if (!pipeline)
      pipeline = pipeline_dst_stage(new_layout);
   /* only barrier if we're changing layout or doing something besides read -> read */
   if (!batch) {
      batch = zink_batch_no_rp(ctx);
   }
   assert(!batch->in_rp);
   if (flags & (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT) &&
       new_layout == VK_IMAGE_LAYOUT_GENERAL && res->obj->simage) {
      imb[1] = imb[0];
      imb[1].image = res->obj->simage;
      imb[1].oldLayout = res->obj->storage_init ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
      imb[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
      num_barriers++;
      res->obj->storage_init = true;
   }
   vkCmdPipelineBarrier(
      batch->state->cmdbuf,
      res->access_stage ? res->access_stage : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      pipeline,
      0,
      0, NULL,
      0, NULL,
      num_barriers, imb
   );

   res->layout = new_layout;
   res->access_stage = pipeline;
   res->access = imb[0].dstAccessMask;
}


VkPipelineStageFlags
zink_pipeline_flags_from_stage(VkShaderStageFlagBits stage)
{
   switch (stage) {
   case VK_SHADER_STAGE_VERTEX_BIT:
      return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
   case VK_SHADER_STAGE_FRAGMENT_BIT:
      return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
   case VK_SHADER_STAGE_GEOMETRY_BIT:
      return VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
   case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
      return VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
   case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
      return VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
   case VK_SHADER_STAGE_COMPUTE_BIT:
      return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
   default:
      unreachable("unknown shader stage bit");
   }
}

static VkPipelineStageFlags
pipeline_access_stage(VkAccessFlags flags)
{
   switch (flags) {
   case VK_ACCESS_UNIFORM_READ_BIT:
   case VK_ACCESS_SHADER_READ_BIT:
   case VK_ACCESS_SHADER_WRITE_BIT:
      return VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV |
             VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV |
             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR |
             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
             VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
             VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
             VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
   default:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;
   }
}

bool
zink_resource_buffer_needs_barrier(struct zink_resource *res, VkAccessFlags flags, VkPipelineStageFlags pipeline)
{
   if (!pipeline)
      pipeline = pipeline_access_stage(flags);
   return (res->access_stage & pipeline) != pipeline || (res->access & flags) != flags ||
          (zink_resource_access_is_write(flags) && util_bitcount(flags) > 1);
}

bool
zink_resource_buffer_barrier_init(VkBufferMemoryBarrier *bmb, struct zink_resource *res, VkAccessFlags flags, VkPipelineStageFlags pipeline)
{
   if (!pipeline)
      pipeline = pipeline_access_stage(flags);
   *bmb = (VkBufferMemoryBarrier){
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      NULL,
      res->access,
      flags,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      res->obj->buffer,
      res->obj->offset,
      res->base.b.width0
   };
   return zink_resource_buffer_needs_barrier(res, flags, pipeline);
}

void
zink_resource_buffer_barrier(struct zink_context *ctx, struct zink_batch *batch, struct zink_resource *res, VkAccessFlags flags, VkPipelineStageFlags pipeline)
{
   VkBufferMemoryBarrier bmb[2];
   unsigned num_barriers = 1;
   if (!zink_resource_buffer_barrier_init(&bmb[0], res, flags, pipeline))
      return;
   if (!pipeline)
      pipeline = pipeline_access_stage(flags);
   /* only barrier if we're changing layout or doing something besides read -> read */
   if (!batch) {
      batch = zink_batch_no_rp(ctx);
   }
   assert(!batch->in_rp);
   if (flags & (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT) && res->obj->sbuffer) {
      bmb[1] = bmb[0];
      bmb[1].buffer = res->obj->sbuffer;
      num_barriers++;
   }
   vkCmdPipelineBarrier(
      batch->state->cmdbuf,
      res->access_stage ? res->access_stage : pipeline_access_stage(res->access),
      pipeline,
      0,
      0, NULL,
      num_barriers, bmb,
      0, NULL
   );
   res->access = bmb[0].dstAccessMask;
   res->access_stage = pipeline;
}

bool
zink_resource_needs_barrier(struct zink_resource *res, VkImageLayout layout, VkAccessFlags flags, VkPipelineStageFlags pipeline)
{
   if (res->base.b.target == PIPE_BUFFER)
      return zink_resource_buffer_needs_barrier(res, flags, pipeline);
   return zink_resource_image_needs_barrier(res, layout, flags, pipeline);
}

void
zink_resource_barrier(struct zink_context *ctx, struct zink_batch *batch, struct zink_resource *res, VkImageLayout layout, VkAccessFlags flags, VkPipelineStageFlags pipeline)
{
   if (res->base.b.target == PIPE_BUFFER)
      zink_resource_buffer_barrier(ctx, batch, res, flags, pipeline);
   else
      zink_resource_image_barrier(ctx, batch, res, layout, flags, pipeline);
}

VkShaderStageFlagBits
zink_shader_stage(enum pipe_shader_type type)
{
   VkShaderStageFlagBits stages[] = {
      [PIPE_SHADER_VERTEX] = VK_SHADER_STAGE_VERTEX_BIT,
      [PIPE_SHADER_FRAGMENT] = VK_SHADER_STAGE_FRAGMENT_BIT,
      [PIPE_SHADER_GEOMETRY] = VK_SHADER_STAGE_GEOMETRY_BIT,
      [PIPE_SHADER_TESS_CTRL] = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
      [PIPE_SHADER_TESS_EVAL] = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
      [PIPE_SHADER_COMPUTE] = VK_SHADER_STAGE_COMPUTE_BIT,
   };
   return stages[type];
}

static uint32_t
hash_gfx_program(const void *key)
{
   const struct zink_shader **shaders = (void*)key;
   uint32_t hash = 0;
   unsigned zero = 0;
   /* pointers can be recycled, so we need to check the shader ids */
   for (unsigned i = 0; i < ZINK_SHADER_COUNT; i++)
      hash = XXH32(shaders[i] ? &shaders[i]->shader_id : &zero, sizeof(unsigned), hash);
   return hash;
}

static bool
equals_gfx_program(const void *a, const void *b)
{
   const struct zink_shader **left = (void*)a, **right = (void*)b;
   /* if any shaders are set/unset or shader ids don't match then these aren't equal */
   for (unsigned i = 0; i < ZINK_SHADER_COUNT; i++) {
      if (!!left[i] != !!right[i])
         return false;
      if (left[i] && right[i] && left[i]->shader_id != right[i]->shader_id)
         return false;
   }
   return true;
}

static void
zink_flush(struct pipe_context *pctx,
           struct pipe_fence_handle **pfence,
           enum pipe_flush_flags flags)
{
   struct zink_context *ctx = zink_context(pctx);
   bool deferred = flags & PIPE_FLUSH_DEFERRED;
   bool deferred_fence = false;
   struct zink_batch *batch = &ctx->batch;
   struct zink_fence *fence = NULL;
   struct zink_screen *screen = zink_screen(ctx->base.screen);

   if (!deferred && ctx->clears_enabled) {
      /* start rp to do all the clears */
      zink_begin_render_pass(ctx, batch);
   }

   if (!deferred) {
      if (flags & PIPE_FLUSH_END_OF_FRAME) {
         if (ctx->fb_state.nr_cbufs)
            zink_end_render_pass(ctx, batch);
         for (int i = 0; i < ctx->fb_state.nr_cbufs; i++)
            zink_resource_image_barrier(ctx, batch,
                                        ctx->fb_state.cbufs[i] ? zink_resource(ctx->fb_state.cbufs[i]->texture) : NULL,
                                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0);
      }
   }

   if (!batch->has_work) {
       if (pfence) {
          /* reuse last fence */
          fence = ctx->last_fence;
       }
       if (!deferred) {
          struct zink_batch_state *last = zink_batch_state(ctx->last_fence);
          if (last) {
             sync_flush(ctx, last);
             if (last->is_device_lost && ctx->reset.reset) {
                ctx->is_device_lost = true;
                ctx->reset.reset(ctx->reset.data, PIPE_GUILTY_CONTEXT_RESET);
             }
          }
       }
   } else {
      fence = &batch->state->fence;
      if (deferred && !(flags & PIPE_FLUSH_FENCE_FD) && pfence)
         deferred_fence = true;
      else
         flush_batch(ctx, true);
   }

   if (pfence) {
      struct zink_tc_fence *mfence;

      if (flags & TC_FLUSH_ASYNC) {
         mfence = zink_tc_fence(*pfence);
         assert(mfence);
      } else {
         mfence = zink_create_tc_fence();

         screen->base.fence_reference(&screen->base, pfence, NULL);
         *pfence = (struct pipe_fence_handle *)mfence;
      }

      zink_batch_state_reference(screen, NULL, zink_batch_state(fence));
      mfence->fence = fence;
      if (fence)
         mfence->batch_id = fence->batch_id;

      if (deferred_fence) {
         assert(fence);
         mfence->deferred_ctx = pctx;
         mfence->deferred_id = fence->batch_id;
      }

      if (!fence || flags & TC_FLUSH_ASYNC) {
         if (!util_queue_fence_is_signalled(&mfence->ready))
            util_queue_fence_signal(&mfence->ready);
      }
   }
   if (fence && !(flags & (PIPE_FLUSH_DEFERRED | PIPE_FLUSH_ASYNC)))
      sync_flush(ctx, zink_batch_state(fence));
   if (flags & PIPE_FLUSH_END_OF_FRAME && !(flags & TC_FLUSH_ASYNC) && !deferred) {
      if (!ctx->first_frame)
         zink_vkfence_wait(screen, fence, PIPE_TIMEOUT_INFINITE);
      ctx->first_frame = true;
   }
}

void
zink_maybe_flush_or_stall(struct zink_context *ctx)
{
   struct zink_screen *screen = zink_screen(ctx->base.screen);
   /* flush anytime our total batch memory usage is potentially >= 1/10 of total system memory */
   if (ctx->batch.state->resource_size >= screen->total_mem / 10)
      flush_batch(ctx, true);

   if (ctx->resource_size >= screen->total_mem / 10 || _mesa_hash_table_num_entries(&ctx->batch_states) > 10) {
      sync_flush(ctx, zink_batch_state(ctx->last_fence));
      zink_vkfence_wait(screen, ctx->last_fence, PIPE_TIMEOUT_INFINITE);
      zink_batch_reset_all(ctx);
   }
}

void
zink_fence_wait(struct pipe_context *pctx)
{
   struct zink_context *ctx = zink_context(pctx);

   if (ctx->batch.has_work)
      pctx->flush(pctx, NULL, PIPE_FLUSH_HINT_FINISH);
   if (ctx->last_fence) {
      sync_flush(ctx, zink_batch_state(ctx->last_fence));
      zink_vkfence_wait(zink_screen(ctx->base.screen), ctx->last_fence, PIPE_TIMEOUT_INFINITE);
      zink_batch_reset_all(ctx);
   }
}

void
zink_wait_on_batch(struct zink_context *ctx, uint32_t batch_id)
{
   struct zink_batch_state *bs = ctx->batch.state;
   assert(bs);
   if (!batch_id || bs->fence.batch_id == batch_id)
      /* not submitted yet */
      flush_batch(ctx, true);

   struct zink_fence *fence;

   assert(batch_id || ctx->last_fence);
   if (ctx->last_fence && (!batch_id || batch_id == zink_batch_state(ctx->last_fence)->fence.batch_id))
      fence = ctx->last_fence;
   else {
      struct hash_entry *he = _mesa_hash_table_search_pre_hashed(&ctx->batch_states, batch_id, (void*)(uintptr_t)batch_id);
      /* if we can't find it, it must have finished already */
      if (!he)
         return;
      fence = he->data;
   }
   assert(fence);
   sync_flush(ctx, zink_batch_state(fence));
   zink_vkfence_wait(zink_screen(ctx->base.screen), fence, PIPE_TIMEOUT_INFINITE);
}

bool
zink_check_batch_completion(struct zink_context *ctx, uint32_t batch_id)
{
   assert(batch_id);
   struct zink_batch_state *bs = ctx->batch.state;
   assert(bs);
   if (bs->fence.batch_id == batch_id)
      /* not submitted yet */
      return false;

   struct zink_fence *fence;

   if (ctx->last_fence && batch_id == zink_batch_state(ctx->last_fence)->fence.batch_id)
      fence = ctx->last_fence;
   else {
      struct hash_entry *he = _mesa_hash_table_search_pre_hashed(&ctx->batch_states, batch_id, (void*)(uintptr_t)batch_id);
      /* if we can't find it, it must have finished already */
      if (!he)
         return true;
      fence = he->data;
   }
   assert(fence);
   if (util_queue_is_initialized(&ctx->batch.flush_queue) &&
       !util_queue_fence_is_signalled(&zink_batch_state(fence)->flush_completed))
      return false;
   return zink_vkfence_wait(zink_screen(ctx->base.screen), fence, 0);
}

static void
zink_texture_barrier(struct pipe_context *pctx, unsigned flags)
{
   struct zink_context *ctx = zink_context(pctx);
   /* TODO: if we ever start using fully parallelized batches, this probably needs a stall */
   if (ctx->batch.has_work)
      pctx->flush(pctx, NULL, 0);
   zink_flush_queue(ctx);
}

static void
zink_memory_barrier(struct pipe_context *pctx, unsigned flags)
{
   struct zink_context *ctx = zink_context(pctx);
   VkAccessFlags sflags = 0;
   VkAccessFlags dflags = 0;
   VkPipelineStageFlags src = 0;
   VkPipelineStageFlags dst = 0;

   VkPipelineStageFlags all_flags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                    VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
                                    VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
                                    VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

   if (flags == PIPE_BARRIER_ALL) {
      sflags = dflags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
      src = dst = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
   } else {
      while (flags) {
         unsigned flag = u_bit_scan(&flags);
         
         switch (1 << flag) {
         case PIPE_BARRIER_MAPPED_BUFFER:
            sflags |= VK_ACCESS_SHADER_WRITE_BIT;
            dflags |= VK_ACCESS_TRANSFER_READ_BIT;
            break;
         case PIPE_BARRIER_SHADER_BUFFER:
            sflags |= VK_ACCESS_SHADER_WRITE_BIT;
            dflags |= VK_ACCESS_SHADER_READ_BIT;
            src |= all_flags;
            dst |= all_flags;
            break;
         case PIPE_BARRIER_QUERY_BUFFER:
            sflags |= VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            dflags |= VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                      VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
            break;
         case PIPE_BARRIER_VERTEX_BUFFER:
            sflags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            dflags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            src |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            dst |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            break;
         case PIPE_BARRIER_INDEX_BUFFER:
            sflags |= VK_ACCESS_INDEX_READ_BIT;
            dflags |= VK_ACCESS_INDEX_READ_BIT;
            src |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            dst |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            break;
         case PIPE_BARRIER_CONSTANT_BUFFER:
            sflags |= VK_ACCESS_UNIFORM_READ_BIT;
            dflags |= VK_ACCESS_UNIFORM_READ_BIT;
            src |= all_flags;
            dst |= all_flags;
            break;
         case PIPE_BARRIER_INDIRECT_BUFFER:
            sflags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            dflags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            src |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
            dst |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
            break;
         case PIPE_BARRIER_TEXTURE:
            sflags |= VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            dflags |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
            src |= all_flags;
            dst |= all_flags;
            break;
         case PIPE_BARRIER_IMAGE:
            sflags |= VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            dflags |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT;
            src |= all_flags;
            dst |= all_flags;
            break;
         case PIPE_BARRIER_FRAMEBUFFER:
            sflags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dflags |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            src |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dst |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
         case PIPE_BARRIER_STREAMOUT_BUFFER:
            sflags |= VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT;
            dflags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            src |= VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;
            dst |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            break;
         case PIPE_BARRIER_GLOBAL_BUFFER:
            debug_printf("zink: unhandled barrier flag %u\n", flag);
            break;
         case PIPE_BARRIER_UPDATE_BUFFER:
            sflags |= VK_ACCESS_TRANSFER_WRITE_BIT;
            dflags |= VK_ACCESS_TRANSFER_READ_BIT;
            src |= VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst |= VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
         case PIPE_BARRIER_UPDATE_TEXTURE:
            sflags |= VK_ACCESS_TRANSFER_WRITE_BIT;
            dflags |= VK_ACCESS_TRANSFER_READ_BIT;
            src |= VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst |= VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
         }
      }
   }
   VkMemoryBarrier b = {};
   b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   /* TODO: these are all probably wrong */
   b.srcAccessMask = sflags;
   b.dstAccessMask = dflags;

   struct zink_batch *batch = &ctx->batch;
   if (batch->has_work) {
      /* TODO: figure out self-referencing renderpass dependency and remove this
       * 
       * can't barrier during renderpass without inlining flush_batch() here
       */

      zink_end_render_pass(ctx, batch);

      /* this should be the only call needed */
      vkCmdPipelineBarrier(batch->state->cmdbuf, src, dst, 0, 0, &b, 0, NULL, 0, NULL);
      zink_end_batch(ctx, batch);
      sync_flush(ctx, batch->state);
      if (batch->state->is_device_lost && ctx->reset.reset) {
         ctx->is_device_lost = true;
         ctx->reset.reset(ctx->reset.data, PIPE_GUILTY_CONTEXT_RESET);
      }
      zink_vkfence_wait(zink_screen(ctx->base.screen), &batch->state->fence, PIPE_TIMEOUT_INFINITE);
      incr_curr_batch(ctx);

      zink_start_batch(ctx, batch);
   }
}

static void
zink_flush_resource(struct pipe_context *pipe,
                    struct pipe_resource *resource)
{
}

void
zink_copy_buffer(struct zink_context *ctx, struct zink_batch *batch, struct zink_resource *dst, struct zink_resource *src,
                 unsigned dst_offset, unsigned src_offset, unsigned size)
{
   VkBufferCopy region;
   region.srcOffset = src_offset;
   region.dstOffset = dst_offset;
   region.size = size;

   if (!batch)
      batch = zink_batch_no_rp(ctx);
   assert(!batch->in_rp);
   zink_batch_reference_resource_rw(batch, src, false);
   zink_batch_reference_resource_rw(batch, dst, true);
   util_range_add(&dst->base.b, &dst->valid_buffer_range, dst_offset, dst_offset + size);
   zink_resource_buffer_barrier(ctx, batch, src, VK_ACCESS_TRANSFER_READ_BIT, 0);
   zink_resource_buffer_barrier(ctx, batch, dst, VK_ACCESS_TRANSFER_WRITE_BIT, 0);
   vkCmdCopyBuffer(batch->state->cmdbuf, src->obj->buffer, dst->obj->buffer, 1, &region);
}

void
zink_copy_image_buffer(struct zink_context *ctx, struct zink_batch *batch, struct zink_resource *dst, struct zink_resource *src,
                       unsigned dst_level, unsigned dstx, unsigned dsty, unsigned dstz,
                       unsigned src_level, const struct pipe_box *src_box, enum pipe_map_flags map_flags)
{
   struct zink_resource *img = dst->base.b.target == PIPE_BUFFER ? src : dst;
   struct zink_resource *buf = dst->base.b.target == PIPE_BUFFER ? dst : src;

   if (!batch)
      batch = zink_batch_no_rp(ctx);

   bool buf2img = buf == src;

   if (buf2img) {
      zink_resource_image_barrier(ctx, batch, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0);
      zink_resource_buffer_barrier(ctx, batch, buf, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   } else {
      zink_resource_image_barrier(ctx, batch, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0);
      zink_resource_buffer_barrier(ctx, batch, buf, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
      util_range_add(&dst->base.b, &dst->valid_buffer_range, dstx, dstx + src_box->width);
   }

   VkBufferImageCopy region = {};
   region.bufferOffset = buf2img ? src_box->x : dstx;
   region.bufferRowLength = 0;
   region.bufferImageHeight = 0;
   region.imageSubresource.mipLevel = buf2img ? dst_level : src_level;
   region.imageSubresource.layerCount = 1;
   if (img->base.b.array_size > 1) {
      region.imageSubresource.baseArrayLayer = buf2img ? dstz : src_box->z;
      region.imageSubresource.layerCount = src_box->depth;
      region.imageExtent.depth = 1;
   } else {
      region.imageOffset.z = buf2img ? dstz : src_box->z;
      region.imageExtent.depth = src_box->depth;
   }
   region.imageOffset.x = buf2img ? dstx : src_box->x;
   region.imageOffset.y = buf2img ? dsty : src_box->y;

   region.imageExtent.width = src_box->width;
   region.imageExtent.height = src_box->height;

   zink_batch_reference_resource_rw(batch, img, buf2img);
   zink_batch_reference_resource_rw(batch, buf, !buf2img);

   /* we're using u_transfer_helper_deinterleave, which means we'll be getting PIPE_MAP_* usage
    * to indicate whether to copy either the depth or stencil aspects
    */
   unsigned aspects = 0;
   if (map_flags) {
      assert((map_flags & (PIPE_MAP_DEPTH_ONLY | PIPE_MAP_STENCIL_ONLY)) !=
             (PIPE_MAP_DEPTH_ONLY | PIPE_MAP_STENCIL_ONLY));
      if (map_flags & PIPE_MAP_DEPTH_ONLY)
         aspects = VK_IMAGE_ASPECT_DEPTH_BIT;
      else if (map_flags & PIPE_MAP_STENCIL_ONLY)
         aspects = VK_IMAGE_ASPECT_STENCIL_BIT;
   }
   if (!aspects)
      aspects = img->aspect;
   while (aspects) {
      int aspect = 1 << u_bit_scan(&aspects);
      region.imageSubresource.aspectMask = aspect;

      /* this may or may not work with multisampled depth/stencil buffers depending on the driver implementation:
       *
       * srcImage must have a sample count equal to VK_SAMPLE_COUNT_1_BIT
       * - vkCmdCopyImageToBuffer spec
       *
       * dstImage must have a sample count equal to VK_SAMPLE_COUNT_1_BIT
       * - vkCmdCopyBufferToImage spec
       */
      if (buf2img)
         vkCmdCopyBufferToImage(batch->state->cmdbuf, buf->obj->buffer, img->obj->image, img->layout, 1, &region);
      else
         vkCmdCopyImageToBuffer(batch->state->cmdbuf, img->obj->image, img->layout, buf->obj->buffer, 1, &region);
   }
}

static void
zink_resource_copy_region(struct pipe_context *pctx,
                          struct pipe_resource *pdst,
                          unsigned dst_level, unsigned dstx, unsigned dsty, unsigned dstz,
                          struct pipe_resource *psrc,
                          unsigned src_level, const struct pipe_box *src_box)
{
   struct zink_resource *dst = zink_resource(pdst);
   struct zink_resource *src = zink_resource(psrc);
   struct zink_context *ctx = zink_context(pctx);
   if (dst->base.b.target != PIPE_BUFFER && src->base.b.target != PIPE_BUFFER) {
      VkImageCopy region = {};
      if (util_format_get_num_planes(src->base.b.format) == 1 &&
          util_format_get_num_planes(dst->base.b.format) == 1) {
      /* If neither the calling command’s srcImage nor the calling command’s dstImage
       * has a multi-planar image format then the aspectMask member of srcSubresource
       * and dstSubresource must match
       *
       * -VkImageCopy spec
       */
         assert(src->aspect == dst->aspect);
      } else
         unreachable("planar formats not yet handled");

      zink_fb_clears_apply_or_discard(ctx, pdst, (struct u_rect){dstx, dstx + src_box->width, dsty, dsty + src_box->height}, false);
      zink_fb_clears_apply_region(ctx, psrc, zink_rect_from_box(src_box));

      region.srcSubresource.aspectMask = src->aspect;
      region.srcSubresource.mipLevel = src_level;
      region.srcSubresource.layerCount = 1;
      if (src->base.b.array_size > 1) {
         region.srcSubresource.baseArrayLayer = src_box->z;
         region.srcSubresource.layerCount = src_box->depth;
         region.extent.depth = 1;
      } else {
         region.srcOffset.z = src_box->z;
         region.srcSubresource.layerCount = 1;
         region.extent.depth = src_box->depth;
      }

      region.srcOffset.x = src_box->x;
      region.srcOffset.y = src_box->y;

      region.dstSubresource.aspectMask = dst->aspect;
      region.dstSubresource.mipLevel = dst_level;
      if (dst->base.b.array_size > 1) {
         region.dstSubresource.baseArrayLayer = dstz;
         region.dstSubresource.layerCount = src_box->depth;
      } else {
         region.dstOffset.z = dstz;
         region.dstSubresource.layerCount = 1;
      }

      region.dstOffset.x = dstx;
      region.dstOffset.y = dsty;
      region.extent.width = src_box->width;
      region.extent.height = src_box->height;

      struct zink_batch *batch = zink_batch_no_rp(ctx);
      zink_batch_reference_resource_rw(batch, src, false);
      zink_batch_reference_resource_rw(batch, dst, true);

      zink_resource_setup_transfer_layouts(ctx, src, dst);
      vkCmdCopyImage(batch->state->cmdbuf, src->obj->image, src->layout,
                     dst->obj->image, dst->layout,
                     1, &region);
   } else if (dst->base.b.target == PIPE_BUFFER &&
              src->base.b.target == PIPE_BUFFER) {
      zink_copy_buffer(ctx, NULL, dst, src, dstx, src_box->x, src_box->width);
   } else
      zink_copy_image_buffer(ctx, NULL, dst, src, dst_level, dstx, dsty, dstz, src_level, src_box, 0);
}

static struct pipe_stream_output_target *
zink_create_stream_output_target(struct pipe_context *pctx,
                                 struct pipe_resource *pres,
                                 unsigned buffer_offset,
                                 unsigned buffer_size)
{
   struct zink_so_target *t;
   t = CALLOC_STRUCT(zink_so_target);
   if (!t)
      return NULL;

   /* using PIPE_BIND_CUSTOM here lets us create a custom pipe buffer resource,
    * which allows us to differentiate and use VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT
    * as we must for this case
    */
   t->counter_buffer = pipe_buffer_create(pctx->screen, PIPE_BIND_STREAM_OUTPUT | PIPE_BIND_CUSTOM, PIPE_USAGE_DEFAULT, 4);
   if (!t->counter_buffer) {
      FREE(t);
      return NULL;
   }

   t->base.reference.count = 1;
   t->base.context = pctx;
   pipe_resource_reference(&t->base.buffer, pres);
   t->base.buffer_offset = buffer_offset;
   t->base.buffer_size = buffer_size;

   struct zink_resource *res = zink_resource(pres);
   util_range_add(pres, &res->valid_buffer_range, buffer_offset,
                  buffer_offset + buffer_size);
   return &t->base;
}

static void
zink_stream_output_target_destroy(struct pipe_context *pctx,
                                  struct pipe_stream_output_target *psot)
{
   struct zink_so_target *t = (struct zink_so_target *)psot;
   pipe_resource_reference(&t->counter_buffer, NULL);
   pipe_resource_reference(&t->base.buffer, NULL);
   FREE(t);
}

static void
zink_set_stream_output_targets(struct pipe_context *pctx,
                               unsigned num_targets,
                               struct pipe_stream_output_target **targets,
                               const unsigned *offsets)
{
   struct zink_context *ctx = zink_context(pctx);

   if (num_targets == 0) {
      for (unsigned i = 0; i < ctx->num_so_targets; i++)
         pipe_so_target_reference(&ctx->so_targets[i], NULL);
      ctx->num_so_targets = 0;
   } else {
      for (unsigned i = 0; i < num_targets; i++) {
         struct zink_so_target *t = zink_so_target(targets[i]);
         pipe_so_target_reference(&ctx->so_targets[i], targets[i]);
         if (!t)
            continue;
         struct zink_resource *res = zink_resource(t->counter_buffer);
         if (offsets[0] == (unsigned)-1)
            ctx->xfb_barrier |= zink_resource_buffer_needs_barrier(res,
                                                                   VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
                                                                   VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
         else
            ctx->xfb_barrier |= zink_resource_buffer_needs_barrier(res,
                                                                   VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
                                                                   VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT);
      }
      for (unsigned i = num_targets; i < ctx->num_so_targets; i++)
         pipe_so_target_reference(&ctx->so_targets[i], NULL);
      ctx->num_so_targets = num_targets;

      /* TODO: possibly avoid rebinding on resume if resuming from same buffers? */
      ctx->dirty_so_targets = true;
   }
}

void
zink_resource_rebind(struct zink_context *ctx, struct zink_resource *res)
{
   assert(res->base.b.target == PIPE_BUFFER);

   for (unsigned shader = 0; shader < PIPE_SHADER_TYPES; shader++) {
      if (!(res->bind_stages & (1 << shader)))
         continue;
      for (enum zink_descriptor_type type = 0; type < ZINK_DESCRIPTOR_TYPES; type++) {
         if (!(res->bind_history & BITFIELD64_BIT(type)))
            continue;

         uint32_t usage = zink_program_get_descriptor_usage(ctx, shader, type);
         while (usage) {
            const int i = u_bit_scan(&usage);
            struct zink_resource *cres = get_resource_for_descriptor(ctx, type, shader, i);
            if (res != cres)
               continue;

            switch (type) {
            case ZINK_DESCRIPTOR_TYPE_SSBO: {
               struct pipe_shader_buffer *ssbo = &ctx->ssbos[shader][i];
               util_range_add(&res->base.b, &res->valid_buffer_range, ssbo->buffer_offset,
                              ssbo->buffer_offset + ssbo->buffer_size);
               break;
            }
            case ZINK_DESCRIPTOR_TYPE_SAMPLER_VIEW: {
               struct zink_sampler_view *sampler_view = zink_sampler_view(ctx->sampler_views[shader][i]);
               sampler_view_buffer_clear(ctx, sampler_view);
               sampler_view->buffer_view = get_buffer_view(ctx, res, sampler_view->base.format,
                                                           sampler_view->base.u.buf.offset, sampler_view->base.u.buf.size);
               break;
            }
            case ZINK_DESCRIPTOR_TYPE_IMAGE: {
               struct zink_image_view *image_view = &ctx->image_views[shader][i];
               zink_descriptor_set_refs_clear(&image_view->desc_set_refs, image_view);
               zink_buffer_view_reference(ctx, &image_view->buffer_view, NULL);
               if (!zink_resource_object_init_storage(zink_screen(ctx->base.screen), res)) {
                  debug_printf("couldn't create storage image!");
                  continue;
               }
               image_view->buffer_view = get_buffer_view(ctx, res, image_view->base.format,
                                                         image_view->base.u.buf.offset, image_view->base.u.buf.size);
               assert(image_view->buffer_view);
               util_range_add(&res->base.b, &res->valid_buffer_range, image_view->base.u.buf.offset,
                              image_view->base.u.buf.offset + image_view->base.u.buf.size);
               break;
            }
            default:
               break;
            }

            invalidate_descriptor_state(ctx, shader, type);
         }
      }
   }
}

static void
zink_context_replace_buffer_storage(struct pipe_context *pctx, struct pipe_resource *dst, struct pipe_resource *src)
{
   struct zink_resource *d = zink_resource(dst);
   struct zink_resource *s = zink_resource(src);

   assert(d->internal_format == s->internal_format);
   zink_resource_object_reference(zink_screen(pctx->screen), &d->obj, s->obj);
   d->access = s->access;
   d->access_stage = s->access_stage;
   zink_resource_rebind(zink_context(pctx), d);
}

struct pipe_context *
zink_context_create(struct pipe_screen *pscreen, void *priv, unsigned flags)
{
   struct zink_screen *screen = zink_screen(pscreen);
   struct zink_context *ctx = rzalloc(NULL, struct zink_context);
   if (!ctx)
      goto fail;

   ctx->gfx_pipeline_state.hash = 0;

   ctx->base.screen = pscreen;
   ctx->base.priv = priv;

   ctx->base.destroy = zink_context_destroy;
   ctx->base.get_device_reset_status = zink_get_device_reset_status;
   ctx->base.set_device_reset_callback = zink_set_device_reset_callback;

   zink_context_state_init(&ctx->base);

   ctx->base.create_sampler_state = zink_create_sampler_state;
   ctx->base.bind_sampler_states = zink_bind_sampler_states;
   ctx->base.delete_sampler_state = zink_delete_sampler_state;

   ctx->base.create_sampler_view = zink_create_sampler_view;
   ctx->base.set_sampler_views = zink_set_sampler_views;
   ctx->base.sampler_view_destroy = zink_sampler_view_destroy;
   ctx->base.get_sample_position = zink_get_sample_position;

   zink_program_init(ctx);

   ctx->base.set_polygon_stipple = zink_set_polygon_stipple;
   ctx->base.set_vertex_buffers = zink_set_vertex_buffers;
   ctx->base.set_viewport_states = zink_set_viewport_states;
   ctx->base.set_scissor_states = zink_set_scissor_states;
   ctx->base.set_constant_buffer = zink_set_constant_buffer;
   ctx->base.set_shader_buffers = zink_set_shader_buffers;
   ctx->base.set_shader_images = zink_set_shader_images;
   ctx->base.set_framebuffer_state = zink_set_framebuffer_state;
   ctx->base.set_stencil_ref = zink_set_stencil_ref;
   ctx->base.set_clip_state = zink_set_clip_state;
   ctx->base.set_blend_color = zink_set_blend_color;
   ctx->base.set_tess_state = zink_set_tess_state;

   ctx->base.set_sample_mask = zink_set_sample_mask;

   ctx->base.clear = zink_clear;
   ctx->base.clear_texture = zink_clear_texture;

   ctx->base.draw_vbo = zink_draw_vbo;
   ctx->base.launch_grid = zink_launch_grid;
   ctx->base.fence_server_sync = zink_fence_server_sync;
   ctx->base.flush = zink_flush;
   ctx->base.memory_barrier = zink_memory_barrier;
   ctx->base.texture_barrier = zink_texture_barrier;

   ctx->base.resource_copy_region = zink_resource_copy_region;
   ctx->base.blit = zink_blit;
   ctx->base.create_stream_output_target = zink_create_stream_output_target;
   ctx->base.stream_output_target_destroy = zink_stream_output_target_destroy;

   ctx->base.set_stream_output_targets = zink_set_stream_output_targets;
   ctx->base.flush_resource = zink_flush_resource;
   zink_context_surface_init(&ctx->base);
   zink_context_resource_init(&ctx->base);
   zink_context_query_init(&ctx->base);

   _mesa_hash_table_init(&ctx->surface_cache, ctx, NULL, equals_ivci);
   _mesa_hash_table_init(&ctx->bufferview_cache, ctx, NULL, equals_bvci);
   simple_mtx_init(&ctx->surface_mtx, mtx_plain);
   simple_mtx_init(&ctx->bufferview_mtx, mtx_plain);
   util_dynarray_init(&ctx->free_batch_states, ctx);
   _mesa_hash_table_init(&ctx->batch_states, ctx, NULL, _mesa_key_pointer_equal);

   ctx->gfx_pipeline_state.have_EXT_extended_dynamic_state = screen->info.have_EXT_extended_dynamic_state;

   slab_create_child(&ctx->transfer_pool, &screen->transfer_pool);
   slab_create_child(&ctx->transfer_pool_unsync, &screen->transfer_pool);

   ctx->base.stream_uploader = u_upload_create_default(&ctx->base);
   ctx->base.const_uploader = ctx->base.stream_uploader;
   for (int i = 0; i < ARRAY_SIZE(ctx->fb_clears); i++)
      util_dynarray_init(&ctx->fb_clears[i].clears, ctx);

   int prim_hwsupport = 1 << PIPE_PRIM_POINTS |
                        1 << PIPE_PRIM_LINES |
                        1 << PIPE_PRIM_LINE_STRIP |
                        1 << PIPE_PRIM_TRIANGLES |
                        1 << PIPE_PRIM_TRIANGLE_STRIP;
   if (screen->have_triangle_fans)
      prim_hwsupport |= 1 << PIPE_PRIM_TRIANGLE_FAN;

   ctx->primconvert = util_primconvert_create(&ctx->base, prim_hwsupport);
   if (!ctx->primconvert)
      goto fail;

   ctx->blitter = util_blitter_create(&ctx->base);
   if (!ctx->blitter)
      goto fail;

   vkGetDeviceQueue(screen->dev, screen->gfx_queue, 0, &ctx->batch.queue);

   incr_curr_batch(ctx);
   zink_start_batch(ctx, &ctx->batch);
   if (!ctx->batch.state)
      goto fail;

   ctx->program_cache = _mesa_hash_table_create(NULL,
                                                hash_gfx_program,
                                                equals_gfx_program);
   ctx->compute_program_cache = _mesa_hash_table_create(NULL,
                                                _mesa_hash_uint,
                                                _mesa_key_uint_equal);
   ctx->render_pass_cache = _mesa_hash_table_create(NULL,
                                                    hash_render_pass_state,
                                                    equals_render_pass_state);
   if (!ctx->program_cache || !ctx->compute_program_cache || !ctx->render_pass_cache)
      goto fail;

   const uint8_t data[] = { 0 };
   ctx->dummy_vertex_buffer = pipe_buffer_create_with_data(&ctx->base,
      PIPE_BIND_VERTEX_BUFFER, PIPE_USAGE_IMMUTABLE, sizeof(data), data);
   if (!ctx->dummy_vertex_buffer)
      goto fail;
   ctx->dummy_xfb_buffer = pipe_buffer_create_with_data(&ctx->base,
      PIPE_BIND_STREAM_OUTPUT, PIPE_USAGE_DEFAULT, sizeof(data), data);
   if (!ctx->dummy_xfb_buffer)
      goto fail;

   if (!zink_descriptor_pool_init(ctx))
      goto fail;

   if (!(flags & PIPE_CONTEXT_PREFER_THREADED) || flags & PIPE_CONTEXT_COMPUTE_ONLY) {
      return &ctx->base;
   }

   struct threaded_context *tc = (struct threaded_context*)threaded_context_create(&ctx->base, &screen->transfer_pool,
                                                     zink_context_replace_buffer_storage,
                                                     zink_create_tc_fence_for_tc, &ctx->tc);

   if (tc && (struct zink_context*)tc != ctx) {
      tc->bytes_mapped_limit = screen->total_mem / 4;
   }
   ctx->threaded = true;

   return (struct pipe_context*)tc;

fail:
   if (ctx)
      zink_context_destroy(&ctx->base);
   return NULL;
}
