#include "zink_context.h"
#include "zink_helpers.h"
#include "zink_resource.h"
#include "zink_screen.h"

#include "util/u_blitter.h"
#include "util/u_surface.h"
#include "util/format/u_format.h"

static bool
blit_resolve(struct zink_context *ctx, const struct pipe_blit_info *info)
{
   if (util_format_get_mask(info->dst.format) != info->mask ||
       util_format_get_mask(info->src.format) != info->mask ||
       util_format_is_depth_or_stencil(info->dst.format) ||
       info->scissor_enable ||
       info->alpha_blend ||
       info->render_condition_enable)
      return false;

   struct zink_resource *src = zink_resource(info->src.resource);
   struct zink_resource *dst = zink_resource(info->dst.resource);

   struct zink_screen *screen = zink_screen(ctx->base.screen);
   if (src->format != zink_get_format(screen, info->src.format) ||
       dst->format != zink_get_format(screen, info->dst.format))
      return false;

   struct zink_batch *batch = zink_batch_no_rp(ctx);

   zink_batch_reference_resource_rw(batch, src, false);
   zink_batch_reference_resource_rw(batch, dst, true);

   assert(src != dst);
   zink_resource_setup_transfer_layouts(batch, src, dst);

   VkImageResolve region = {};

   region.srcSubresource.aspectMask = src->aspect;
   region.srcSubresource.mipLevel = info->src.level;
   region.srcSubresource.baseArrayLayer = 0; // no clue
   region.srcSubresource.layerCount = 1; // no clue
   region.srcOffset.x = info->src.box.x;
   region.srcOffset.y = info->src.box.y;
   region.srcOffset.z = info->src.box.z;

   region.dstSubresource.aspectMask = dst->aspect;
   region.dstSubresource.mipLevel = info->dst.level;
   region.dstSubresource.baseArrayLayer = 0; // no clue
   region.dstSubresource.layerCount = 1; // no clue
   region.dstOffset.x = info->dst.box.x;
   region.dstOffset.y = info->dst.box.y;
   region.dstOffset.z = info->dst.box.z;

   region.extent.width = info->dst.box.width;
   region.extent.height = info->dst.box.height;
   region.extent.depth = info->dst.box.depth;
   vkCmdResolveImage(batch->cmdbuf, src->image, src->layout,
                     dst->image, dst->layout,
                     1, &region);

   return true;
}

