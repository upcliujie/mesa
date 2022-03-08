/*
 * Copyright © 2021 Collabora Ltd.
 *
 * Derived from tu_cmd_buffer.c which is:
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 * Copyright © 2015 Intel Corporation
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

#include "genxml/gen_macros.h"

#include "panvk_cs.h"
#include "panvk_private.h"

#include "pan_blitter.h"
#include "pan_cs.h"
#include "pan_encoder.h"

#include "util/rounding.h"
#include "util/u_pack_color.h"
#include "vk_format.h"

void
panvk_per_arch(cmd_add_job_ptr)(struct panvk_cmd_buffer *cmdbuf,
                                void *job_ptr)
{
   struct panvk_batch *batch = cmdbuf->state.batch;

   if (cmdbuf->vk.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY) {
      /* We only store the job offsets to stay immune to CPU buffer
       * remapping.
       */
      job_ptr = (void *)((uintptr_t)job_ptr -
                         (uintptr_t)cmdbuf->desc_pool.cpu_bo.ptr.cpu);
   }

   util_dynarray_append(&batch->jobs, void *, job_ptr);
}

static void
panvk_cmd_prepare_fragment_job(struct panvk_cmd_buffer *cmdbuf)
{
   const struct pan_fb_info *fbinfo = &cmdbuf->state.fb.info;
   struct panvk_batch *batch = cmdbuf->state.batch;
   struct panfrost_ptr job_ptr =
      pan_pool_alloc_desc(&cmdbuf->desc_pool.base, FRAGMENT_JOB);

   GENX(pan_emit_fragment_job)(fbinfo, batch->fb.desc.gpu, job_ptr.cpu);
   
   batch->fragment_job = job_ptr.gpu;
   
   panvk_per_arch(cmd_add_job_ptr)(cmdbuf, job_ptr.cpu);
}

#if PAN_ARCH == 5
void
panvk_per_arch(cmd_get_polygon_list)(struct panvk_cmd_buffer *cmdbuf,
                                     unsigned width, unsigned height,
                                     bool has_draws)
{
   struct panfrost_device *pdev = &cmdbuf->device->physical_device->pdev;
   struct panvk_batch *batch = cmdbuf->state.batch;

   if (batch->tiler.ctx.midgard.polygon_list)
      return;

   unsigned size =
      panfrost_tiler_get_polygon_list_size(pdev, width, height, has_draws);
   size = util_next_power_of_two(size);

   /* Create the BO as invisible if we can. In the non-hierarchical tiler case,
    * we need to write the polygon list manually because there's not WRITE_VALUE
    * job in the chain. */
   bool init_polygon_list = !has_draws && pdev->model->quirks.no_hierarchical_tiling;
   batch->tiler.ctx.midgard.polygon_list =
      panfrost_bo_create(pdev, size,
                         init_polygon_list ? 0 : PAN_BO_INVISIBLE,
                         "Polygon list");


   if (init_polygon_list) {
      assert(batch->tiler.ctx.midgard.polygon_list->ptr.cpu);
      uint32_t *polygon_list_body =
         batch->tiler.ctx.midgard.polygon_list->ptr.cpu +
         MALI_MIDGARD_TILER_MINIMUM_HEADER_SIZE;
      polygon_list_body[0] = 0xa0000000;
   }

   batch->tiler.ctx.midgard.disable = !has_draws;
}
#endif

#if PAN_ARCH <= 5
static void
panvk_copy_fb_desc(struct panvk_cmd_buffer *cmdbuf, void *src)
{
   const struct pan_fb_info *fbinfo = &cmdbuf->state.fb.info;
   struct panvk_batch *batch = cmdbuf->state.batch;
   uint32_t size = pan_size(FRAMEBUFFER);

   if (fbinfo->zs.view.zs || fbinfo->zs.view.s)
      size += pan_size(ZS_CRC_EXTENSION);

   size += MAX2(fbinfo->rt_count, 1) * pan_size(RENDER_TARGET);

   memcpy(batch->fb.desc.cpu, src, size);
}
#endif

static void
panvk_cmd_fix_cpu_pointers(struct panvk_cmd_buffer *cmdbuf)
{
   struct pan_scoreboard *scoreboard = &cmdbuf->state.batch->scoreboard;
   void *desc_pool_cpu_base = cmdbuf->state.desc_pool_cpu_base;

   if (cmdbuf->vk.level != VK_COMMAND_BUFFER_LEVEL_SECONDARY ||
       desc_pool_cpu_base == cmdbuf->desc_pool.cpu_bo.ptr.cpu)
      return;

   intptr_t translation = (intptr_t)cmdbuf->desc_pool.cpu_bo.ptr.cpu -
                          (intptr_t)desc_pool_cpu_base;

   if (scoreboard->first_tiler) {
      scoreboard->first_tiler =
         (void *)((uintptr_t)scoreboard->first_tiler + translation);
   }

   if (scoreboard->prev_job) {
      scoreboard->prev_job =
         (void *)((uintptr_t)scoreboard->prev_job + translation);
   }
   if (cmdbuf->state.batch->tls.cpu) {
      cmdbuf->state.batch->tls.cpu =
      (void *)((uintptr_t) cmdbuf->state.batch->tls.cpu + translation);
   }
   if (cmdbuf->state.batch->fb.desc.cpu) {
      cmdbuf->state.batch->fb.desc.cpu =
      (void *)((uintptr_t) cmdbuf->state.batch->fb.desc.cpu + translation);
   }

   cmdbuf->state.desc_pool_cpu_base = cmdbuf->desc_pool.cpu_bo.ptr.cpu;
}

void
panvk_per_arch(cmd_close_batch)(struct panvk_cmd_buffer *cmdbuf)
{
   struct panvk_batch *batch = cmdbuf->state.batch;

   if (!batch)
      return;

   const struct pan_fb_info *fbinfo = &cmdbuf->state.fb.info;
#if PAN_ARCH <= 5
   uint32_t tmp_fbd[(pan_size(FRAMEBUFFER) +
                     pan_size(ZS_CRC_EXTENSION) +
                     (MAX_RTS * pan_size(RENDER_TARGET))) / 4];
#endif

   assert(batch);

   bool clear = fbinfo->zs.clear.z | fbinfo->zs.clear.s;
   for (unsigned i = 0; i < fbinfo->rt_count; i++)
      clear |= fbinfo->rts[i].clear;

   panvk_cmd_fix_cpu_pointers(cmdbuf);
   if (!clear && !batch->scoreboard.first_job) {
      if (util_dynarray_num_elements(&batch->event_ops, struct panvk_event_op) == 0) {
         /* Content-less batch, let's drop it */
         vk_free(&cmdbuf->pool->vk.alloc, batch);
      } else {
         /* Batch has no jobs but is needed for synchronization, let's add a
          * NULL job so the SUBMIT ioctl doesn't choke on it.
          */
         struct panfrost_ptr ptr = pan_pool_alloc_desc(&cmdbuf->desc_pool.base,
                                                       JOB_HEADER);
         panvk_per_arch(cmd_add_job)(cmdbuf, MALI_JOB_TYPE_NULL,
                                     false, false, 0, 0,
                                     &ptr, false);
         list_addtail(&batch->node, &cmdbuf->batches);
      }
      cmdbuf->state.batch = NULL;
      return;
   }

   struct panfrost_device *pdev = &cmdbuf->device->physical_device->pdev;

   list_addtail(&batch->node, &cmdbuf->batches);

   if (cmdbuf->usage_flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) {
      cmdbuf->state.batch = NULL;
      return;
   }

   if (batch->scoreboard.first_tiler) {
      /* Make sure the CPU-remapping (if any) happens before
       * pan_preload_fb(). 16k of descriptors should be more than enough
       * for those reload jobs.
       */
      if (cmdbuf->desc_pool.cpu_only)
         panvk_cpu_pool_reserve_mem(&cmdbuf->desc_pool, 16 * 1024, 4096);
      struct panfrost_ptr preload_jobs[2];
      unsigned num_preload_jobs =
         GENX(pan_preload_fb)(&cmdbuf->desc_pool.base, &batch->scoreboard,
                              &cmdbuf->state.fb.info,
                              PAN_ARCH >= 6 ? batch->tls.gpu : batch->fb.desc.gpu,
                              PAN_ARCH >= 6 ? batch->tiler.descs.gpu : 0,
                              preload_jobs);
      for (unsigned i = 0; i < num_preload_jobs; i++)
         panvk_per_arch(cmd_add_job_ptr)(cmdbuf, preload_jobs[i].cpu);
   }

   if (batch->tlsinfo.tls.size) {
      batch->tlsinfo.tls.ptr =
         pan_pool_alloc_aligned(&cmdbuf->tls_pool.base, batch->tlsinfo.tls.size, 4096).gpu;
   }

   if (batch->tlsinfo.wls.size) {
      assert(batch->wls_total_size);
      batch->tlsinfo.wls.ptr =
         pan_pool_alloc_aligned(&cmdbuf->tls_pool.base, batch->wls_total_size, 4096).gpu;
   }

   if ((PAN_ARCH >= 6 || !batch->fb.desc.cpu) && batch->tls.cpu)
      GENX(pan_emit_tls)(&batch->tlsinfo, batch->tls.cpu);

   if (batch->fb.desc.cpu) {
#if PAN_ARCH == 5
      panvk_per_arch(cmd_get_polygon_list)(cmdbuf,
                                           fbinfo->width,
                                           fbinfo->height,
                                           false);

      mali_ptr polygon_list =
         batch->tiler.ctx.midgard.polygon_list->ptr.gpu;
      struct panfrost_ptr writeval_job =
         panfrost_scoreboard_initialize_tiler(&cmdbuf->desc_pool.base,
                                              &batch->scoreboard,
                                              polygon_list);
      if (writeval_job.cpu)
         panvk_per_arch(cmd_add_job_ptr)(cmdbuf, writeval_job.cpu);
#endif

#if PAN_ARCH <= 5
      void *fbd = tmp_fbd;
#else
      void *fbd = batch->fb.desc.cpu;
#endif

      batch->fb.desc.gpu |=
         GENX(pan_emit_fbd)(pdev, &cmdbuf->state.fb.info, &batch->tlsinfo,
                            &batch->tiler.ctx, fbd);

#if PAN_ARCH <= 5
      panvk_copy_fb_desc(cmdbuf, tmp_fbd);
      memcpy(batch->tiler.templ,
             pan_section_ptr(fbd, FRAMEBUFFER, TILER),
             pan_size(TILER_CONTEXT));
#endif

      panvk_cmd_prepare_fragment_job(cmdbuf);
   }

   cmdbuf->state.batch = NULL;
}

void
panvk_per_arch(CmdNextSubpass2)(VkCommandBuffer commandBuffer,
                                const VkSubpassBeginInfo *pSubpassBeginInfo,
                                const VkSubpassEndInfo *pSubpassEndInfo)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   panvk_per_arch(cmd_close_batch)(cmdbuf);

   cmdbuf->state.subpass++;
   panvk_cmd_fb_info_set_subpass(cmdbuf);
   panvk_cmd_open_batch(cmdbuf);
}

