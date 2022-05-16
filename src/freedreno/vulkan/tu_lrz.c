/*
 * Copyright © 2022 Igalia S.L.
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

#include "tu_private.h"

#include "tu_cs.h"

/* Low-resolution Z buffer is very similar to a depth prepass that helps
 * the HW avoid executing the fragment shader on those fragments that will
 * be subsequently discarded by the depth test afterwards.
 *
 * The interesting part of this feature is that it allows applications
 * to submit the vertices in any order.
 *
 * In the binning pass it is possible to store the depth value of each
 * vertex into internal low resolution depth buffer and quickly test
 * the primitives against it during the render pass.
 *
 * There are a number of limitations when LRZ cannot be used:
 * - Fragment shader side-effects (writing to SSBOs, atomic operations, etc);
 * - Writing to stencil buffer
 * - Writing depth while:
 *   - Changing direction of depth test (e.g. from OP_GREATER to OP_LESS);
 *   - Using OP_ALWAYS or OP_NOT_EQUAL;
 * - Clearing depth with vkCmdClearAttachments;
 * - (pre-a650) Not clearing depth attachment with LOAD_OP_CLEAR;
 * - (pre-a650) Using secondary command buffers;
 * - Sysmem rendering (with small caveat).
 *
 * Pre-a650 (before gen3)
 * ======================
 *
 * The direction is fully tracked on CPU. In renderpass LRZ starts with
 * unknown direction, the direction is set first time when depth write occurs
 * and if it does change afterwards - direction becomes invalid and LRZ is
 * disabled for the rest of the renderpass.
 *
 * Since direction is not tracked by GPU - it's impossible to know whether
 * LRZ is enabled during construction of secondary command buffers.
 *
 * For the same reason it's impossible to reuse LRZ between renderpasses.
 *
 * A650+ (gen3+)
 * =============
 *
 * Now LRZ direction could be tracked on GPU. There are to parts:
 * - Direction byte which stores current LRZ direction;
 * - Parameters of the last used depth view.
 *
 * The idea is the same as when LRZ tracked on CPU: when GRAS_LRZ_CNTL
 * is used - its direction is compared to previously known direction
 * and direction byte is set to disabled when directions are incompatible.
 *
 * Additionally, to reuse LRZ between renderpasses, GRAS_LRZ_CNTL checks
 * if current value of GRAS_LRZ_DEPTH_VIEW is equal to the value
 * stored in the buffer, if not - LRZ is disabled. (This is necessary
 * because depth buffer may have several layers and mip levels, on the
 * other hand LRZ buffer represents only a single layer + mip level).
 *
 * LRZ direction between renderpasses is disabled when underlying depth
 * buffer is changed, the following commands could change depth image:
 * - vkCmdBlitImage*
 * - vkCmdCopyBufferToImage*
 * - vkCmdCopyImage*
 *
 * LRZ Fast-Clear
 * ==============
 *
 * The LRZ fast-clear buffer is initialized to zeroes and read/written
 * when GRAS_LRZ_CNTL.FC_ENABLE (b3) is set. It appears to store 1b/block.
 * '0' means block has original depth clear value, and '1' means that the
 * corresponding block in LRZ has been modified.
 *
 * LRZ Caches
 * ==========
 *
 * LRZ_FLUSH flushes and invalidates LRZ caches, there are two caches:
 * - Cache for fast-clear buffer;
 * - Cache for direction byte + depth view params.
 * They could be cleared by LRZ_CLEAR. To become visible in GPU memory
 * the caches should be flushed with LRZ_FLUSH afterwards.
 *
 * GRAS_LRZ_CNTL reads from these caches.
 */