static bool
blit_native(struct zink_context *ctx, const struct pipe_blit_info *info)
{
   if (util_format_get_mask(info->dst.format) != info->mask ||
       util_format_get_mask(info->src.format) != info->mask ||
       info->scissor_enable ||
       info->alpha_blend ||
       info->render_condition_enable)
      return false;

   if (util_format_is_depth_or_stencil(info->dst.format) &&
       info->dst.format != info->src.format)
      return false;

   /* vkCmdBlitImage must not be used for multisampled source or destination images. */
   if (info->src.resource->nr_samples > 1 || info->dst.resource->nr_samples > 1)
      return false;

   struct zink_resource *src = zink_resource(info->src.resource);
   struct zink_resource *dst = zink_resource(info->dst.resource);

   struct zink_screen *screen = zink_screen(ctx->base.screen);
   if (src->format != zink_get_format(screen, info->src.format) ||
       dst->format != zink_get_format(screen, info->dst.format))
      return false;

   struct zink_batch *batch = zink_batch_no_rp(ctx);
   zink_batch_reference_resource_rw(batch, src, false);
   zink_batch_reference_resource_rw(batch, dst, true);

   VkImageBlit region = {};
   region.srcSubresource.aspectMask = src->aspect;
   region.srcSubresource.mipLevel = info->src.level;
   region.srcOffsets[0].x = info->src.box.x;
   region.srcOffsets[0].y = info->src.box.y;
   region.srcOffsets[1].x = info->src.box.x + info->src.box.width;
   region.srcOffsets[1].y = info->src.box.y + info->src.box.height;

   if (src->base.array_size > 1) {
      region.srcOffsets[0].z = 0;
      region.srcOffsets[1].z = 1;
      region.srcSubresource.baseArrayLayer = info->src.box.z;
      region.srcSubresource.layerCount = info->src.box.depth;
   } else {
      region.srcOffsets[0].z = info->src.box.z;
      region.srcOffsets[1].z = info->src.box.z + info->src.box.depth;
      region.srcSubresource.baseArrayLayer = 0;
      region.srcSubresource.layerCount = 1;
   }

   region.dstSubresource.aspectMask = dst->aspect;
   region.dstSubresource.mipLevel = info->dst.level;
   region.dstOffsets[0].x = info->dst.box.x;
   region.dstOffsets[0].y = info->dst.box.y;
   region.dstOffsets[1].x = info->dst.box.x + info->dst.box.width;
   region.dstOffsets[1].y = info->dst.box.y + info->dst.box.height;

   if (dst->base.array_size > 1) {
      region.dstOffsets[0].z = 0;
      region.dstOffsets[1].z = 1;
      region.dstSubresource.baseArrayLayer = info->dst.box.z;
      region.dstSubresource.layerCount = info->dst.box.depth;
   } else {
      region.dstOffsets[0].z = info->dst.box.z;
      region.dstOffsets[1].z = info->dst.box.z + info->dst.box.depth;
      region.dstSubresource.baseArrayLayer = 0;
      region.dstSubresource.layerCount = 1;
   }

   bool same_subresource_range = false;
   if (src == dst) {
      same_subresource_range = info->src.level == info->dst.level &&
          util_test_intersection(region.srcSubresource.baseArrayLayer,
                                 region.srcSubresource.baseArrayLayer +
                                 region.srcSubresource.layerCount,
                                 region.dstSubresource.baseArrayLayer,
                                 region.dstSubresource.baseArrayLayer +
                                 region.dstSubresource.layerCount);
      if (!same_subresource_range) {
         /* The Vulkan 1.1 specification says the following about valid usage
          * of vkCmdBlitImage:
          *
          * "srcImageLayout must be VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
          *  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL"
          *
          * and:
          *
          * "dstImageLayout must be VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
          *  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL"
          *
          * Since we cant have the same image in two states at the same time,
          * we're effectively left with VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR or
          * VK_IMAGE_LAYOUT_GENERAL. And since this isn't a present-related
          * operation, VK_IMAGE_LAYOUT_GENERAL seems most appropriate.
          */

         zink_resource_barrier(batch->cmdbuf, src, src->aspect,
                              VK_IMAGE_LAYOUT_GENERAL);

      } else {
         /* Since we only track a single layout per resource, we need to
          * temporarily whack these into the right layout, and back again.
          */

         if (src->layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            zink_resource_barrier_range(batch->cmdbuf, src, src->aspect,
                                       src->layout,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       info->src.level, 1,
                                       region.srcSubresource.baseArrayLayer,
                                       region.srcSubresource.layerCount);

         if (dst->layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            zink_resource_barrier_range(batch->cmdbuf, dst, dst->aspect,
                                       dst->layout,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       info->dst.level, 1,
                                       region.dstSubresource.baseArrayLayer,
                                       region.dstSubresource.layerCount);
      }
   } else
      zink_resource_setup_transfer_layouts(batch, src, dst);

   vkCmdBlitImage(batch->cmdbuf, src->image, src->layout,
                  dst->image, dst->layout,
                  1, &region,
                  zink_filter(info->filter));

   if (same_subresource_range) {
      /* restore the layouts back to their tracked state */
      if (src->layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
         zink_resource_barrier_range(batch->cmdbuf, src, src->aspect,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     src->layout,
                                     info->src.level, 1,
                                     region.srcSubresource.baseArrayLayer,
                                     region.srcSubresource.layerCount);
      if (dst->layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
         zink_resource_barrier_range(batch->cmdbuf, dst, dst->aspect,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     dst->layout,
                                     info->dst.level, 1,
                                     region.dstSubresource.baseArrayLayer,
                                     region.dstSubresource.layerCount);
   }

   return true;
}

void
zink_blit(struct pipe_context *pctx,
          const struct pipe_blit_info *info)
{
   struct zink_context *ctx = zink_context(pctx);
   if (info->src.resource->nr_samples > 1 &&
       info->dst.resource->nr_samples <= 1) {
      if (blit_resolve(ctx, info))
         return;
   } else {
      if (blit_native(ctx, info))
         return;
   }

   struct zink_resource *src = zink_resource(info->src.resource);
   struct zink_resource *dst = zink_resource(info->dst.resource);
   /* if we're copying between resources with matching aspects then we can probably just copy_region */
   if (src->aspect == dst->aspect && util_try_blit_via_copy_region(pctx, info))
      return;

   if (!util_blitter_is_blit_supported(ctx->blitter, info)) {
      debug_printf("blit unsupported %s -> %s\n",
              util_format_short_name(info->src.resource->format),
              util_format_short_name(info->dst.resource->format));
      return;
   }

   util_blitter_save_blend(ctx->blitter, ctx->gfx_pipeline_state.blend_state);
   util_blitter_save_depth_stencil_alpha(ctx->blitter, ctx->dsa_state);
   util_blitter_save_vertex_elements(ctx->blitter, ctx->element_state);
   util_blitter_save_stencil_ref(ctx->blitter, &ctx->stencil_ref);
   util_blitter_save_rasterizer(ctx->blitter, ctx->rast_state);
   util_blitter_save_fragment_shader(ctx->blitter, ctx->gfx_stages[PIPE_SHADER_FRAGMENT]);
   util_blitter_save_vertex_shader(ctx->blitter, ctx->gfx_stages[PIPE_SHADER_VERTEX]);
   util_blitter_save_geometry_shader(ctx->blitter, ctx->gfx_stages[PIPE_SHADER_GEOMETRY]);
   util_blitter_save_framebuffer(ctx->blitter, &ctx->fb_state);
   util_blitter_save_viewport(ctx->blitter, ctx->viewport_states);
   util_blitter_save_scissor(ctx->blitter, ctx->scissor_states);
   util_blitter_save_fragment_sampler_states(ctx->blitter,
                                             ctx->num_samplers[PIPE_SHADER_FRAGMENT],
                                             ctx->sampler_states[PIPE_SHADER_FRAGMENT]);
   util_blitter_save_fragment_sampler_views(ctx->blitter,
                                            ctx->num_image_views[PIPE_SHADER_FRAGMENT],
                                            ctx->image_views[PIPE_SHADER_FRAGMENT]);
   util_blitter_save_fragment_constant_buffer_slot(ctx->blitter, ctx->ubos[PIPE_SHADER_FRAGMENT]);
   util_blitter_save_vertex_buffer_slot(ctx->blitter, ctx->buffers);
   util_blitter_save_sample_mask(ctx->blitter, ctx->gfx_pipeline_state.sample_mask);
   util_blitter_save_so_targets(ctx->blitter, ctx->num_so_targets, ctx->so_targets);

   util_blitter_blit(ctx->blitter, info);
}

void
zink_resource_copy_region(struct pipe_context *pctx,
                          struct pipe_resource *pdst,
                          unsigned dst_level, unsigned dstx,
                          unsigned dsty, unsigned dstz,
                          struct pipe_resource *psrc,
                          unsigned src_level,
                          const struct pipe_box *src_box)
{
   struct zink_resource *dst = zink_resource(pdst);
   struct zink_resource *src = zink_resource(psrc);
   struct zink_context *ctx = zink_context(pctx);
   if (dst->base.target != PIPE_BUFFER &&
       src->base.target != PIPE_BUFFER) {
      VkImageCopy region = {};
      if (util_format_get_num_planes(src->base.format) == 1 &&
          util_format_get_num_planes(dst->base.format) == 1) {
      /* If neither the calling command’s srcImage nor the calling command’s dstImage
       * has a multi-planar image format then the aspectMask member of srcSubresource
       * and dstSubresource must match
       *
       * -VkImageCopy spec
       */
         assert(src->aspect == dst->aspect);
      } else
         unreachable("planar formats not yet handled");

      region.srcSubresource.aspectMask = src->aspect;
      region.srcSubresource.mipLevel = src_level;
      region.srcSubresource.layerCount = 1;
      if (src->base.array_size > 1) {
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
      if (dst->base.array_size > 1) {
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

      bool same_subresource_range = false;
      if (src == dst) {
         same_subresource_range = src_level == dst_level &&
            util_test_intersection(region.srcSubresource.baseArrayLayer,
                                    region.srcSubresource.baseArrayLayer +
                                    region.srcSubresource.layerCount,
                                    region.dstSubresource.baseArrayLayer,
                                    region.dstSubresource.baseArrayLayer +
                                    region.dstSubresource.layerCount);
         if (!same_subresource_range) {
            /* The Vulkan 1.1 specification says the following about valid usage
            * of vkCmdBlitImage:
            *
            * "srcImageLayout must be VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
            *  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL"
            *
            * and:
            *
            * "dstImageLayout must be VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
            *  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL"
            *
            * Since we cant have the same image in two states at the same time,
            * we're effectively left with VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR or
            * VK_IMAGE_LAYOUT_GENERAL. And since this isn't a present-related
            * operation, VK_IMAGE_LAYOUT_GENERAL seems most appropriate.
            */

            zink_resource_barrier(batch->cmdbuf, src, src->aspect,
                                 VK_IMAGE_LAYOUT_GENERAL);

         } else {
            /* Since we only track a single layout per resource, we need to
            * temporarily whack these into the right layout, and back again.
            */

            if (src->layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
               zink_resource_barrier_range(batch->cmdbuf, src, src->aspect,
                                          src->layout,
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                          src_level, 1,
                                          region.srcSubresource.baseArrayLayer,
                                          region.srcSubresource.layerCount);

            if (dst->layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
               zink_resource_barrier_range(batch->cmdbuf, dst, dst->aspect,
                                          dst->layout,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          dst_level, 1,
                                          region.dstSubresource.baseArrayLayer,
                                          region.dstSubresource.layerCount);
         }
      } else
         zink_resource_setup_transfer_layouts(batch, src, dst);

      vkCmdCopyImage(batch->cmdbuf, src->image, src->layout,
                     dst->image, dst->layout,
                     1, &region);

      if (same_subresource_range) {
         /* restore the layouts back to their tracked state */
         if (src->layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            zink_resource_barrier_range(batch->cmdbuf, src, src->aspect,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       src->layout,
                                       src_level, 1,
                                       region.srcSubresource.baseArrayLayer,
                                       region.srcSubresource.layerCount);
         if (dst->layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            zink_resource_barrier_range(batch->cmdbuf, dst, dst->aspect,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       dst->layout,
                                       dst_level, 1,
                                       region.dstSubresource.baseArrayLayer,
                                       region.dstSubresource.layerCount);
      }

   } else if (dst->base.target == PIPE_BUFFER &&
              src->base.target == PIPE_BUFFER) {
      VkBufferCopy region;
      region.srcOffset = src_box->x;
      region.dstOffset = dstx;
      region.size = src_box->width;

      struct zink_batch *batch = zink_batch_no_rp(ctx);
      zink_batch_reference_resource_rw(batch, src, false);
      zink_batch_reference_resource_rw(batch, dst, true);

      vkCmdCopyBuffer(batch->cmdbuf, src->buffer, dst->buffer, 1, &region);
   } else
      debug_printf("zink: TODO resource copy\n");
}