void
panvk_per_arch(CmdNextSubpass)(VkCommandBuffer cmd, VkSubpassContents contents)
{
   VkSubpassBeginInfo binfo = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO,
      .contents = contents
   };
   VkSubpassEndInfo einfo = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO,
   };

   panvk_per_arch(CmdNextSubpass2)(cmd, &binfo, &einfo);
}

void
panvk_per_arch(cmd_alloc_fb_desc)(struct panvk_cmd_buffer *cmdbuf)
{
   struct panvk_batch *batch = cmdbuf->state.batch;

   if (batch->fb.desc.gpu)
      return;

   const struct pan_fb_info *fbinfo = &cmdbuf->state.fb.info;
   bool has_zs_ext = fbinfo->zs.view.zs || fbinfo->zs.view.s;
   unsigned tags = MALI_FBD_TAG_IS_MFBD;

   batch->fb.info = cmdbuf->state.framebuffer;
   batch->fb.desc =
      pan_pool_alloc_desc_aggregate(&cmdbuf->desc_pool.base,
                                    PAN_DESC(FRAMEBUFFER),
                                    PAN_DESC_ARRAY(has_zs_ext ? 1 : 0, ZS_CRC_EXTENSION),
                                    PAN_DESC_ARRAY(MAX2(fbinfo->rt_count, 1), RENDER_TARGET));

   /* Tag the pointer */
   batch->fb.desc.gpu |= tags;

#if PAN_ARCH >= 6
   memset(&cmdbuf->state.fb.info.bifrost.pre_post.dcds, 0,
          sizeof(cmdbuf->state.fb.info.bifrost.pre_post.dcds));
#endif
}

void
panvk_per_arch(cmd_alloc_tls_desc)(struct panvk_cmd_buffer *cmdbuf, bool gfx)
{
   struct panvk_batch *batch = cmdbuf->state.batch;

   assert(batch);
   if (batch->tls.gpu)
      return;

   if (PAN_ARCH == 5 && gfx) {
      panvk_per_arch(cmd_alloc_fb_desc)(cmdbuf);
      batch->tls = batch->fb.desc;
      batch->tls.gpu &= ~63ULL;
   } else {
      batch->tls =
         pan_pool_alloc_desc(&cmdbuf->desc_pool.base, LOCAL_STORAGE);
   }
}

static void
panvk_sysval_upload_ssbo_info(struct panvk_cmd_buffer *cmdbuf,
                              unsigned ssbo_id,
                              struct panvk_cmd_bind_point_state *bind_point_state,
                              union panvk_sysval_data *data)
{
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;
   const struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;

   for (unsigned s = 0; s < pipeline->layout->num_sets; s++) {
      unsigned ssbo_offset = pipeline->layout->sets[s].ssbo_offset;
      unsigned num_ssbos = pipeline->layout->sets[s].layout->num_ssbos;
      unsigned dyn_ssbo_offset = pipeline->layout->sets[s].dyn_ssbo_offset + pipeline->layout->num_ssbos;
      unsigned num_dyn_ssbos = pipeline->layout->sets[s].layout->num_dyn_ssbos;
      const struct panvk_buffer_desc *ssbo = NULL;

      if (ssbo_id >= ssbo_offset && ssbo_id < (ssbo_offset + num_ssbos))
         ssbo = &desc_state->sets[s]->ssbos[ssbo_id - ssbo_offset];
      else if (ssbo_id >= dyn_ssbo_offset && ssbo_id < (dyn_ssbo_offset + num_dyn_ssbos))
         ssbo = &desc_state->dyn.ssbos[ssbo_id - pipeline->layout->num_ssbos];

      if (ssbo) {
         data->u64[0] = ssbo->buffer->bo->ptr.gpu +
                        ssbo->buffer->bo_offset +
                        ssbo->offset;
         data->u32[2] = ssbo->size == VK_WHOLE_SIZE ?
                        ssbo->buffer->size - ssbo->offset :
                        ssbo->size;
      }
   }
}

static void
panvk_cmd_upload_sysval(struct panvk_cmd_buffer *cmdbuf,
                        unsigned id,
                        struct panvk_cmd_bind_point_state *bind_point_state,
                        union panvk_sysval_data *data)
{
   switch (PAN_SYSVAL_TYPE(id)) {
   case PAN_SYSVAL_VIEWPORT_SCALE:
      panvk_sysval_upload_viewport_scale(&cmdbuf->state.viewport, data);
      break;
   case PAN_SYSVAL_VIEWPORT_OFFSET:
      panvk_sysval_upload_viewport_offset(&cmdbuf->state.viewport, data);
      break;
   case PAN_SYSVAL_VERTEX_INSTANCE_OFFSETS:
      data->u32[0] = cmdbuf->state.ib.first_vertex;
      data->u32[1] = cmdbuf->state.ib.base_vertex;
      data->u32[2] = cmdbuf->state.ib.base_instance;
      break;
   case PAN_SYSVAL_BLEND_CONSTANTS:
      memcpy(data->f32, cmdbuf->state.blend.constants, sizeof(data->f32));
      break;
   case PAN_SYSVAL_SSBO:
      /* This won't work with dynamic SSBO indexing. We might want to
       * consider storing SSBO mappings in a separate UBO if we need to
       * support
       * VkPhysicalDeviceVulkan12Features.shaderStorageBufferArrayNonUniformIndexing.
       */
      panvk_sysval_upload_ssbo_info(cmdbuf, PAN_SYSVAL_ID(id), bind_point_state, data);
      break;
   case PAN_SYSVAL_NUM_WORK_GROUPS:
      data->u32[0] = cmdbuf->state.compute.wg_count.x;
      data->u32[1] = cmdbuf->state.compute.wg_count.y;
      data->u32[2] = cmdbuf->state.compute.wg_count.z;
      break;
   case PAN_SYSVAL_LOCAL_GROUP_SIZE:
      data->u32[0] = bind_point_state->pipeline->cs.local_size.x;
      data->u32[1] = bind_point_state->pipeline->cs.local_size.y;
      data->u32[2] = bind_point_state->pipeline->cs.local_size.z;
      break;
   default:
      unreachable("Invalid static sysval");
   }
}

static void
panvk_cmd_prepare_sysvals(struct panvk_cmd_buffer *cmdbuf,
                          struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;

   if (!pipeline->num_sysvals)
      return;

   uint32_t dirty = cmdbuf->state.dirty | desc_state->dirty;

   for (unsigned i = 0; i < ARRAY_SIZE(desc_state->sysvals); i++) {
      unsigned sysval_count = pipeline->sysvals[i].ids.sysval_count;
      if (!sysval_count || pipeline->sysvals[i].ubo ||
          (desc_state->sysvals[i] && !(dirty & pipeline->sysvals[i].dirty_mask)))
         continue;

      struct panfrost_ptr sysvals =
         pan_pool_alloc_aligned(&cmdbuf->desc_pool.base, sysval_count * 16, 16);
      union panvk_sysval_data *data = sysvals.cpu;

      for (unsigned s = 0; s < pipeline->sysvals[i].ids.sysval_count; s++) {
         panvk_cmd_upload_sysval(cmdbuf, pipeline->sysvals[i].ids.sysvals[s],
                                 bind_point_state, &data[s]);
      }

      desc_state->sysvals[i] = sysvals.gpu;
   }
}

static void
panvk_cmd_prepare_push_constants(struct panvk_cmd_buffer *cmdbuf,
                                 struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;

   if (!pipeline->layout->push_constants.size || desc_state->push_constants)
      return;

   struct panfrost_ptr push_constants =
      pan_pool_alloc_aligned(&cmdbuf->desc_pool.base,
                             ALIGN_POT(pipeline->layout->push_constants.size, 16),
                             16);

   memcpy(push_constants.cpu, cmdbuf->push_constants,
          pipeline->layout->push_constants.size);
   desc_state->push_constants = push_constants.gpu;
}

static void
panvk_cmd_prepare_ubos(struct panvk_cmd_buffer *cmdbuf,
                       struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;

   if (!pipeline->num_ubos || desc_state->ubos)
      return;

   panvk_cmd_prepare_sysvals(cmdbuf, bind_point_state);
   panvk_cmd_prepare_push_constants(cmdbuf, bind_point_state);

   uint32_t num_ubos =
      pipeline->num_ubos +
      (cmdbuf->vk.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY ? 1 : 0);
   struct panfrost_ptr ubos =
      pan_pool_alloc_desc_array(&cmdbuf->desc_pool.base,
                                num_ubos,
                                UNIFORM_BUFFER);

   panvk_per_arch(emit_ubos)(pipeline, desc_state, ubos.cpu);

   if (cmdbuf->vk.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY) {
      memset(ubos.cpu + (pan_size(UNIFORM_BUFFER) * pipeline->num_ubos),
             0, pan_size(UNIFORM_BUFFER));
   }

   desc_state->ubos = ubos.gpu;
}

static void
panvk_cmd_prepare_textures(struct panvk_cmd_buffer *cmdbuf,
                           struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;
   unsigned num_textures = pipeline->layout->num_textures;

   if (!num_textures || desc_state->textures)
      return;

   unsigned tex_entry_size = PAN_ARCH >= 6 ?
                             pan_size(TEXTURE) :
                             sizeof(mali_ptr);
   struct panfrost_ptr textures =
      pan_pool_alloc_aligned(&cmdbuf->desc_pool.base,
                             num_textures * tex_entry_size,
                             tex_entry_size);

   void *texture = textures.cpu;

   for (unsigned i = 0; i < ARRAY_SIZE(desc_state->sets); i++) {
      if (!desc_state->sets[i]) continue;

      memcpy(texture,
             desc_state->sets[i]->textures,
             desc_state->sets[i]->layout->num_textures *
             tex_entry_size);

      texture += desc_state->sets[i]->layout->num_textures *
                 tex_entry_size;
   }

   desc_state->textures = textures.gpu;
}

static void
panvk_cmd_prepare_samplers(struct panvk_cmd_buffer *cmdbuf,
                           struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;
   unsigned num_samplers = pipeline->layout->num_samplers;

   if (!num_samplers || desc_state->samplers)
      return;

   struct panfrost_ptr samplers =
      pan_pool_alloc_desc_array(&cmdbuf->desc_pool.base,
                                num_samplers,
                                SAMPLER);

   void *sampler = samplers.cpu;

   /* Prepare the dummy sampler */
   pan_pack(sampler, SAMPLER, cfg) {
#if PAN_ARCH >= 6
      cfg.seamless_cube_map = false;
#endif
      cfg.magnify_nearest = true;
      cfg.minify_nearest = true;
      cfg.normalized_coordinates = false;
   }

   sampler += pan_size(SAMPLER);

   for (unsigned i = 0; i < ARRAY_SIZE(desc_state->sets); i++) {
      if (!desc_state->sets[i]) continue;

      memcpy(sampler,
             desc_state->sets[i]->samplers,
             desc_state->sets[i]->layout->num_samplers *
             pan_size(SAMPLER));

      sampler += desc_state->sets[i]->layout->num_samplers *
                 pan_size(SAMPLER);
   }

   desc_state->samplers = samplers.gpu;
}