static void
tu6_emit_lrz_buffer(struct tu_cs *cs, struct tu_image *depth_image)
{
   if (!depth_image) {
      tu_cs_emit_regs(cs,
                      A6XX_GRAS_LRZ_BUFFER_BASE(0),
                      A6XX_GRAS_LRZ_BUFFER_PITCH(0),
                      A6XX_GRAS_LRZ_FAST_CLEAR_BUFFER_BASE(0));
      return;
   }

   uint64_t lrz_iova = depth_image->iova + depth_image->lrz_offset;
   uint64_t lrz_fc_iova = depth_image->iova + depth_image->lrz_fc_offset;
   if (!depth_image->lrz_fc_offset)
      lrz_fc_iova = 0;

   tu_cs_emit_regs(cs,
                   A6XX_GRAS_LRZ_BUFFER_BASE(.qword = lrz_iova),
                   A6XX_GRAS_LRZ_BUFFER_PITCH(.pitch = depth_image->lrz_pitch),
                   A6XX_GRAS_LRZ_FAST_CLEAR_BUFFER_BASE(.qword = lrz_fc_iova));
}

static void
tu6_write_lrz_reg(struct tu_cmd_buffer *cmd, struct tu_cs *cs,
                  struct tu_reg_value reg)
{
   if (cmd->device->physical_device->info->a6xx.lrz_track_quirk) {
      tu_cs_emit_pkt7(cs, CP_REG_WRITE, 3);
      tu_cs_emit(cs, CP_REG_WRITE_0_TRACKER(TRACK_LRZ));
      tu_cs_emit(cs, reg.reg);
      tu_cs_emit(cs, reg.value);
   } else {
      tu_cs_emit_pkt4(cs, reg.reg, 1);
      tu_cs_emit(cs, reg.value);
   }
}

static void
tu6_disable_lrz_via_depth_view(struct tu_cmd_buffer *cmd, struct tu_cs *cs)
{
   /* Disable direction by writing invalid depth view. */
   tu6_write_lrz_reg(cmd, cs, A6XX_GRAS_LRZ_DEPTH_VIEW(
      .base_layer = 0b11111111111,
      .layer_count = 0b11111111111,
      .base_mip_level = 0b1111,
   ));

   tu6_write_lrz_reg(cmd, cs, A6XX_GRAS_LRZ_CNTL(
      .enable = true,
      .disable_on_wrong_dir = true,
   ));

   tu6_emit_event_write(cmd, cs, LRZ_CLEAR);
   tu6_emit_event_write(cmd, cs, LRZ_FLUSH);
}

static void
tu_lrz_init_state(struct tu_cmd_buffer *cmd,
                  const struct tu_render_pass_attachment *att,
                  const struct tu_image_view *view)
{
   if (!view->image->lrz_height)
      return;

   bool clears_depth = att->clear_mask &
      (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT);
   bool has_gpu_tracking =
      cmd->device->physical_device->info->a6xx.has_lrz_dir_tracking;

   if (!has_gpu_tracking && !clears_depth)
      return;

   if (!clears_depth && !att->load)
      return;

   cmd->state.lrz.image_view = view;
   cmd->state.lrz.valid = true;
   cmd->state.lrz.prev_direction = TU_LRZ_UNKNOWN;
   /* Be optimistic and unconditionally enable fast-clear in
    * secondary cmdbufs and when reusing previous LRZ state.
    */
   cmd->state.lrz.fast_clear = view->image->lrz_fc_size > 0;

   cmd->state.lrz.gpu_dir_tracking = has_gpu_tracking;
   cmd->state.lrz.reuse_previous_state = !clears_depth;
}

void
tu_lrz_begin_renderpass(struct tu_cmd_buffer *cmd,
                        const VkRenderPassBeginInfo *pRenderPassBegin)
{
   const struct tu_render_pass *pass = cmd->state.pass;

   int lrz_img_count = 0;
   for (unsigned i = 0; i < pass->attachment_count; i++) {
      if (cmd->state.attachments[i]->image->lrz_height)
         lrz_img_count++;
   }

   if (cmd->device->physical_device->info->a6xx.has_lrz_dir_tracking &&
       cmd->state.pass->subpass_count > 1 && lrz_img_count > 1) {
      /* We cannot support LRZ if subpasses have different depth
       * attachments. We'd have to split renderpass to do this.
       */

      for (unsigned i = 0; i < pass->attachment_count; i++) {
         struct tu_image *image = cmd->state.attachments[i]->image;
         tu_disable_lrz(cmd, &cmd->cs, image);
      }
   }

    /* Track LRZ valid state */
   cmd->state.lrz.valid = false;
   uint32_t a = cmd->state.subpass->depth_stencil_attachment.attachment;
   if (a != VK_ATTACHMENT_UNUSED) {
      const struct tu_render_pass_attachment *att = &cmd->state.pass->attachments[a];
      tu_lrz_init_state(cmd, att, cmd->state.attachments[a]);
      if (att->clear_mask & (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT)) {
         VkClearValue clear = pRenderPassBegin->pClearValues[a];
         cmd->state.lrz.depth_clear_value = clear;
         cmd->state.lrz.fast_clear = cmd->state.lrz.fast_clear &&
                                     (clear.depthStencil.depth == 0.f ||
                                      clear.depthStencil.depth == 1.f);
      }
      cmd->state.dirty |= TU_CMD_DIRTY_LRZ;
   }

   if (!cmd->state.lrz.valid) {
      memset(&cmd->state.lrz, 0, sizeof(cmd->state.lrz));
      tu6_emit_lrz_buffer(&cmd->cs, NULL);
   }
}

void
tu_lrz_begin_secondary_cmdbuf(struct tu_cmd_buffer *cmd,
                              struct tu_framebuffer *fb)
{
   uint32_t a = cmd->state.subpass->depth_stencil_attachment.attachment;
   if (a != VK_ATTACHMENT_UNUSED &&
       cmd->device->physical_device->info->a6xx.has_lrz_dir_tracking) {
      const struct tu_render_pass_attachment *att = &cmd->state.pass->attachments[a];
      struct tu_image_view *view = fb->attachments[a].attachment;

      tu_lrz_init_state(cmd, att, view);
   }
}

void
tu_lrz_tiling_begin(struct tu_cmd_buffer *cmd, struct tu_cs *cs)
{
   if (!cmd->state.lrz.image_view)
      return;

   struct tu_lrz_state *lrz = &cmd->state.lrz;

   tu6_emit_lrz_buffer(cs, lrz->image_view->image);

   if (lrz->reuse_previous_state) {
      /* Reuse previous LRZ state, LRZ cache is assumed to be
       * already invalidated by previous renderpass.
       */
      assert(lrz->gpu_dir_tracking);

      tu6_write_lrz_reg(cmd, cs,
         A6XX_GRAS_LRZ_DEPTH_VIEW(.dword = lrz->image_view->view.GRAS_LRZ_DEPTH_VIEW));
      return;
   }

   if (lrz->fast_clear || lrz->gpu_dir_tracking) {
      /* Following the blob we elect to disable LRZ for the whole renderpass
       * if it is known that LRZ is disabled somewhere in the renderpass.
       *
       * This is accomplished by making later GRAS_LRZ_CNTL (in binning pass)
       * to fail the comparison of depth views.
       */
      bool invalidate_lrz = !lrz->valid && lrz->gpu_dir_tracking;
      if (invalidate_lrz) {
         tu6_write_lrz_reg(cmd, cs, A6XX_GRAS_LRZ_DEPTH_VIEW(
            .base_layer = 0b11111111111,
            .layer_count = 0b11111111111,
            .base_mip_level = 0b1111,
         ));
      }

      if (lrz->valid && lrz->gpu_dir_tracking) {
         tu6_write_lrz_reg(cmd, cs,
            A6XX_GRAS_LRZ_DEPTH_VIEW(.dword = lrz->image_view->view.GRAS_LRZ_DEPTH_VIEW));
      }

      tu6_write_lrz_reg(cmd, cs, A6XX_GRAS_LRZ_CNTL(
         .enable = true,
         .fc_enable = lrz->fast_clear,
         .disable_on_wrong_dir = lrz->gpu_dir_tracking,
      ));

      /* LRZ_CLEAR.fc_enable + LRZ_CLEAR - clears fast-clear buffer;
       * LRZ_CLEAR.disable_on_wrong_dir + LRZ_CLEAR - sets direction to
       *  CUR_DIR_UNSET.
       */
      tu6_emit_event_write(cmd, cs, LRZ_CLEAR);

      if (invalidate_lrz) {
         tu6_emit_event_write(cmd, cs, LRZ_FLUSH);
         tu6_write_lrz_reg(cmd, cs,
            A6XX_GRAS_LRZ_DEPTH_VIEW(.dword = 0));
      }
   }

   if (!lrz->fast_clear) {
      /* Cache should be invalidated if LRZ buffer is manually changed. */
      tu6_emit_event_write(cmd, cs, LRZ_FLUSH);

      tu6_clear_lrz(cmd, cs, lrz->image_view->image, &lrz->depth_clear_value);

      /* Even though we disable fast-clear we still have to dirty
       * fast-clear buffer because both secondary cmdbufs and following
       * renderpasses won't know that fast-clear is disabled.
       *
       * TODO: we could avoid this if we don't store depth and don't
       * expect secondary cmdbufs.
       */
      if (lrz->image_view->image->lrz_fc_size) {
         tu6_dirty_lrz_fc(cmd, cs, lrz->image_view->image);
      }

      /* Clearing writes via CCU color in the PS stage, and LRZ is read via
       * UCHE in the earlier GRAS stage.
       */
      cmd->state.cache.flush_bits |=
         TU_CMD_FLAG_CCU_FLUSH_COLOR | TU_CMD_FLAG_CACHE_INVALIDATE |
         TU_CMD_FLAG_WAIT_FOR_IDLE;
   }
}