static void
panvk_draw_prepare_fs_rsd(struct panvk_cmd_buffer *cmdbuf,
                          struct panvk_draw_info *draw)
{
   const struct panvk_pipeline *pipeline =
      panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);

   if (!pipeline->fs.dynamic_rsd) {
      draw->fs_rsd = pipeline->rsds[MESA_SHADER_FRAGMENT];
      return;
   }

   if (!cmdbuf->state.fs_rsd) {
      struct panfrost_ptr rsd =
         pan_pool_alloc_desc_aggregate(&cmdbuf->desc_pool.base,
                                       PAN_DESC(RENDERER_STATE),
                                       PAN_DESC_ARRAY(pipeline->blend.state.rt_count,
                                                      BLEND));

      struct mali_renderer_state_packed rsd_dyn;
      struct mali_renderer_state_packed *rsd_templ =
         (struct mali_renderer_state_packed *)&pipeline->fs.rsd_template;

      STATIC_ASSERT(sizeof(pipeline->fs.rsd_template) >= sizeof(*rsd_templ));

      panvk_per_arch(emit_dyn_fs_rsd)(pipeline, &cmdbuf->state, &rsd_dyn);
      pan_merge(rsd_dyn, (*rsd_templ), RENDERER_STATE);
      memcpy(rsd.cpu, &rsd_dyn, sizeof(rsd_dyn));

      void *bd = rsd.cpu + pan_size(RENDERER_STATE);
      for (unsigned i = 0; i < pipeline->blend.state.rt_count; i++) {
         if (pipeline->blend.constant[i].index != (uint8_t)~0) {
            struct mali_blend_packed bd_dyn;
            struct mali_blend_packed *bd_templ =
               (struct mali_blend_packed *)&pipeline->blend.bd_template[i];

            STATIC_ASSERT(sizeof(pipeline->blend.bd_template[0]) >= sizeof(*bd_templ));
            panvk_per_arch(emit_blend_constant)(cmdbuf->device, pipeline, i,
                                                cmdbuf->state.blend.constants,
                                                &bd_dyn);
            pan_merge(bd_dyn, (*bd_templ), BLEND);
            memcpy(bd, &bd_dyn, sizeof(bd_dyn));
         }
         bd += pan_size(BLEND);
      }

      cmdbuf->state.fs_rsd = rsd.gpu;
   }

   draw->fs_rsd = cmdbuf->state.fs_rsd;
}

#if PAN_ARCH >= 6
void
panvk_per_arch(cmd_get_tiler_context)(struct panvk_cmd_buffer *cmdbuf,
                                      unsigned width, unsigned height)
{
   struct panvk_batch *batch = cmdbuf->state.batch;

   if (batch->tiler.descs.cpu ||
       (cmdbuf->vk.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY &&
        (cmdbuf->usage_flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT)))
      return;

   batch->tiler.descs =
      pan_pool_alloc_desc_aggregate(&cmdbuf->desc_pool.base,
                                    PAN_DESC(TILER_CONTEXT),
                                    PAN_DESC(TILER_HEAP));
   STATIC_ASSERT(sizeof(batch->tiler.templ) >=
                 pan_size(TILER_CONTEXT) + pan_size(TILER_HEAP));

   struct panfrost_ptr desc = {
      .gpu = batch->tiler.descs.gpu,
      .cpu = batch->tiler.templ,
   };

   panvk_per_arch(emit_tiler_context)(cmdbuf->device, width, height, &desc);
   memcpy(batch->tiler.descs.cpu, batch->tiler.templ,
          pan_size(TILER_CONTEXT) + pan_size(TILER_HEAP));
   batch->tiler.ctx.bifrost = batch->tiler.descs.gpu;
}
#endif

void
panvk_per_arch(cmd_prepare_tiler_context)(struct panvk_cmd_buffer *cmdbuf)
{
   const struct pan_fb_info *fbinfo = &cmdbuf->state.fb.info;

#if PAN_ARCH == 5
   panvk_per_arch(cmd_get_polygon_list)(cmdbuf,
                                        fbinfo->width,
                                        fbinfo->height,
                                        true);
#else
   panvk_per_arch(cmd_get_tiler_context)(cmdbuf,
                                         fbinfo->width,
                                         fbinfo->height);
#endif
}

static void
panvk_draw_prepare_tiler_context(struct panvk_cmd_buffer *cmdbuf,
                                 struct panvk_draw_info *draw)
{
   struct panvk_batch *batch = cmdbuf->state.batch;

   panvk_per_arch(cmd_prepare_tiler_context)(cmdbuf);
   draw->tiler_ctx = &batch->tiler.ctx;
}

static void
panvk_draw_prepare_varyings(struct panvk_cmd_buffer *cmdbuf,
                            struct panvk_draw_info *draw)
{
   const struct panvk_pipeline *pipeline = panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);
   struct panvk_varyings_info *varyings = &cmdbuf->state.varyings;

   panvk_varyings_alloc(varyings, &cmdbuf->varying_pool.base,
                        draw->padded_vertex_count * draw->instance_count);

   unsigned buf_count =
      panvk_varyings_buf_count(varyings) +
      (cmdbuf->vk.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY ? 1 : 0);
   struct panfrost_ptr bufs =
      pan_pool_alloc_desc_array(&cmdbuf->desc_pool.base,
                                buf_count + (PAN_ARCH >= 6 ? 1 : 0),
                                ATTRIBUTE_BUFFER);

   if (cmdbuf->vk.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY) {
      memset(bufs.cpu + (pan_size(ATTRIBUTE_BUFFER) * (buf_count - 1)),
             0, pan_size(ATTRIBUTE_BUFFER));
   }

   panvk_per_arch(emit_varying_bufs)(varyings, bufs.cpu);

   /* We need an empty entry to stop prefetching on Bifrost */
#if PAN_ARCH >= 6
   memset(bufs.cpu + (pan_size(ATTRIBUTE_BUFFER) * buf_count), 0,
          pan_size(ATTRIBUTE_BUFFER));
#endif

   if (BITSET_TEST(varyings->active, VARYING_SLOT_POS)) {
      draw->position = varyings->buf[varyings->varying[VARYING_SLOT_POS].buf].address +
                       varyings->varying[VARYING_SLOT_POS].offset;
   }

   if (BITSET_TEST(varyings->active, VARYING_SLOT_PSIZ)) {
      draw->psiz = varyings->buf[varyings->varying[VARYING_SLOT_PSIZ].buf].address +
                       varyings->varying[VARYING_SLOT_POS].offset;
   } else if (pipeline->ia.topology == MALI_DRAW_MODE_LINES ||
              pipeline->ia.topology == MALI_DRAW_MODE_LINE_STRIP ||
              pipeline->ia.topology == MALI_DRAW_MODE_LINE_LOOP) {
      draw->line_width = pipeline->dynamic_state_mask & PANVK_DYNAMIC_LINE_WIDTH ?
                         cmdbuf->state.rast.line_width : pipeline->rast.line_width;
   } else {
      draw->line_width = 1.0f;
   }
   draw->varying_bufs = bufs.gpu;

   for (unsigned s = 0; s < MESA_SHADER_STAGES; s++) {
      if (!varyings->stage[s].count) continue;

      struct panfrost_ptr attribs =
         pan_pool_alloc_desc_array(&cmdbuf->desc_pool.base,
                                   varyings->stage[s].count,
                                   ATTRIBUTE);

      panvk_per_arch(emit_varyings)(cmdbuf->device, varyings, s, attribs.cpu);
      draw->stages[s].varyings = attribs.gpu;
   }
}

static void
panvk_fill_non_vs_attribs(struct panvk_cmd_buffer *cmdbuf,
                          struct panvk_cmd_bind_point_state *bind_point_state,
                          void *attrib_bufs, void *attribs,
                          unsigned first_buf)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;

   for (unsigned s = 0; s < pipeline->layout->num_sets; s++) {
      const struct panvk_descriptor_set *set = desc_state->sets[s];

      if (!set) continue;

      const struct panvk_descriptor_set_layout *layout = set->layout;
      unsigned img_idx = pipeline->layout->sets[s].img_offset;
      unsigned offset = img_idx * pan_size(ATTRIBUTE_BUFFER) * 2;
      unsigned size = layout->num_imgs * pan_size(ATTRIBUTE_BUFFER) * 2;

      memcpy(attrib_bufs + offset, desc_state->sets[s]->img_attrib_bufs, size);

      offset = img_idx * pan_size(ATTRIBUTE);
      for (unsigned i = 0; i < layout->num_imgs; i++) {
         pan_pack(attribs + offset, ATTRIBUTE, cfg) {
            cfg.buffer_index = first_buf + (img_idx + i) * 2;
            cfg.format = desc_state->sets[s]->img_fmts[i];
            cfg.offset_enable = PAN_ARCH <= 5;
         }
         offset += pan_size(ATTRIBUTE);
      }
   }
}

static void
panvk_prepare_non_vs_attribs(struct panvk_cmd_buffer *cmdbuf,
                             struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;

   if (desc_state->non_vs_attribs || !pipeline->img_access_mask)
      return;

   unsigned attrib_count = pipeline->layout->num_imgs;
   unsigned attrib_buf_count = (pipeline->layout->num_imgs * 2);
   struct panfrost_ptr bufs =
      pan_pool_alloc_desc_array(&cmdbuf->desc_pool.base,
                                attrib_buf_count + (PAN_ARCH >= 6 ? 1 : 0),
                                ATTRIBUTE_BUFFER);
   struct panfrost_ptr attribs =
      pan_pool_alloc_desc_array(&cmdbuf->desc_pool.base, attrib_count,
                                ATTRIBUTE);

   panvk_fill_non_vs_attribs(cmdbuf, bind_point_state, bufs.cpu, attribs.cpu, 0);

   desc_state->non_vs_attrib_bufs = bufs.gpu;
   desc_state->non_vs_attribs = attribs.gpu;
}

static void
panvk_draw_prepare_vs_attribs(struct panvk_cmd_buffer *cmdbuf,
                              struct panvk_draw_info *draw)
{
   struct panvk_cmd_bind_point_state *bind_point_state =
      panvk_cmd_get_bind_point_state(cmdbuf, GRAPHICS);
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;
   unsigned num_imgs =
      pipeline->img_access_mask & BITFIELD_BIT(MESA_SHADER_VERTEX) ?
      pipeline->layout->num_imgs : 0;
   unsigned attrib_count = pipeline->attribs.attrib_count + num_imgs;