void
tu_lrz_tiling_end(struct tu_cmd_buffer *cmd, struct tu_cs *cs)
{
   if (cmd->state.lrz.fast_clear || cmd->state.lrz.gpu_dir_tracking) {
      tu6_emit_lrz_buffer(cs, cmd->state.lrz.image_view->image);

      if (cmd->state.lrz.gpu_dir_tracking) {
         tu6_write_lrz_reg(cmd, &cmd->cs,
            A6XX_GRAS_LRZ_DEPTH_VIEW(.dword = cmd->state.lrz.image_view->view.GRAS_LRZ_DEPTH_VIEW));
      }

      tu6_write_lrz_reg(cmd, cs, A6XX_GRAS_LRZ_CNTL(
         .enable = true,
         .fc_enable = cmd->state.lrz.fast_clear,
         .disable_on_wrong_dir = cmd->state.lrz.gpu_dir_tracking,
      ));

      /* Flushing with fc_enable flushes writes to LRZ fast-clear buffer */
      tu6_emit_event_write(cmd, cs, LRZ_FLUSH);
      tu6_write_lrz_reg(cmd, cs, A6XX_GRAS_LRZ_CNTL());
   }

   /* If gpu_dir_tracking is enabled and lrz is not valid blob, at this point,
    * additionally clears direction buffer:
    *  GRAS_LRZ_DEPTH_VIEW(.dword = 0)
    *  GRAS_LRZ_DEPTH_VIEW(.dword = 0xffffffff)
    *  A6XX_GRAS_LRZ_CNTL(.enable = true, .disable_on_wrong_dir = true)
    *  LRZ_CLEAR
    *  LRZ_FLUSH
    * Since it happens after all of the rendering is done there is no known
    * reason to do such clear.
    */

   if (!cmd->state.lrz.fast_clear && !cmd->state.lrz.gpu_dir_tracking) {
      tu6_write_lrz_reg(cmd, cs, A6XX_GRAS_LRZ_CNTL(0));
      tu6_emit_event_write(cmd, cs, LRZ_FLUSH);
   }
}

void
tu_lrz_sysmem_begin(struct tu_cmd_buffer *cmd, struct tu_cs *cs)
{
   if (!cmd->state.lrz.image_view)
      return;

   /* Actually, LRZ buffer could be filled in sysmem, in theory to
    * be used in another renderpass, but the benefit is rather dubious.
    */

   struct tu_lrz_state *lrz = &cmd->state.lrz;
   tu6_emit_lrz_buffer(cs, lrz->image_view->image);
   tu_disable_lrz(cmd, cs, lrz->image_view->image);
}

void
tu_lrz_sysmem_end(struct tu_cmd_buffer *cmd, struct tu_cs *cs)
{
   /* Nothing to do, direction was disabled in tu_lrz_sysmem_begin. */
}

/* Disable LRZ outside of renderpass. */
void
tu_disable_lrz(struct tu_cmd_buffer *cmd, struct tu_cs *cs,
               struct tu_image *image)
{
   if (!cmd->device->physical_device->info->a6xx.has_lrz_dir_tracking)
      return;

   if (!image->lrz_height)
      return;

   tu6_emit_lrz_buffer(cs, image);
   tu6_disable_lrz_via_depth_view(cmd, cs);
}

/* Clear LRZ, used for out of renderpass depth clears. */
void
tu_lrz_clear_depth_image(struct tu_cmd_buffer *cmd,
                         struct tu_image *image,
                         const VkClearDepthStencilValue *pDepthStencil,
                         uint32_t rangeCount,
                         const VkImageSubresourceRange *pRanges)
{
   if (!rangeCount || !image->lrz_height ||
       !cmd->device->physical_device->info->a6xx.has_lrz_dir_tracking)
      return;

   /* We cannot predict which depth subresource would be used later on,
    * so we just pick the first one with depth cleared and clear the LRZ.
    */
   const VkImageSubresourceRange *range = NULL;
   for (unsigned i = 0; i < rangeCount; i++) {
      if (pRanges[i].aspectMask &
            (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT)) {
         range = &pRanges[i];
         break;
      }
   }

   if (!range)
      return;

   const VkImageSubresourceRange *last_range = &pRanges[rangeCount - 1];
   uint64_t lrz_fc_iova = image->iova + image->lrz_fc_offset;
   tu_cs_emit_regs(&cmd->cs,
               A6XX_GRAS_LRZ_BUFFER_BASE(.qword = 0),
               A6XX_GRAS_LRZ_BUFFER_PITCH(.pitch = 0),
               A6XX_GRAS_LRZ_FAST_CLEAR_BUFFER_BASE(.qword = lrz_fc_iova));

   tu6_write_lrz_reg(cmd, &cmd->cs, A6XX_GRAS_LRZ_DEPTH_VIEW(
         .base_layer = last_range->baseArrayLayer,
         .layer_count = tu_get_layerCount(image, last_range),
         .base_mip_level = last_range->baseMipLevel,
   ));

   bool fast_clear = pDepthStencil->depth == 0.f ||
                     pDepthStencil->depth == 1.f;

   tu6_write_lrz_reg(cmd, &cmd->cs, A6XX_GRAS_LRZ_CNTL(
      .enable = true,
      .fc_enable = fast_clear,
      .disable_on_wrong_dir = true,
   ));

   tu6_emit_event_write(cmd, &cmd->cs, LRZ_CLEAR);
   tu6_emit_event_write(cmd, &cmd->cs, LRZ_FLUSH);

   if (!fast_clear) {
      tu6_clear_lrz(cmd, &cmd->cs, image, (const VkClearValue*) pDepthStencil);
   }
}

void
tu_lrz_disable_during_renderpass(struct tu_cmd_buffer *cmd)
{
   assert(cmd->state.pass);

   cmd->state.lrz.valid = false;
   cmd->state.dirty |= TU_CMD_DIRTY_LRZ;

   if (cmd->state.lrz.gpu_dir_tracking) {
      tu6_write_lrz_reg(cmd, &cmd->cs, A6XX_GRAS_LRZ_CNTL(
         .enable = true,
         .dir = LRZ_DIR_INVALID,
         .disable_on_wrong_dir = true,
      ));
   }
}