   if (desc_state->vs_attribs || !attrib_count)
      return;

   if (!pipeline->attribs.buf_count) {
      panvk_prepare_non_vs_attribs(cmdbuf, bind_point_state);
      desc_state->vs_attrib_bufs = desc_state->non_vs_attrib_bufs;
      desc_state->vs_attribs = desc_state->non_vs_attribs;
      return;
   }

   unsigned attrib_buf_count = pipeline->attribs.buf_count * 2;
   struct panfrost_ptr bufs =
      pan_pool_alloc_desc_array(&cmdbuf->desc_pool.base,
                                attrib_buf_count + (PAN_ARCH >= 6 ? 1 : 0),
                                ATTRIBUTE_BUFFER);
   struct panfrost_ptr attribs =
      pan_pool_alloc_desc_array(&cmdbuf->desc_pool.base, attrib_count,
                                ATTRIBUTE);

   panvk_per_arch(emit_attrib_bufs)(&pipeline->attribs,
                                    cmdbuf->state.vb.bufs,
                                    cmdbuf->state.vb.count,
                                    draw, bufs.cpu);
   panvk_per_arch(emit_attribs)(cmdbuf->device, draw, &pipeline->attribs,
                                cmdbuf->state.vb.bufs, cmdbuf->state.vb.count,
                                attribs.cpu);

   if (attrib_count > pipeline->attribs.buf_count) {
      unsigned bufs_offset = pipeline->attribs.buf_count * pan_size(ATTRIBUTE_BUFFER) * 2;
      unsigned attribs_offset = pipeline->attribs.buf_count * pan_size(ATTRIBUTE);

      panvk_fill_non_vs_attribs(cmdbuf, bind_point_state,
                                bufs.cpu + bufs_offset, attribs.cpu + attribs_offset,
                                pipeline->attribs.buf_count * 2);
   }

   /* A NULL entry is needed to stop prefecting on Bifrost */
#if PAN_ARCH >= 6
   memset(bufs.cpu + (pan_size(ATTRIBUTE_BUFFER) * attrib_buf_count), 0,
          pan_size(ATTRIBUTE_BUFFER));
#endif

   desc_state->vs_attrib_bufs = bufs.gpu;
   desc_state->vs_attribs = attribs.gpu;
}

static void
panvk_draw_prepare_attributes(struct panvk_cmd_buffer *cmdbuf,
                              struct panvk_draw_info *draw)
{
   struct panvk_cmd_bind_point_state *bind_point_state =
      panvk_cmd_get_bind_point_state(cmdbuf, GRAPHICS);
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;

   for (unsigned i = 0; i < ARRAY_SIZE(draw->stages); i++) {
      if (i == MESA_SHADER_VERTEX) {
         panvk_draw_prepare_vs_attribs(cmdbuf, draw);
         draw->stages[i].attributes = desc_state->vs_attribs;
         draw->stages[i].attribute_bufs = desc_state->vs_attrib_bufs;
      } else if (pipeline->img_access_mask & BITFIELD_BIT(i)) {
         panvk_prepare_non_vs_attribs(cmdbuf, bind_point_state);
         draw->stages[i].attributes = desc_state->non_vs_attribs;
         draw->stages[i].attribute_bufs = desc_state->non_vs_attrib_bufs;
      }
   }
}

static void
panvk_draw_prepare_viewport(struct panvk_cmd_buffer *cmdbuf,
                            struct panvk_draw_info *draw)
{
   const struct panvk_pipeline *pipeline = panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);

   if (pipeline->vpd) {
      draw->viewport = pipeline->vpd;
   } else if (cmdbuf->state.vpd) {
      draw->viewport = cmdbuf->state.vpd;
   } else {
      struct panfrost_ptr vp =
         pan_pool_alloc_desc(&cmdbuf->desc_pool.base, VIEWPORT);

      const VkViewport *viewport =
         pipeline->dynamic_state_mask & PANVK_DYNAMIC_VIEWPORT ?
         &cmdbuf->state.viewport : &pipeline->viewport;
      const VkRect2D *scissor =
         pipeline->dynamic_state_mask & PANVK_DYNAMIC_SCISSOR ?
         &cmdbuf->state.scissor : &pipeline->scissor;

      panvk_per_arch(emit_viewport)(viewport, scissor, vp.cpu);
      draw->viewport = cmdbuf->state.vpd = vp.gpu;
   }
}

static void
panvk_draw_prepare_vertex_job(struct panvk_cmd_buffer *cmdbuf,
                              struct panvk_draw_info *draw)
{
   const struct panvk_pipeline *pipeline = panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);
   struct panfrost_ptr ptr =
      pan_pool_alloc_desc(&cmdbuf->desc_pool.base, COMPUTE_JOB);

   panvk_per_arch(emit_vertex_job)(pipeline, draw, ptr.cpu);

   draw->vertex_job_id =
      panvk_per_arch(cmd_add_job)(cmdbuf, MALI_JOB_TYPE_VERTEX,
                                  false, false, 0, 0,
                                  &ptr, false);
}

static void
panvk_draw_prepare_tiler_job(struct panvk_cmd_buffer *cmdbuf,
                             struct panvk_draw_info *draw)
{
   const struct panvk_pipeline *pipeline = panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);
   struct panfrost_ptr ptr =
      pan_pool_alloc_desc(&cmdbuf->desc_pool.base, TILER_JOB);

   panvk_per_arch(emit_tiler_job)(pipeline, draw, ptr.cpu);
   panvk_per_arch(cmd_add_job)(cmdbuf, MALI_JOB_TYPE_TILER,
                               false, false, draw->vertex_job_id, 0,
                               &ptr, false);
}

static void
panvk_cmd_draw(struct panvk_cmd_buffer *cmdbuf,
               struct panvk_draw_info *draw)
{
   struct panvk_batch *batch = cmdbuf->state.batch;
   struct panvk_cmd_bind_point_state *bind_point_state =
      panvk_cmd_get_bind_point_state(cmdbuf, GRAPHICS);
   const struct panvk_pipeline *pipeline =
      panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);

   /* There are only 16 bits in the descriptor for the job ID, make sure all
    * the 3 (2 in Bifrost) jobs in this draw are in the same batch.
    */
   if (batch->scoreboard.job_index >= (UINT16_MAX - 3)) {
      panvk_per_arch(cmd_close_batch)(cmdbuf);
      panvk_cmd_preload_fb_after_batch_split(cmdbuf);
      batch = panvk_cmd_open_batch(cmdbuf);
   }

   if (pipeline->fs.required)
      panvk_per_arch(cmd_alloc_fb_desc)(cmdbuf);

   panvk_per_arch(cmd_alloc_tls_desc)(cmdbuf, true);

   unsigned base_vertex = draw->index_size ? draw->vertex_offset : 0;
   if (cmdbuf->state.ib.first_vertex != draw->offset_start ||
       cmdbuf->state.ib.base_vertex != base_vertex ||
       cmdbuf->state.ib.base_vertex != draw->first_instance) {
      cmdbuf->state.ib.base_vertex = base_vertex;
      cmdbuf->state.ib.base_instance = draw->first_instance;
      cmdbuf->state.ib.first_vertex = draw->offset_start;
      cmdbuf->state.dirty |= PANVK_DYNAMIC_VERTEX_INSTANCE_OFFSETS;
   }

   panvk_cmd_prepare_ubos(cmdbuf, bind_point_state);
   panvk_cmd_prepare_textures(cmdbuf, bind_point_state);
   panvk_cmd_prepare_samplers(cmdbuf, bind_point_state);

   /* TODO: indexed draws */
   struct panvk_descriptor_state *desc_state =
      panvk_cmd_get_desc_state(cmdbuf, GRAPHICS);

   draw->tls = batch->tls.gpu;
   draw->fb = batch->fb.desc.gpu;
   draw->ubos = desc_state->ubos;
   draw->textures = desc_state->textures;
   draw->samplers = desc_state->samplers;

   STATIC_ASSERT(sizeof(draw->invocation) >= sizeof(struct mali_invocation_packed));
   panfrost_pack_work_groups_compute((struct mali_invocation_packed *)&draw->invocation,
                                      1, draw->vertex_range, draw->instance_count,
                                      1, 1, 1, true, false);

   panvk_draw_prepare_fs_rsd(cmdbuf, draw);
   panvk_draw_prepare_varyings(cmdbuf, draw);
   panvk_draw_prepare_attributes(cmdbuf, draw);
   panvk_draw_prepare_viewport(cmdbuf, draw);
   panvk_draw_prepare_tiler_context(cmdbuf, draw);
   panvk_draw_prepare_vertex_job(cmdbuf, draw);
   panvk_draw_prepare_tiler_job(cmdbuf, draw);
   batch->tlsinfo.tls.size = MAX2(pipeline->tls_size, batch->tlsinfo.tls.size);
   assert(!pipeline->wls_size);

   /* Clear the dirty flags all at once */
   desc_state->dirty = cmdbuf->state.dirty = 0;
}

void
panvk_per_arch(CmdDraw)(VkCommandBuffer commandBuffer,
                        uint32_t vertexCount,
                        uint32_t instanceCount,
                        uint32_t firstVertex,
                        uint32_t firstInstance)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   if (instanceCount == 0 || vertexCount == 0)
      return;

   struct panvk_draw_info draw = {
      .first_vertex = firstVertex,
      .vertex_count = vertexCount,
      .vertex_range = vertexCount,
      .first_instance = firstInstance,
      .instance_count = instanceCount,
      .padded_vertex_count = instanceCount > 1 ?
                             panfrost_padded_vertex_count(vertexCount) :
                             vertexCount,
      .offset_start = firstVertex,
   };

   panvk_cmd_draw(cmdbuf, &draw);
}

static void
panvk_index_minmax_search(struct panvk_cmd_buffer *cmdbuf,
                          uint32_t start, uint32_t count,
                          uint32_t *min, uint32_t *max)
{
   void *ptr = cmdbuf->state.ib.buffer->bo->ptr.cpu +
               cmdbuf->state.ib.buffer->bo_offset +
               cmdbuf->state.ib.offset;

   fprintf(stderr, "WARNING: Crawling index buffers from the CPU isn't valid in Vulkan\n");

   assert(cmdbuf->state.ib.buffer);
   assert(cmdbuf->state.ib.buffer->bo);
   assert(cmdbuf->state.ib.buffer->bo->ptr.cpu);

   *max = 0;

   /* TODO: Use panfrost_minmax_cache */
   /* TODO: Read full cacheline of data to mitigate the uncached
    * mapping slowness.
    */
   switch (cmdbuf->state.ib.index_size) {
#define MINMAX_SEARCH_CASE(sz) \
   case sz: { \
      uint ## sz ## _t *indices = ptr; \
      *min = UINT ## sz ## _MAX; \
      for (uint32_t i = 0; i < count; i++) { \
         *min = MIN2(indices[i + start], *min); \
         *max = MAX2(indices[i + start], *max); \
      } \
      break; \
   }
   MINMAX_SEARCH_CASE(32)
   MINMAX_SEARCH_CASE(16)
   MINMAX_SEARCH_CASE(8)
#undef MINMAX_SEARCH_CASE
   default:
      unreachable("Invalid index size");
   }
}

void
panvk_per_arch(CmdDrawIndexed)(VkCommandBuffer commandBuffer,
                               uint32_t indexCount,
                               uint32_t instanceCount,
                               uint32_t firstIndex,
                               int32_t vertexOffset,
                               uint32_t firstInstance)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   uint32_t min_vertex, max_vertex;

   if (instanceCount == 0 || indexCount == 0)
      return;

   panvk_index_minmax_search(cmdbuf, firstIndex, indexCount,
                             &min_vertex, &max_vertex);

   unsigned vertex_range = max_vertex - min_vertex + 1;
   struct panvk_draw_info draw = {
      .index_size = cmdbuf->state.ib.index_size,
      .first_index = firstIndex,
      .index_count = indexCount,
      .vertex_offset = vertexOffset,
      .first_instance = firstInstance,
      .instance_count = instanceCount,
      .vertex_range = vertex_range,
      .vertex_count = indexCount + abs(vertexOffset),
      .padded_vertex_count = instanceCount > 1 ?
                             panfrost_padded_vertex_count(vertex_range) :
                             vertex_range,
      .offset_start = min_vertex + vertexOffset,
      .indices = cmdbuf->state.ib.buffer->bo->ptr.gpu +
                 cmdbuf->state.ib.buffer->bo_offset +
                 cmdbuf->state.ib.offset +
                 (firstIndex * (cmdbuf->state.ib.index_size / 8)),
   };

   panvk_cmd_draw(cmdbuf, &draw);
}

VkResult
panvk_per_arch(EndCommandBuffer)(VkCommandBuffer commandBuffer)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VkResult ret =
      cmdbuf->vk.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY ?
      cmdbuf->vk.cmd_queue.error : cmdbuf->record_result;

   panvk_per_arch(cmd_close_batch)(cmdbuf);
   cmdbuf->status = ret == VK_SUCCESS ?
                    PANVK_CMD_BUFFER_STATUS_EXECUTABLE :
                    PANVK_CMD_BUFFER_STATUS_INVALID;
   return ret;
}

void
panvk_per_arch(CmdEndRenderPass2)(VkCommandBuffer commandBuffer,
                                  const VkSubpassEndInfoKHR *pSubpassEndInfo)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   panvk_per_arch(cmd_close_batch)(cmdbuf);
   vk_free(&cmdbuf->pool->vk.alloc, cmdbuf->state.clear);
   cmdbuf->state.batch = NULL;
   cmdbuf->state.pass = NULL;
   cmdbuf->state.subpass = NULL;
   cmdbuf->state.framebuffer = NULL;
   cmdbuf->state.clear = NULL;
}

void
panvk_per_arch(CmdEndRenderPass)(VkCommandBuffer cmd)
{
   VkSubpassEndInfoKHR einfo = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO,
   };

   panvk_per_arch(CmdEndRenderPass2)(cmd, &einfo);
}


void
panvk_per_arch(CmdPipelineBarrier2)(VkCommandBuffer commandBuffer,
                                    const VkDependencyInfo *pDependencyInfo)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   /* Caches are flushed/invalidated at batch boundaries for now, nothing to do
    * for memory barriers assuming we implement barriers with the creation of a
    * new batch.
    * FIXME: We can probably do better with a CacheFlush job that has the
    * barrier flag set to true.
    */
   if (cmdbuf->state.batch) {
      panvk_per_arch(cmd_close_batch)(cmdbuf);
      panvk_cmd_preload_fb_after_batch_split(cmdbuf);
      panvk_cmd_open_batch(cmdbuf);
   }
}

static void
panvk_add_set_event_operation(struct panvk_cmd_buffer *cmdbuf,
                              struct panvk_event *event,
                              enum panvk_event_op_type type)
{
   struct panvk_event_op op = {
      .type = type,
      .event = event,
   };

   if (cmdbuf->state.batch == NULL) {
      /* No open batch, let's create a new one so this operation happens in
       * the right order.
       */
      panvk_cmd_open_batch(cmdbuf);
      util_dynarray_append(&cmdbuf->state.batch->event_ops,
                           struct panvk_event_op,
                           op);
      panvk_per_arch(cmd_close_batch)(cmdbuf);
   } else {
      /* Let's close the current batch so the operation executes before any
       * future commands.
       */
      util_dynarray_append(&cmdbuf->state.batch->event_ops,
                           struct panvk_event_op,
                           op);
      panvk_per_arch(cmd_close_batch)(cmdbuf);
      panvk_cmd_preload_fb_after_batch_split(cmdbuf);
      panvk_cmd_open_batch(cmdbuf);
   }
}

static void
panvk_add_wait_event_operation(struct panvk_cmd_buffer *cmdbuf,
                               struct panvk_event *event)
{
   struct panvk_event_op op = {
      .type = PANVK_EVENT_OP_WAIT,
      .event = event,
   };

   if (cmdbuf->state.batch == NULL) {
      /* No open batch, let's create a new one and have it wait for this event. */
      panvk_cmd_open_batch(cmdbuf);
      util_dynarray_append(&cmdbuf->state.batch->event_ops,
                           struct panvk_event_op,
                           op);
   } else {
      /* Let's close the current batch so any future commands wait on the
       * event signal operation.
       */
      if (cmdbuf->state.batch->fragment_job ||
          cmdbuf->state.batch->scoreboard.first_job) {
         panvk_per_arch(cmd_close_batch)(cmdbuf);
         panvk_cmd_preload_fb_after_batch_split(cmdbuf);
         panvk_cmd_open_batch(cmdbuf);
      }
      util_dynarray_append(&cmdbuf->state.batch->event_ops,
                           struct panvk_event_op,
                           op);
   }
}

void
panvk_per_arch(CmdSetEvent2)(VkCommandBuffer commandBuffer,
                             VkEvent _event,
                             const VkDependencyInfo *pDependencyInfo)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_event, event, _event);

   /* vkCmdSetEvent cannot be called inside a render pass */
   assert(cmdbuf->state.pass == NULL);

   panvk_add_set_event_operation(cmdbuf, event, PANVK_EVENT_OP_SET);
}

void
panvk_per_arch(CmdResetEvent2)(VkCommandBuffer commandBuffer,
                               VkEvent _event,
                               VkPipelineStageFlags2 stageMask)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_event, event, _event);

   /* vkCmdResetEvent cannot be called inside a render pass */
   assert(cmdbuf->state.pass == NULL);

   panvk_add_set_event_operation(cmdbuf, event, PANVK_EVENT_OP_RESET);
}

void
panvk_per_arch(CmdWaitEvents2)(VkCommandBuffer commandBuffer,
                               uint32_t eventCount,
                               const VkEvent *pEvents,
                               const VkDependencyInfo *pDependencyInfos)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   assert(eventCount > 0);

   for (uint32_t i = 0; i < eventCount; i++) {
      VK_FROM_HANDLE(panvk_event, event, pEvents[i]);
      panvk_add_wait_event_operation(cmdbuf, event);
   }
}

static VkResult
panvk_reset_cmdbuf(struct panvk_cmd_buffer *cmdbuf)
{
   vk_command_buffer_reset(&cmdbuf->vk);

   cmdbuf->record_result = VK_SUCCESS;

   list_for_each_entry_safe(struct panvk_batch, batch, &cmdbuf->batches, node) {
      list_del(&batch->node);
      util_dynarray_fini(&batch->jobs);
#if PAN_ARCH <= 5
      panfrost_bo_unreference(batch->tiler.ctx.midgard.polygon_list);
#endif

      util_dynarray_fini(&batch->event_ops);

      vk_free(&cmdbuf->pool->vk.alloc, batch);
   }

   panvk_pool_reset(&cmdbuf->desc_pool);
   panvk_pool_reset(&cmdbuf->tls_pool);
   panvk_pool_reset(&cmdbuf->varying_pool);
   cmdbuf->status = PANVK_CMD_BUFFER_STATUS_INITIAL;

   for (unsigned i = 0; i < MAX_BIND_POINTS; i++)
      memset(&cmdbuf->bind_points[i].desc_state.sets, 0, sizeof(cmdbuf->bind_points[0].desc_state.sets));

   return cmdbuf->record_result;
}

static void
panvk_destroy_cmdbuf(struct panvk_cmd_buffer *cmdbuf)
{
   struct panvk_device *device = cmdbuf->device;

   list_del(&cmdbuf->pool_link);

   list_for_each_entry_safe(struct panvk_batch, batch, &cmdbuf->batches, node) {
      list_del(&batch->node);
      util_dynarray_fini(&batch->jobs);
#if PAN_ARCH <= 5
      panfrost_bo_unreference(batch->tiler.ctx.midgard.polygon_list);
#endif

      util_dynarray_fini(&batch->event_ops);

      vk_free(&cmdbuf->pool->vk.alloc, batch);
   }

   panvk_pool_cleanup(&cmdbuf->desc_pool);
   panvk_pool_cleanup(&cmdbuf->tls_pool);
   panvk_pool_cleanup(&cmdbuf->varying_pool);
   vk_command_buffer_finish(&cmdbuf->vk);
   vk_free(&device->vk.alloc, cmdbuf);
}

static VkResult
panvk_create_cmdbuf(struct panvk_device *device,
                    struct panvk_cmd_pool *pool,
                    VkCommandBufferLevel level,
                    struct panvk_cmd_buffer **cmdbuf_out)
{
   struct panvk_cmd_buffer *cmdbuf;

   cmdbuf = vk_zalloc(&device->vk.alloc, sizeof(*cmdbuf),
                      8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!cmdbuf)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   VkResult result = vk_command_buffer_init(&cmdbuf->vk, &pool->vk, level);
   if (result != VK_SUCCESS) {
      vk_free(&device->vk.alloc, cmdbuf);
      return result;
   }

   cmdbuf->device = device;
   cmdbuf->pool = pool;

   if (pool) {
      list_addtail(&cmdbuf->pool_link, &pool->active_cmd_buffers);
      cmdbuf->queue_family_index = pool->vk.queue_family_index;
   } else {
      /* Init the pool_link so we can safely call list_del when we destroy
       * the command buffer
       */
      list_inithead(&cmdbuf->pool_link);
      cmdbuf->queue_family_index = PANVK_QUEUE_GENERAL;
   }

   cmdbuf->vk.level = level;