/* update lrz state based on stencil-test func:
 *
 * Conceptually the order of the pipeline is:
 *
 *
 *   FS -> Alpha-Test  ->  Stencil-Test  ->  Depth-Test
 *                              |                |
 *                       if wrmask != 0     if wrmask != 0
 *                              |                |
 *                              v                v
 *                        Stencil-Write      Depth-Write
 *
 * Because Stencil-Test can have side effects (Stencil-Write) prior
 * to depth test, in this case we potentially need to disable early
 * lrz-test. See:
 *
 * https://www.khronos.org/opengl/wiki/Per-Sample_Processing
 */
static bool
tu6_stencil_op_lrz_allowed(struct A6XX_GRAS_LRZ_CNTL *gras_lrz_cntl,
                           VkCompareOp func,
                           bool stencil_write)
{
   switch (func) {
   case VK_COMPARE_OP_ALWAYS:
      /* nothing to do for LRZ, but for stencil test when stencil-
       * write is enabled, we need to disable lrz-test, since
       * conceptually stencil test and write happens before depth-test.
       */
      if (stencil_write) {
         return false;
      }
      break;
   case VK_COMPARE_OP_NEVER:
      /* fragment never passes, disable lrz_write for this draw. */
      gras_lrz_cntl->lrz_write = false;
      break;
   default:
      /* whether the fragment passes or not depends on result
       * of stencil test, which we cannot know when doing binning
       * pass.
       */
      gras_lrz_cntl->lrz_write = false;
      /* similarly to the VK_COMPARE_OP_ALWAYS case, if there are side-
       * effects from stencil test we need to disable lrz-test.
       */
      if (stencil_write) {
         return false;
      }
      break;
   }

   return true;
}