   if (level == VK_COMMAND_BUFFER_LEVEL_SECONDARY) {
      panvk_cpu_pool_init(&cmdbuf->desc_pool, &device->physical_device->pdev,
                          0, "Command buffer descriptor pool",
                          0xffffff00000000ULL);
      panvk_cpu_pool_init(&cmdbuf->tls_pool, &device->physical_device->pdev,
                          PAN_BO_INVISIBLE, "TLS pool",
                          0xfffffe00000000ULL);
      panvk_cpu_pool_init(&cmdbuf->varying_pool, &device->physical_device->pdev,
                          PAN_BO_INVISIBLE, "Varyings pool",
                          0xfffffd00000000ULL);
   } else {
      panvk_pool_init(&cmdbuf->desc_pool, &device->physical_device->pdev,
                      pool ? &pool->desc_bo_pool : NULL, 0, 64 * 1024,
                      "Command buffer descriptor pool", true);
      panvk_pool_init(&cmdbuf->tls_pool, &device->physical_device->pdev,
                      pool ? &pool->tls_bo_pool : NULL,
                     PAN_BO_INVISIBLE, 64 * 1024, "TLS pool", false);
      panvk_pool_init(&cmdbuf->varying_pool, &device->physical_device->pdev,
                      pool ? &pool->varying_bo_pool : NULL,
                      PAN_BO_INVISIBLE, 64 * 1024, "Varyings pool", false);
   }
   list_inithead(&cmdbuf->batches);
   cmdbuf->status = PANVK_CMD_BUFFER_STATUS_INITIAL;
   *cmdbuf_out = cmdbuf;
   return VK_SUCCESS;
}

VkResult
panvk_per_arch(AllocateCommandBuffers)(VkDevice _device,
                                       const VkCommandBufferAllocateInfo *pAllocateInfo,
                                       VkCommandBuffer *pCommandBuffers)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_cmd_pool, pool, pAllocateInfo->commandPool);

   VkResult result = VK_SUCCESS;
   unsigned i;

   for (i = 0; i < pAllocateInfo->commandBufferCount; i++) {
      struct panvk_cmd_buffer *cmdbuf = NULL;

      if (pAllocateInfo->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY &&
          !list_is_empty(&pool->free_cmd_buffers)) {
         cmdbuf = list_first_entry(
            &pool->free_cmd_buffers, struct panvk_cmd_buffer, pool_link);

         list_del(&cmdbuf->pool_link);
         list_addtail(&cmdbuf->pool_link, &pool->active_cmd_buffers);

         vk_command_buffer_finish(&cmdbuf->vk);
         result = vk_command_buffer_init(&cmdbuf->vk, &pool->vk, pAllocateInfo->level);
      } else {
         result = panvk_create_cmdbuf(device, pool, pAllocateInfo->level, &cmdbuf);
      }

      if (result != VK_SUCCESS)
         goto err_free_cmd_bufs;

      pCommandBuffers[i] = panvk_cmd_buffer_to_handle(cmdbuf);
   }

   return VK_SUCCESS;

err_free_cmd_bufs:
   panvk_per_arch(FreeCommandBuffers)(_device, pAllocateInfo->commandPool, i,
                                      pCommandBuffers);
   for (unsigned j = 0; j < i; j++)
      pCommandBuffers[j] = VK_NULL_HANDLE;

   return result;
}

void
panvk_per_arch(FreeCommandBuffers)(VkDevice device,
                                   VkCommandPool commandPool,
                                   uint32_t commandBufferCount,
                                   const VkCommandBuffer *pCommandBuffers)
{
   for (uint32_t i = 0; i < commandBufferCount; i++) {
      VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, pCommandBuffers[i]);

      if (cmdbuf) {
         if (cmdbuf->vk.level == VK_COMMAND_BUFFER_LEVEL_PRIMARY &&
             cmdbuf->pool) {
            list_del(&cmdbuf->pool_link);
            panvk_reset_cmdbuf(cmdbuf);
            list_addtail(&cmdbuf->pool_link,
                         &cmdbuf->pool->free_cmd_buffers);
         } else
            panvk_destroy_cmdbuf(cmdbuf);
      }
   }
}

VkResult
panvk_per_arch(ResetCommandBuffer)(VkCommandBuffer commandBuffer,
                                   VkCommandBufferResetFlags flags)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   return panvk_reset_cmdbuf(cmdbuf);
}

VkResult
panvk_per_arch(BeginCommandBuffer)(VkCommandBuffer commandBuffer,
                                   const VkCommandBufferBeginInfo *pBeginInfo)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VkResult result = VK_SUCCESS;

   if (cmdbuf->status != PANVK_CMD_BUFFER_STATUS_INITIAL) {
      /* If the command buffer has already been reset with
       * vkResetCommandBuffer, no need to do it again.
       */
      result = panvk_reset_cmdbuf(cmdbuf);
      if (result != VK_SUCCESS)
         return result;
   }

   memset(&cmdbuf->state, 0, sizeof(cmdbuf->state));

   cmdbuf->usage_flags = pBeginInfo->flags;
   if (cmdbuf->vk.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY) {
      if (cmdbuf->usage_flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) {
         cmdbuf->state.pass = panvk_render_pass_from_handle(pBeginInfo->pInheritanceInfo->renderPass);
         cmdbuf->state.subpass = &cmdbuf->state.pass->subpasses[pBeginInfo->pInheritanceInfo->subpass];
         memset(&cmdbuf->state.render_area, 0, sizeof(cmdbuf->state.render_area));
         cmdbuf->state.batch = vk_zalloc(&cmdbuf->pool->vk.alloc,
                                         sizeof(*cmdbuf->state.batch), 8,
                                         VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
         util_dynarray_init(&cmdbuf->state.batch->jobs, NULL);
         util_dynarray_init(&cmdbuf->state.batch->event_ops, NULL);
         cmdbuf->state.clear = NULL;
         memset(&cmdbuf->state.fb.info, 0, sizeof(cmdbuf->state.fb.info));
      }
   }

   cmdbuf->status = PANVK_CMD_BUFFER_STATUS_RECORDING;

   return VK_SUCCESS;
}

void
panvk_per_arch(DestroyCommandPool)(VkDevice _device,
                                   VkCommandPool commandPool,
                                   const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_cmd_pool, pool, commandPool);

   list_for_each_entry_safe(struct panvk_cmd_buffer, cmdbuf,
                            &pool->active_cmd_buffers, pool_link)
      panvk_destroy_cmdbuf(cmdbuf);

   list_for_each_entry_safe(struct panvk_cmd_buffer, cmdbuf,
                            &pool->free_cmd_buffers, pool_link)
      panvk_destroy_cmdbuf(cmdbuf);

   panvk_bo_pool_cleanup(&pool->desc_bo_pool);
   panvk_bo_pool_cleanup(&pool->varying_bo_pool);
   panvk_bo_pool_cleanup(&pool->tls_bo_pool);

   vk_command_pool_finish(&pool->vk);
   vk_free2(&device->vk.alloc, pAllocator, pool);
}

VkResult
panvk_per_arch(ResetCommandPool)(VkDevice device,
                                 VkCommandPool commandPool,
                                 VkCommandPoolResetFlags flags)
{
   VK_FROM_HANDLE(panvk_cmd_pool, pool, commandPool);
   VkResult result;

   list_for_each_entry(struct panvk_cmd_buffer, cmdbuf, &pool->active_cmd_buffers,
                       pool_link)
   {
      result = panvk_reset_cmdbuf(cmdbuf);
      if (result != VK_SUCCESS)
         return result;
   }

   return VK_SUCCESS;
}

void
panvk_per_arch(TrimCommandPool)(VkDevice device,
                                VkCommandPool commandPool,
                                VkCommandPoolTrimFlags flags)
{
   VK_FROM_HANDLE(panvk_cmd_pool, pool, commandPool);

   if (!pool)
      return;

   list_for_each_entry_safe(struct panvk_cmd_buffer, cmdbuf,
                            &pool->free_cmd_buffers, pool_link)
      panvk_destroy_cmdbuf(cmdbuf);
}

void
panvk_per_arch(CmdDispatch)(VkCommandBuffer commandBuffer,
                            uint32_t x,
                            uint32_t y,
                            uint32_t z)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   const struct panfrost_device *pdev =
      &cmdbuf->device->physical_device->pdev;
   struct panvk_dispatch_info dispatch = {
      .wg_count = { x, y, z },
   };

   panvk_per_arch(cmd_close_batch)(cmdbuf);
   struct panvk_batch *batch = panvk_cmd_open_batch(cmdbuf);

   struct panvk_cmd_bind_point_state *bind_point_state =
      panvk_cmd_get_bind_point_state(cmdbuf, COMPUTE);
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;
   struct panfrost_ptr job =
      pan_pool_alloc_desc(&cmdbuf->desc_pool.base, COMPUTE_JOB);

   cmdbuf->state.compute.wg_count = dispatch.wg_count;
   panvk_per_arch(cmd_alloc_tls_desc)(cmdbuf, false);
   dispatch.tsd = batch->tls.gpu;

   panvk_prepare_non_vs_attribs(cmdbuf, bind_point_state);
   dispatch.attributes = desc_state->non_vs_attribs;
   dispatch.attribute_bufs = desc_state->non_vs_attrib_bufs;

   panvk_cmd_prepare_ubos(cmdbuf, bind_point_state);
   dispatch.ubos = desc_state->ubos;

   panvk_cmd_prepare_textures(cmdbuf, bind_point_state);
   dispatch.textures = desc_state->textures;

   panvk_cmd_prepare_samplers(cmdbuf, bind_point_state);
   dispatch.samplers = desc_state->samplers;

   panvk_per_arch(emit_compute_job)(pipeline, &dispatch, job.cpu);
   panfrost_add_job(&cmdbuf->desc_pool.base, &batch->scoreboard,
                    MALI_JOB_TYPE_COMPUTE, false, false, 0, 0,
                    &job, false);

   batch->tlsinfo.tls.size = pipeline->tls_size;
   batch->tlsinfo.wls.size = pipeline->wls_size;
   if (batch->tlsinfo.wls.size) {
      batch->wls_total_size =
         pan_wls_mem_size(pdev, &dispatch.wg_count, batch->tlsinfo.wls.size);
   }

   panvk_per_arch(cmd_close_batch)(cmdbuf);
   desc_state->dirty = 0;
}

struct panvk_reloc_ctx {
   struct {
      struct panvk_cmd_buffer *cmdbuf;
      struct panfrost_ptr desc_base, varying_base;
   } src, dst;
   uint32_t desc_size, varying_size;
   uint16_t job_idx_offset;
};

#define PANVK_RELOC_SET(dst_ptr, type, field, value) \
        do { \
           mali_ptr *dst_addr = (dst_ptr) + pan_field_byte_offset(type, field); \
           *dst_addr = value; \
        } while (0)