static struct A6XX_GRAS_LRZ_CNTL
tu6_calculate_lrz_state(struct tu_cmd_buffer *cmd,
                        const uint32_t a)
{
   struct tu_pipeline *pipeline = cmd->state.pipeline;
   bool z_test_enable = cmd->state.rb_depth_cntl & A6XX_RB_DEPTH_CNTL_Z_TEST_ENABLE;
   bool z_write_enable = cmd->state.rb_depth_cntl & A6XX_RB_DEPTH_CNTL_Z_WRITE_ENABLE;
   bool z_read_enable = cmd->state.rb_depth_cntl & A6XX_RB_DEPTH_CNTL_Z_READ_ENABLE;
   bool z_bounds_enable = cmd->state.rb_depth_cntl & A6XX_RB_DEPTH_CNTL_Z_BOUNDS_ENABLE;
   VkCompareOp depth_compare_op = (cmd->state.rb_depth_cntl & A6XX_RB_DEPTH_CNTL_ZFUNC__MASK) >> A6XX_RB_DEPTH_CNTL_ZFUNC__SHIFT;

   struct A6XX_GRAS_LRZ_CNTL gras_lrz_cntl = { 0 };

   if (!cmd->state.lrz.valid) {
      return gras_lrz_cntl;
   }

   /* If depth test is disabled we shouldn't touch LRZ.
    * Same if there is no depth attachment.
    */
   if (a == VK_ATTACHMENT_UNUSED || !z_test_enable ||
       (cmd->device->instance->debug_flags & TU_DEBUG_NOLRZ))
      return gras_lrz_cntl;

   if (!cmd->state.lrz.gpu_dir_tracking && !cmd->state.attachments) {
      /* Without on-gpu LRZ direction tracking - there is nothing we
       * can do to enable LRZ in secondary command buffers.
       */
      return gras_lrz_cntl;
   }

   gras_lrz_cntl.enable = true;
   gras_lrz_cntl.lrz_write =
      z_write_enable &&
      !(pipeline->lrz.force_disable_mask & TU_LRZ_FORCE_DISABLE_WRITE);
   gras_lrz_cntl.z_test_enable = z_read_enable && z_write_enable;
   gras_lrz_cntl.z_bounds_enable = z_bounds_enable;
   gras_lrz_cntl.fc_enable = cmd->state.lrz.fast_clear;
   gras_lrz_cntl.dir_write = cmd->state.lrz.gpu_dir_tracking;
   gras_lrz_cntl.disable_on_wrong_dir = cmd->state.lrz.gpu_dir_tracking;

   /* LRZ is disabled until it is cleared, which means that one "wrong"
    * depth test or shader could disable LRZ until depth buffer is cleared.
    */
   bool disable_lrz = false;
   bool temporary_disable_lrz = false;

   /* What happens in FS could affect LRZ, e.g.: writes to gl_FragDepth
    * or early fragment tests.
    */
   if (pipeline->lrz.force_disable_mask & TU_LRZ_FORCE_DISABLE_LRZ) {
      perf_debug(cmd->device, "Invalidating LRZ due to FS");
      disable_lrz = true;
   }

   /* If Z is not written - it doesn't affect LRZ buffer state.
    * Which means two things:
    * - Don't lock direction until Z is written for the first time;
    * - If Z isn't written and direction IS locked it's possible to just
    *   temporary disable LRZ instead of fully bailing out, when direction
    *   is changed.
    */

   enum tu_lrz_direction lrz_direction = TU_LRZ_UNKNOWN;
   switch (depth_compare_op) {
   case VK_COMPARE_OP_ALWAYS:
   case VK_COMPARE_OP_NOT_EQUAL:
      /* OP_ALWAYS and OP_NOT_EQUAL could have depth value of any direction,
       * so if there is a depth write - LRZ must be disabled.
       */
      if (z_write_enable) {
         perf_debug(cmd->device, "Invalidating LRZ due to ALWAYS/NOT_EQUAL");
         disable_lrz = true;
         gras_lrz_cntl.dir = LRZ_DIR_INVALID;
      } else {
         perf_debug(cmd->device, "Skipping LRZ due to ALWAYS/NOT_EQUAL");
         temporary_disable_lrz = true;
      }
      break;
   case VK_COMPARE_OP_EQUAL:
   case VK_COMPARE_OP_NEVER:
      /* Blob disables LRZ for OP_EQUAL, and from our empirical
       * evidence it is a right thing to do.
       *
       * Both OP_EQUAL and OP_NEVER don't change LRZ buffer so
       * we could just temporary disable LRZ.
       */
      temporary_disable_lrz = true;
      break;
   case VK_COMPARE_OP_GREATER:
   case VK_COMPARE_OP_GREATER_OR_EQUAL:
      lrz_direction = TU_LRZ_GREATER;
      gras_lrz_cntl.greater = true;
      gras_lrz_cntl.dir = LRZ_DIR_GE;
      break;
   case VK_COMPARE_OP_LESS:
   case VK_COMPARE_OP_LESS_OR_EQUAL:
      lrz_direction = TU_LRZ_LESS;
      gras_lrz_cntl.greater = false;
      gras_lrz_cntl.dir = LRZ_DIR_LE;
      break;
   default:
      unreachable("bad VK_COMPARE_OP value or uninitialized");
      break;
   };

   /* If depthfunc direction is changed, bail out on using LRZ. The
    * LRZ buffer encodes a min/max depth value per block, but if
    * we switch from GT/GE <-> LT/LE, those values cannot be
    * interpreted properly.
    */
   if (cmd->state.lrz.prev_direction != TU_LRZ_UNKNOWN &&
       lrz_direction != TU_LRZ_UNKNOWN &&
       cmd->state.lrz.prev_direction != lrz_direction) {
      if (z_write_enable) {
         perf_debug(cmd->device, "Invalidating LRZ due to direction change");
         disable_lrz = true;
      } else {
         perf_debug(cmd->device, "Skipping LRZ due to direction change");
         temporary_disable_lrz = true;
      }
   }

   /* Consider the following sequence of depthfunc changes:
    *
    * - COMPARE_OP_GREATER -> COMPARE_OP_EQUAL -> COMPARE_OP_GREATER
    * LRZ is disabled during COMPARE_OP_EQUAL but could be enabled
    * during second VK_COMPARE_OP_GREATER.
    *
    * - COMPARE_OP_GREATER -> COMPARE_OP_EQUAL -> COMPARE_OP_LESS
    * Here, LRZ is disabled during COMPARE_OP_EQUAL and should become
    * invalid during COMPARE_OP_LESS.
    *
    * This shows that we should keep last KNOWN direction.
    */
   if (z_write_enable && lrz_direction != TU_LRZ_UNKNOWN)
      cmd->state.lrz.prev_direction = lrz_direction;

   /* Invalidate LRZ and disable write if stencil test is enabled */
   bool stencil_test_enable = cmd->state.rb_stencil_cntl & A6XX_RB_STENCIL_CONTROL_STENCIL_ENABLE;
   if (!disable_lrz && stencil_test_enable) {
      bool stencil_front_writemask =
         (pipeline->dynamic_state_mask & BIT(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK)) ?
         (cmd->state.dynamic_stencil_wrmask & 0xff) :
         (pipeline->stencil_wrmask & 0xff);

      bool stencil_back_writemask =
         (pipeline->dynamic_state_mask & BIT(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK)) ?
         ((cmd->state.dynamic_stencil_wrmask & 0xff00) >> 8) :
         (pipeline->stencil_wrmask & 0xff00) >> 8;

      VkCompareOp stencil_front_compare_op =
         (cmd->state.rb_stencil_cntl & A6XX_RB_STENCIL_CONTROL_FUNC__MASK) >> A6XX_RB_STENCIL_CONTROL_FUNC__SHIFT;

      VkCompareOp stencil_back_compare_op =
         (cmd->state.rb_stencil_cntl & A6XX_RB_STENCIL_CONTROL_FUNC_BF__MASK) >> A6XX_RB_STENCIL_CONTROL_FUNC_BF__SHIFT;

      bool lrz_allowed = true;
      lrz_allowed = lrz_allowed && tu6_stencil_op_lrz_allowed(
                                      &gras_lrz_cntl, stencil_front_compare_op,
                                      stencil_front_writemask);

      lrz_allowed = lrz_allowed && tu6_stencil_op_lrz_allowed(
                                      &gras_lrz_cntl, stencil_back_compare_op,
                                      stencil_back_writemask);

      /* Without depth write it's enough to make sure that depth test
       * is executed after stencil test, so temporary disabling LRZ is enough.
       */
      if (!lrz_allowed) {
         if (z_write_enable)
            disable_lrz = true;
         else
            temporary_disable_lrz = true;
      }

      if (disable_lrz)
         perf_debug(cmd->device, "Invalidating LRZ due to stencil write");
      if (temporary_disable_lrz)
         perf_debug(cmd->device, "Skipping LRZ due to stencil write");
   }

   if (disable_lrz && cmd->state.lrz.gpu_dir_tracking) {
      cmd->state.lrz.valid = false;

      /* Direction byte on GPU should be set to CUR_DIR_DISABLED,
       * for this it's not enough to emit empty GRAS_LRZ_CNTL.
       */
      gras_lrz_cntl.enable = true;
      gras_lrz_cntl.dir = LRZ_DIR_INVALID;

      return gras_lrz_cntl;
   }

   if (temporary_disable_lrz)
      gras_lrz_cntl.enable = false;

   cmd->state.lrz.enabled = cmd->state.lrz.valid && gras_lrz_cntl.enable;
   if (!cmd->state.lrz.enabled)
      memset(&gras_lrz_cntl, 0, sizeof(gras_lrz_cntl));

   return gras_lrz_cntl;
}

void
tu6_emit_lrz(struct tu_cmd_buffer *cmd, struct tu_cs *cs)
{
   const uint32_t a = cmd->state.subpass->depth_stencil_attachment.attachment;
   struct A6XX_GRAS_LRZ_CNTL gras_lrz_cntl = tu6_calculate_lrz_state(cmd, a);

   tu6_write_lrz_reg(cmd, cs, pack_A6XX_GRAS_LRZ_CNTL(gras_lrz_cntl));
   tu_cs_emit_regs(cs, A6XX_RB_LRZ_CNTL(.enable = gras_lrz_cntl.enable));
}