#define PANVK_RELOC_COPY(ctx, pool, src_ptr, dst_ptr, type, field) \
        do { \
           mali_ptr *src_addr = (src_ptr) + pan_field_byte_offset(type, field); \
           if (*src_addr < (ctx)->src.pool ## _base.gpu || \
               *src_addr >= (ctx)->src.pool ## _base.gpu + (ctx)->pool ## _size) \
              break; \
           mali_ptr *dst_addr = (dst_ptr) + pan_field_byte_offset(type, field); \
           *dst_addr = *src_addr - (ctx)->src.pool ## _base.gpu + (ctx)->dst.pool ## _base.gpu; \
        } while (0)

#define PANVK_RELOC_CHECK_ADDR_DESC_BASE(ctx, p) \
        (p >= ctx->src.desc_base.gpu && \
           p < ctx->src.desc_base.gpu + ctx->desc_size)

#define PANVK_RELOC_CHECK_ADDR_VARYING_BASE(ctx, p) \
        (p >= ctx->src.varying_base.gpu && \
           p < ctx->src.varying_base.gpu + ctx->varying_size)

static void
panvk_reloc_ubos(const struct panvk_reloc_ctx *ctx,
                 void *src_ptr)
{
   mali_ptr *src_ubos = src_ptr;

   if(!PANVK_RELOC_CHECK_ADDR_DESC_BASE(ctx, *src_ubos))
      return;

   uint64_t desc_offset = *src_ubos - ctx->src.desc_base.gpu;
   void *src_ubo = ctx->src.desc_base.cpu + desc_offset;
   void *dst_ubo = ctx->dst.desc_base.cpu + desc_offset;
   while (*((mali_ptr *)src_ubo) != 0) {
      mali_ptr *src_addr_ptr = src_ubo;
      mali_ptr *dst_addr_ptr = dst_ubo;
      mali_ptr addr = (*src_addr_ptr >> 12) << 4;

      if (PANVK_RELOC_CHECK_ADDR_DESC_BASE(ctx, addr)) {
         mali_ptr new_addr = addr - ctx->src.desc_base.gpu + ctx->dst.desc_base.gpu;
         assert((new_addr & 0xff0000000000000fULL) == 0);
         *dst_addr_ptr = (*src_addr_ptr & 0xfff) | ((new_addr >> 4) << 12);
      }
      src_ubo += pan_size(UNIFORM_BUFFER);
      dst_ubo += pan_size(UNIFORM_BUFFER);
   }
}

static void
panvk_reloc_varying_buffers(const struct panvk_reloc_ctx *ctx, void *src_ptr)
{
   mali_ptr *src_varying_bufs = src_ptr;
   if (!PANVK_RELOC_CHECK_ADDR_DESC_BASE(ctx, *src_varying_bufs))
      return;

   uint64_t desc_offset = *src_varying_bufs - ctx->src.desc_base.gpu;
   void *src_varying_buf = ctx->src.desc_base.cpu + desc_offset;
   void *dst_varying_buf = ctx->dst.desc_base.cpu + desc_offset;
   while (*((mali_ptr *)src_varying_buf) != 0) {
      mali_ptr *src_addr_ptr = src_varying_buf;
      mali_ptr *dst_addr_ptr = dst_varying_buf;
      mali_ptr addr_mask = 0xffffffffffffc0ULL;
      mali_ptr addr = *src_addr_ptr & addr_mask;

      if (PANVK_RELOC_CHECK_ADDR_VARYING_BASE(ctx, addr)) {
         addr = addr - ctx->src.varying_base.gpu + ctx->dst.varying_base.gpu;
         assert((addr & ~addr_mask) == 0);
         *dst_addr_ptr = (*src_addr_ptr & ~addr_mask) | addr;
      } else if (PANVK_RELOC_CHECK_ADDR_DESC_BASE(ctx, addr)) {
         addr = addr - ctx->src.desc_base.gpu + ctx->dst.desc_base.gpu;
         assert((addr & ~addr_mask) == 0);
         *dst_addr_ptr = (*src_addr_ptr & ~addr_mask) | addr;
      }

      src_varying_buf += pan_size(ATTRIBUTE_BUFFER);
      dst_varying_buf += pan_size(ATTRIBUTE_BUFFER);
   }
}

static void
panvk_reloc_draw(const struct panvk_reloc_ctx *ctx,
                 void *src_ptr, void *dst_ptr)
{
   /* Sometimes the position array is allocated from the descriptor pool
    * and filled by the CPU. Let's call PANVK_RELOC_COPY() and let it choose
    * which relocation should happen (if any).
    */
   PANVK_RELOC_COPY(ctx, varying, src_ptr, dst_ptr, DRAW, position);
   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, position);

   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, uniform_buffers);
   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, textures);
   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, samplers);
   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, push_uniforms);
   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, state);
   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, attribute_buffers);
   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, attributes);
   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, varying_buffers);
   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, varyings);
   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, viewport);
   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, occlusion);

   panvk_reloc_ubos(ctx, src_ptr + pan_field_byte_offset(DRAW, uniform_buffers));
   panvk_reloc_varying_buffers(ctx, src_ptr + pan_field_byte_offset(DRAW, varying_buffers));

   if (ctx->src.cmdbuf->usage_flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) {
      panvk_per_arch(cmd_alloc_tls_desc)(ctx->dst.cmdbuf, true);
      PANVK_RELOC_SET(dst_ptr, DRAW, thread_storage, ctx->dst.cmdbuf->state.batch->tls.gpu);
   } else {
      PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, DRAW, thread_storage);
   }
}

static void
panvk_reloc_write_value_job_payload(const struct panvk_reloc_ctx *ctx,
                                    void *src_ptr, void *dst_ptr)
{
   PANVK_RELOC_COPY(ctx, desc,
                    pan_section_ptr(src_ptr, WRITE_VALUE_JOB, PAYLOAD),
                    pan_section_ptr(dst_ptr, WRITE_VALUE_JOB, PAYLOAD),
                    WRITE_VALUE_JOB_PAYLOAD, address);
}

static void
panvk_reloc_compute_job_payload(const struct panvk_reloc_ctx *ctx,
                                void *src_ptr, void *dst_ptr)
{
   panvk_reloc_draw(ctx, pan_section_ptr(src_ptr, COMPUTE_JOB, DRAW),
                    pan_section_ptr(dst_ptr, COMPUTE_JOB, DRAW));
}

static void
panvk_reloc_tiler_job_payload(const struct panvk_reloc_ctx *ctx,
                              void *src_ptr, void *dst_ptr)
{
   panvk_reloc_draw(ctx, pan_section_ptr(src_ptr, TILER_JOB, DRAW),
                    pan_section_ptr(dst_ptr, TILER_JOB, DRAW));

   PANVK_RELOC_COPY(ctx, varying,
                    pan_section_ptr(src_ptr, TILER_JOB, PRIMITIVE_SIZE),
                    pan_section_ptr(dst_ptr, TILER_JOB, PRIMITIVE_SIZE),
                    PRIMITIVE_SIZE, size_array);

#if PAN_ARCH >= 6
   if (ctx->src.cmdbuf->usage_flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) {
      struct panvk_batch *dst_batch = ctx->dst.cmdbuf->state.batch;
      assert(dst_batch);

      panvk_per_arch(cmd_alloc_fb_desc)(ctx->dst.cmdbuf);
      panvk_per_arch(cmd_prepare_tiler_context)(ctx->dst.cmdbuf);

      assert(dst_batch->tiler.ctx.bifrost);

      PANVK_RELOC_SET(pan_section_ptr(dst_ptr, TILER_JOB, TILER),
                      TILER_POINTER, address, dst_batch->tiler.ctx.bifrost);
   } else {
      PANVK_RELOC_COPY(ctx, desc,
                       pan_section_ptr(src_ptr, TILER_JOB, TILER),
                       pan_section_ptr(dst_ptr, TILER_JOB, TILER),
                       TILER_POINTER, address);
      mali_ptr *src_tiler = pan_section_ptr(src_ptr, TILER_JOB, TILER) + pan_field_byte_offset(TILER_POINTER, address);

      if (!PANVK_RELOC_CHECK_ADDR_DESC_BASE(ctx, *src_tiler))
         return;

      uint64_t tiler_offset = *src_tiler - ctx->src.desc_base.gpu;
      mali_ptr *src_tiler_cpu = ctx->src.desc_base.cpu + tiler_offset;
      mali_ptr *dst_tiler_cpu = ctx->dst.desc_base.cpu + tiler_offset;

      PANVK_RELOC_COPY(ctx, desc,
                       (void *)src_tiler_cpu,
                       (void *)dst_tiler_cpu,
                       TILER_CONTEXT, heap);
   }
#endif
}

static void
panvk_reloc_fragment_job_payload(const struct panvk_reloc_ctx *ctx,
                                 void *src_ptr, void *dst_ptr)
{
   assert(!(ctx->src.cmdbuf->usage_flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT));

   void* src_payload = pan_section_ptr(src_ptr, FRAGMENT_JOB, PAYLOAD);
   void* dst_payload = pan_section_ptr(dst_ptr, FRAGMENT_JOB, PAYLOAD);

   PANVK_RELOC_COPY(ctx, desc,
                    src_payload,
                    dst_payload,
                    FRAGMENT_JOB_PAYLOAD, framebuffer);

   int off = pan_field_byte_offset(FRAGMENT_JOB_PAYLOAD, framebuffer);

   mali_ptr *src_fb_ptr =  (mali_ptr *)((uint8_t *)src_payload + off);

   if (!PANVK_RELOC_CHECK_ADDR_DESC_BASE(ctx, *src_fb_ptr))
      return;

   uint64_t fb_offset = *src_fb_ptr - ctx->src.desc_base.gpu;

   void* src_fb = (void *)((uint8_t*)ctx->src.desc_base.cpu + fb_offset);
   void* dst_fb = (void *)((uint8_t*)ctx->src.desc_base.cpu + fb_offset);

#if PAN_ARCH >= 6
   mali_ptr* fb_param_src = pan_section_ptr(src_fb, FRAMEBUFFER, PARAMETERS);
   mali_ptr* fb_param_dst = pan_section_ptr(dst_fb, FRAMEBUFFER, PARAMETERS);

   PANVK_RELOC_COPY(ctx, desc,
                    fb_param_src,
                    fb_param_dst,
                    FRAMEBUFFER_PARAMETERS, tiler);
#else
   // TODO
   void* src_tiler = pan_section_ptr(src_fb, FRAMEBUFFER, TILER);
   void* dst_tiler = pan_section_ptr(dst_fb, FRAMEBUFFER, TILER);

   PANVK_RELOC_COPY(ctx, desc,
                    src_tiler,
                    dst_tiler,
                    TILER_CONTEXT, polygon_list);
   PANVK_RELOC_COPY(ctx, desc,
                    src_tiler,
                    dst_tiler,
                    TILER_CONTEXT, polygon_list_body);
   PANVK_RELOC_COPY(ctx, desc,
                    src_tiler,
                    dst_tiler,
                    TILER_CONTEXT, heap_start);
   PANVK_RELOC_COPY(ctx, desc,
                    src_tiler,
                    dst_tiler,
                    TILER_CONTEXT, heap_end);
#endif
}

static void
panvk_reloc_job(const struct panvk_reloc_ctx *ctx,
                void *src_ptr,
                void *dst_ptr)
{
   /* TODO: Add helpers to retrieve a field value without unpacking the whole desc. */
   uint32_t w4 = ((uint32_t *)src_ptr)[4];

   uint8_t type = (w4 >> 1) & 0x7f;
   uint32_t job_idx =  *(uint16_t *)((uint8_t *)src_ptr + pan_field_byte_offset(JOB_HEADER, index)) + ctx->job_idx_offset;
   uint32_t dep1 =  *(uint16_t *)((uint8_t *)src_ptr + pan_field_byte_offset(JOB_HEADER, dependency_1));
   uint32_t dep2 =  *(uint16_t *)((uint8_t *)src_ptr + pan_field_byte_offset(JOB_HEADER, dependency_2));

   if (dep1)
      dep1 += ctx->job_idx_offset;
   if (dep2)
      dep1 += ctx->job_idx_offset;

   switch (type) {
   case MALI_JOB_TYPE_NULL:
   case MALI_JOB_TYPE_CACHE_FLUSH:
      break;
   case MALI_JOB_TYPE_WRITE_VALUE:
      panvk_reloc_write_value_job_payload(ctx, src_ptr, dst_ptr);
      break;
   case MALI_JOB_TYPE_COMPUTE:
   case MALI_JOB_TYPE_VERTEX:
      panvk_reloc_compute_job_payload(ctx, src_ptr, dst_ptr);
      break;
   case MALI_JOB_TYPE_TILER:
      panvk_reloc_tiler_job_payload(ctx, src_ptr, dst_ptr);
      ctx->dst.cmdbuf->state.batch->scoreboard.tiler_dep = job_idx;
      if (!ctx->dst.cmdbuf->state.batch->scoreboard.first_tiler) {
         ctx->dst.cmdbuf->state.batch->scoreboard.first_tiler = dst_ptr;
         ctx->dst.cmdbuf->state.batch->scoreboard.first_tiler_dep1 = dep1;
      } else if (!dep2) {
         dep2 = ctx->dst.cmdbuf->state.batch->scoreboard.tiler_dep;
      }
      break;
   case MALI_JOB_TYPE_FRAGMENT:
      panvk_reloc_fragment_job_payload(ctx, src_ptr, dst_ptr);
      break;
   default:
      unreachable("Unsupported job type!");
   }

   assert(job_idx <= UINT16_MAX);

   uint16_t* dst_job_idx_ptr = (uint16_t *)((uint8_t *)dst_ptr + pan_field_byte_offset(JOB_HEADER, index));
   uint16_t* dst_dep1_ptr = (uint16_t *)((uint8_t *)dst_ptr + pan_field_byte_offset(JOB_HEADER, dependency_1));
   uint16_t* dst_dep2_ptr = (uint16_t *)((uint8_t *)dst_ptr + pan_field_byte_offset(JOB_HEADER, dependency_2));

   *dst_job_idx_ptr = job_idx;
   *dst_dep1_ptr = dep1;
   *dst_dep2_ptr = dep2;

   PANVK_RELOC_COPY(ctx, desc, src_ptr, dst_ptr, JOB_HEADER, next);
}

void
panvk_per_arch(CmdExecuteCommands)(VkCommandBuffer commandBuffer,
                                   uint32_t commandBufferCount,
                                   const VkCommandBuffer *pCmdBuffers)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, dst_cmdbuf, commandBuffer);

   // to connect jobs of different command buffers
   void *prev_cmdbuf_last_job = NULL;

   for (uint32_t i = 0; i < commandBufferCount; i++) {
      VK_FROM_HANDLE(panvk_cmd_buffer, src_cmdbuf, pCmdBuffers[i]);
      uint32_t desc_buf_size = src_cmdbuf->desc_pool.transient_offset;
      uint32_t varyings_buf_size = src_cmdbuf->varying_pool.transient_offset;

      struct panfrost_ptr src_desc_ptr = src_cmdbuf->desc_pool.cpu_bo.ptr;
      struct panfrost_ptr src_varyings_ptr = src_cmdbuf->varying_pool.cpu_bo.ptr;
      struct panfrost_ptr dst_desc_ptr =
         pan_pool_alloc_aligned(&dst_cmdbuf->desc_pool.base, desc_buf_size, 4096);
      struct panfrost_ptr dst_varyings_ptr =
         pan_pool_alloc_aligned(&dst_cmdbuf->varying_pool.base, varyings_buf_size, 64);

      bool cmdbuf_first_job = true;

      memcpy(dst_desc_ptr.cpu, src_desc_ptr.cpu, desc_buf_size);

      struct panvk_reloc_ctx reloc_ctx = {
         .src = {
            .desc_base = src_desc_ptr,
            .varying_base = src_varyings_ptr,
            .cmdbuf = src_cmdbuf,
         },
         .dst = {
            .desc_base = dst_desc_ptr,
            .varying_base = dst_varyings_ptr,
            .cmdbuf = dst_cmdbuf,
         },
         .desc_size = desc_buf_size,
         .varying_size = varyings_buf_size,
      };

      struct panvk_batch *last_src_batch =
         list_last_entry(&src_cmdbuf->batches, struct panvk_batch, node);

      list_for_each_entry(struct panvk_batch, batch, &src_cmdbuf->batches, node) {
         struct panvk_batch *dst_batch = dst_cmdbuf->state.batch;
         bool set_event = false, wait_event = false;

         util_dynarray_foreach(&batch->event_ops, struct panvk_event_op, eop) {
            if (eop->type == PANVK_EVENT_OP_SET ||
                eop->type == PANVK_EVENT_OP_RESET)
               set_event = true;
            if (eop->type == PANVK_EVENT_OP_SET)
               wait_event = true;
         }

         if (dst_batch &&
             (wait_event ||
              dst_batch->scoreboard.job_index + dst_batch->scoreboard.job_index > UINT16_MAX)) {
            panvk_per_arch(cmd_close_batch)(dst_cmdbuf);
            panvk_cmd_preload_fb_after_batch_split(dst_cmdbuf);
            dst_batch = panvk_cmd_open_batch(dst_cmdbuf);
         }

         if (!dst_batch)
            dst_batch = panvk_cmd_open_batch(dst_cmdbuf);

         util_dynarray_foreach(&batch->event_ops, struct panvk_event_op, eop)
            util_dynarray_append(&dst_batch->event_ops, struct panvk_event_op, *eop);

         uint32_t subjob_idx = 0;

         util_dynarray_foreach(&batch->jobs, uintptr_t, job_offset_ptr) {
            uintptr_t job_offset = *job_offset_ptr;
            void *src_ptr = src_desc_ptr.cpu + job_offset;
            void *dst_ptr = dst_desc_ptr.cpu + job_offset;

            if (cmdbuf_first_job && prev_cmdbuf_last_job) {
               PANVK_RELOC_SET(prev_cmdbuf_last_job, JOB_HEADER, next, dst_desc_ptr.gpu + job_offset);
               cmdbuf_first_job = false;
            }

            if (src_ptr < src_cmdbuf->desc_pool.cpu_bo.ptr.cpu ||
                src_ptr >= src_cmdbuf->desc_pool.cpu_bo.ptr.cpu + src_cmdbuf->desc_pool.cpu_bo.size)
	            assert(src_ptr >= src_cmdbuf->desc_pool.cpu_bo.ptr.cpu &&
                  src_ptr < src_cmdbuf->desc_pool.cpu_bo.ptr.cpu + src_cmdbuf->desc_pool.cpu_bo.size);
            panvk_reloc_job(&reloc_ctx, src_ptr, dst_ptr);
            util_dynarray_append(&dst_batch->jobs, void *, dst_ptr);
            dst_batch->scoreboard.prev_job = dst_ptr;
            prev_cmdbuf_last_job = dst_ptr;
            subjob_idx++;
         }

         if (batch->fragment_job) {
             dst_batch->fragment_job = batch->fragment_job - src_desc_ptr.gpu + dst_desc_ptr.gpu;
#if PAN_ARCH >= 6
            memcpy(dst_batch->tiler.templ, batch->tiler.templ, sizeof(batch->tiler.templ));
            uintptr_t tiler_ctx_offset = batch->tiler.descs.gpu - src_desc_ptr.gpu;
            dst_batch->tiler.descs.cpu = dst_desc_ptr.cpu + tiler_ctx_offset;
            dst_batch->tiler.descs.gpu = dst_desc_ptr.gpu + tiler_ctx_offset;
            memcpy(dst_batch->tiler.descs.cpu, dst_batch->tiler.templ,
                   pan_size(TILER_CONTEXT) + pan_size(TILER_HEAP));
#else
            // TODO
            panvk_copy_fb_desc(dst_cmdbuf, batch->fb.desc.cpu);
            memcpy(dst_batch->tiler.templ,
               pan_section_ptr(batch->fb.desc.cpu, FRAMEBUFFER, TILER),
               pan_size(TILER_CONTEXT));
#endif
         }

         if (!dst_batch->scoreboard.first_job && batch->scoreboard.first_job)
            dst_batch->scoreboard.first_job = batch->scoreboard.first_job - src_desc_ptr.gpu + dst_desc_ptr.gpu;

         if (!dst_batch->scoreboard.first_tiler && batch->scoreboard.first_tiler) {
            uintptr_t job_offset = (uintptr_t)batch->scoreboard.first_tiler - (uintptr_t)src_desc_ptr.cpu;

            dst_batch->scoreboard.first_tiler = dst_desc_ptr.cpu + job_offset;
            dst_batch->scoreboard.first_tiler_dep1 = batch->scoreboard.first_tiler_dep1 + reloc_ctx.job_idx_offset;
         }

         if (batch->scoreboard.tiler_dep)
            dst_batch->scoreboard.tiler_dep = batch->scoreboard.tiler_dep + reloc_ctx.job_idx_offset;

         dst_batch->scoreboard.job_index += batch->scoreboard.job_index;

         if (set_event || batch != last_src_batch) {
            panvk_per_arch(cmd_close_batch)(dst_cmdbuf);
            panvk_cmd_preload_fb_after_batch_split(dst_cmdbuf);
            panvk_cmd_open_batch(dst_cmdbuf);
         }
      }
   }
}

unsigned
panvk_per_arch(cmd_add_job)(struct panvk_cmd_buffer *cmdbuf,
                            unsigned type,
                            bool barrier, bool suppress_prefetch,
                            unsigned local_dep, unsigned global_dep,
                            const struct panfrost_ptr *job,
                            bool inject)
{
   struct pan_scoreboard *scoreboard = &cmdbuf->state.batch->scoreboard;

   panvk_cmd_fix_cpu_pointers(cmdbuf);

   panvk_per_arch(cmd_add_job_ptr)(cmdbuf, job->cpu);

   return panfrost_add_job(&cmdbuf->desc_pool.base, scoreboard,
                           type, barrier, suppress_prefetch,
                           local_dep, global_dep, job, inject);
}
