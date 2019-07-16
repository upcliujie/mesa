/*
 * Copyright © 2021 Collabora Ltd.
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

#include "nir/nir_builder.h"
#include "pan_encoder.h"

#include "panvk_private.h"

#include "vk_format.h"

static void
panvk_meta_blit_emit_fb(struct panvk_cmd_buffer *cmdbuf,
                        struct pan_fb_info *fbinfo,
                        const struct pan_tls_info *tlsinfo)
{
   const struct panfrost_device *pdev =
      &cmdbuf->device->physical_device->pdev;
   struct panvk_batch *batch = cmdbuf->state.batch;
   unsigned zs_ext = (fbinfo->zs.view.zs || fbinfo->zs.view.s) ? 1 : 0;

   if (pan_is_bifrost(pdev)) {
      panvk_cmd_get_bifrost_tiler_context(cmdbuf,
                                          fbinfo->width,
                                          fbinfo->height);
   } else {
      panvk_cmd_get_midgard_polygon_list(cmdbuf,
                                         fbinfo->width,
                                         fbinfo->height,
                                         true);
   }

   struct panfrost_ptr jobs[2];
   unsigned njobs =
      pan_preload_fb(&cmdbuf->desc_pool, &batch->scoreboard, fbinfo,
                     batch->tls.gpu,
                     pan_is_bifrost(pdev) ? batch->tiler.bifrost_descs.gpu : 0,
                     jobs);

   assert(njobs <= 2);
   for (unsigned i = 0; i < njobs; i++)
      util_dynarray_append(&batch->jobs, void *, jobs[i].cpu);

   batch->fb.desc =
      panfrost_pool_alloc_desc_aggregate(&cmdbuf->desc_pool,
                                         PAN_DESC(MULTI_TARGET_FRAMEBUFFER),
                                         PAN_DESC_ARRAY(zs_ext, ZS_CRC_EXTENSION),
                                         PAN_DESC_ARRAY(1, RENDER_TARGET));

   batch->fb.desc.gpu |=
      pan_emit_fbd(pdev, fbinfo, tlsinfo,
                   &cmdbuf->state.batch->tiler.ctx,
                   cmdbuf->state.batch->fb.desc.cpu);
}

static void
panvk_meta_blit_emit_tls(struct panvk_cmd_buffer *cmdbuf,
                         const struct pan_tls_info *info)
{
   const struct panfrost_device *pdev =
      &cmdbuf->device->physical_device->pdev;
   struct panvk_batch *batch = cmdbuf->state.batch;

   assert(batch && !batch->tls.gpu);
   if (!pan_is_bifrost(pdev))
      return;

   batch->tls =
      panfrost_pool_alloc_aligned(&cmdbuf->desc_pool,
                                  MALI_LOCAL_STORAGE_LENGTH, 64);
   pan_emit_tls(pdev, info, batch->tls.cpu);
}

static void
panvk_meta_blit_emit_fragment_job(struct panvk_cmd_buffer *cmdbuf,
                                  const struct pan_fb_info *fbinfo)
{
   const struct panfrost_device *pdev =
      &cmdbuf->device->physical_device->pdev;
   struct panvk_batch *batch = cmdbuf->state.batch;
   struct panfrost_ptr job_ptr =
      panfrost_pool_alloc_desc(&cmdbuf->desc_pool, FRAGMENT_JOB);
   job_ptr =
      panfrost_pool_alloc_aligned(&cmdbuf->desc_pool, MALI_FRAGMENT_JOB_LENGTH, 0x200);

   pan_emit_fragment_job(pdev, fbinfo, batch->fb.desc.gpu, job_ptr.cpu),
   cmdbuf->state.batch->fragment_job = job_ptr.gpu;
   util_dynarray_append(&batch->jobs, void *, job_ptr.cpu);
}

static void
panvk_meta_blit_close_batch(struct panvk_cmd_buffer *cmdbuf)
{
   const struct panfrost_device *pdev =
      &cmdbuf->device->physical_device->pdev;
   struct panvk_batch *batch = cmdbuf->state.batch;

   if (!pan_is_bifrost(pdev) && batch->scoreboard.first_tiler) {
      mali_ptr polygon_list =
         batch->tiler.ctx.midgard.polygon_list->ptr.gpu;
      struct panfrost_ptr writeval_job =
         panfrost_scoreboard_initialize_tiler(&cmdbuf->desc_pool,
                                              &batch->scoreboard,
                                              polygon_list);
      if (writeval_job.cpu)
         util_dynarray_append(&batch->jobs, void *, writeval_job.cpu);

      memcpy(&batch->tiler.templ.midgard,
             pan_section_ptr(batch->fb.desc.cpu,
                             MULTI_TARGET_FRAMEBUFFER, TILER),
             sizeof(batch->tiler.templ.midgard));
   }

   list_addtail(&cmdbuf->state.batch->node, &cmdbuf->batches);
   cmdbuf->state.batch = NULL;
}

static void
panvk_meta_blit(struct panvk_cmd_buffer *cmdbuf,
                const struct pan_blit_info *blitinfo)
{
   struct panfrost_device *pdev = &cmdbuf->device->physical_device->pdev;
   struct pan_blit_context ctx;
   struct pan_image_state states[2] = { 0 };
   struct pan_image_view views[2] = {
      {
         .format = blitinfo->dst.planes[0].format,
         .dim = MALI_TEXTURE_DIMENSION_2D,
         .image = blitinfo->dst.planes[0].image,
         .nr_samples = blitinfo->dst.planes[0].image->layout.nr_samples,
         .first_level = blitinfo->dst.level,
         .last_level = blitinfo->dst.level,
         .swizzle = { PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y, PIPE_SWIZZLE_Z, PIPE_SWIZZLE_W },
      },
   };
   struct pan_fb_info fbinfo = {
      .width = u_minify(blitinfo->dst.planes[0].image->layout.width, blitinfo->dst.level),
      .height = u_minify(blitinfo->dst.planes[0].image->layout.height, blitinfo->dst.level),
      .extent = {
         .minx = MAX2(MIN2(blitinfo->dst.start.x, blitinfo->dst.end.x), 0),
         .miny = MAX2(MIN2(blitinfo->dst.start.y, blitinfo->dst.end.y), 0),
         .maxx = MAX2(blitinfo->dst.start.x, blitinfo->dst.end.x),
         .maxy = MAX2(blitinfo->dst.start.y, blitinfo->dst.end.y),
      },
      .nr_samples = blitinfo->dst.planes[0].image->layout.nr_samples,
   };
   struct pan_tls_info tlsinfo = { 0 };

   fbinfo.extent.maxx = MIN2(fbinfo.extent.maxx, fbinfo.width - 1);
   fbinfo.extent.maxy = MIN2(fbinfo.extent.maxy, fbinfo.height - 1);

   /* TODO: don't force preloads of dst resources if unneeded */
   states[0].slices[blitinfo->dst.level].data_valid = true;

   const struct util_format_description *fdesc =
      util_format_description(blitinfo->dst.planes[0].image->layout.format);

   if (util_format_has_depth(fdesc)) {
      /* We want the image format here, otherwise we might lose one of the
       * component.
       */
      views[0].format = blitinfo->dst.planes[0].image->layout.format;
      fbinfo.zs.view.zs = &views[0];
      fbinfo.zs.state.zs = &states[0];
      fbinfo.zs.preload.z = true;
      fbinfo.zs.preload.s = util_format_has_stencil(fdesc);
   } else if (util_format_has_stencil(fdesc)) {
      fbinfo.zs.view.s = &views[0];
      fbinfo.zs.state.s = &states[0];
      fbinfo.zs.preload.s = true;
   } else {
      fbinfo.rt_count = 1;
      fbinfo.rts[0].view = &views[0];
      fbinfo.rts[0].state = &states[0];
      fbinfo.rts[0].preload = true;
   }

   if (blitinfo->dst.planes[1].format != PIPE_FORMAT_NONE) {
      /* TODO: don't force preloads of dst resources if unneeded */
      states[1].slices[blitinfo->dst.level].data_valid = true;
      views[1].format = blitinfo->dst.planes[1].format;
      views[1].dim = MALI_TEXTURE_DIMENSION_2D;
      views[1].image = blitinfo->dst.planes[1].image;
      views[1].nr_samples = blitinfo->dst.planes[1].image->layout.nr_samples;
      views[1].first_level = blitinfo->dst.level;
      views[1].last_level = blitinfo->dst.level;
      views[1].swizzle[0] = PIPE_SWIZZLE_X;
      views[1].swizzle[1] = PIPE_SWIZZLE_Y;
      views[1].swizzle[2] = PIPE_SWIZZLE_Z;
      views[1].swizzle[3] = PIPE_SWIZZLE_W;
      fbinfo.zs.view.s = &views[1];
      fbinfo.zs.state.s = &states[1];
   }

   if (cmdbuf->state.batch)
      panvk_cmd_close_batch(cmdbuf);

   pan_blit_ctx_init(pdev, blitinfo, &cmdbuf->desc_pool, &ctx);
   do {
      if (ctx.dst.cur_layer < 0)
         continue;

      panvk_cmd_open_batch(cmdbuf);

      struct panvk_batch *batch = cmdbuf->state.batch;
      mali_ptr tsd, tiler;

      views[0].first_layer = views[0].last_layer = ctx.dst.cur_layer;
      views[1].first_layer = views[1].last_layer = views[0].first_layer;
      panvk_meta_blit_emit_tls(cmdbuf, &tlsinfo);
      panvk_meta_blit_emit_fb(cmdbuf, &fbinfo, &tlsinfo);

      if (pan_is_bifrost(pdev)) {
         tsd = batch->tls.gpu;
         tiler = batch->tiler.bifrost_descs.gpu;
      } else {
         tsd = batch->fb.desc.gpu;
         tiler = 0;
      }

      struct panfrost_ptr job =
         pan_blit(&ctx, &cmdbuf->desc_pool, &batch->scoreboard, tsd, tiler);
      util_dynarray_append(&batch->jobs, void *, job.cpu);
      panvk_meta_blit_emit_fragment_job(cmdbuf, &fbinfo);
      batch->blit.src = blitinfo->src.planes[0].image->data.bo;
      batch->blit.dst = blitinfo->dst.planes[0].image->data.bo;
      panvk_meta_blit_close_batch(cmdbuf);
   } while (pan_blit_next_surface(&ctx));
}

void
panvk_CmdBlitImage(VkCommandBuffer commandBuffer,
                   VkImage srcImage,
                   VkImageLayout srcImageLayout,
                   VkImage destImage,
                   VkImageLayout destImageLayout,
                   uint32_t regionCount,
                   const VkImageBlit *pRegions,
                   VkFilter filter)

{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_image, src, srcImage);
   VK_FROM_HANDLE(panvk_image, dst, destImage);

   for (unsigned i = 0; i < regionCount; i++) {
      const VkImageBlit *region = &pRegions[i];
      struct pan_blit_info info = {
         .src = {
            .planes[0].image = &src->pimage,
            .planes[0].format = src->pimage.layout.format,
            .level = region->srcSubresource.mipLevel,
            .start = {
               region->srcOffsets[0].x,
               region->srcOffsets[0].y,
               region->srcOffsets[0].z,
               region->srcSubresource.baseArrayLayer,
            },
            .end = {
               region->srcOffsets[1].x,
               region->srcOffsets[1].y,
               region->srcOffsets[1].z,
               region->srcSubresource.baseArrayLayer + region->srcSubresource.layerCount - 1,
            },
         },
         .dst = {
            .planes[0].image = &dst->pimage,
            .planes[0].format = dst->pimage.layout.format,
            .level = region->dstSubresource.mipLevel,
            .start = {
               region->dstOffsets[0].x,
               region->dstOffsets[0].y,
               region->dstOffsets[0].z,
               region->dstSubresource.baseArrayLayer,
            },
            .end = {
               region->dstOffsets[1].x,
               region->dstOffsets[1].y,
               region->dstOffsets[1].z,
               region->dstSubresource.baseArrayLayer + region->dstSubresource.layerCount - 1,
            },
         },
         .nearest = filter == VK_FILTER_NEAREST,
      };

      if (region->srcSubresource.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT)
         info.src.planes[0].format = util_format_stencil_only(info.src.planes[0].format);
      else if (region->srcSubresource.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
         info.src.planes[0].format = util_format_get_depth_only(info.src.planes[0].format);

      if (region->dstSubresource.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT)
         info.dst.planes[0].format = util_format_stencil_only(info.dst.planes[0].format);
      else if (region->dstSubresource.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
         info.dst.planes[0].format = util_format_get_depth_only(info.dst.planes[0].format);

      panvk_meta_blit(cmdbuf, &info);
   }
}

static mali_ptr
panvk_meta_copy_img_emit_texture(struct panfrost_device *pdev,
                                 struct pan_pool *desc_pool,
                                 const struct pan_image_view *view)
{
   if (pan_is_bifrost(pdev)) {
      struct panfrost_ptr texture =
         panfrost_pool_alloc_desc(desc_pool, BIFROST_TEXTURE);
      size_t payload_size =
         panfrost_estimate_texture_payload_size(pdev, view);
      struct panfrost_ptr surfaces =
         panfrost_pool_alloc_aligned(desc_pool, payload_size,
                                     MALI_SURFACE_WITH_STRIDE_ALIGN);

      panfrost_new_texture(pdev, view, texture.cpu, &surfaces);

      return texture.gpu;
   } else {
      size_t sz = MALI_MIDGARD_TEXTURE_LENGTH +
                  panfrost_estimate_texture_payload_size(pdev, view);
      struct panfrost_ptr texture =
         panfrost_pool_alloc_aligned(desc_pool, sz, MALI_MIDGARD_TEXTURE_ALIGN);
      struct panfrost_ptr surfaces = {
         .cpu = texture.cpu + MALI_MIDGARD_TEXTURE_LENGTH,
         .gpu = texture.gpu + MALI_MIDGARD_TEXTURE_LENGTH,
      };

      panfrost_new_texture(pdev, view, texture.cpu, &surfaces);

      return panfrost_pool_upload_aligned(desc_pool, &texture.gpu,
                                          sizeof(mali_ptr),
                                          sizeof(mali_ptr));
   }
}

static mali_ptr
panvk_meta_copy_img_emit_sampler(struct panfrost_device *pdev,
                                 struct pan_pool *desc_pool)
{
   if (pan_is_bifrost(pdev)) {
      struct panfrost_ptr sampler =
         panfrost_pool_alloc_desc(desc_pool, BIFROST_SAMPLER);

      pan_pack(sampler.cpu, BIFROST_SAMPLER, cfg) {
         cfg.seamless_cube_map = false;
         cfg.normalized_coordinates = false;
         cfg.point_sample_minify = true;
         cfg.point_sample_magnify = true;
      }

      return sampler.gpu;
   } else {
      struct panfrost_ptr sampler =
         panfrost_pool_alloc_desc(desc_pool, MIDGARD_SAMPLER);

      pan_pack(sampler.cpu, MIDGARD_SAMPLER, cfg) {
         cfg.normalized_coordinates = false;
         cfg.magnify_nearest = true;
         cfg.minify_nearest = true;
      }

      return sampler.gpu;
   }
}

static mali_ptr
panvk_meta_copy_img2img_shader(struct panfrost_device *pdev,
                               struct pan_pool *bin_pool,
                               enum pipe_format srcfmt,
                               enum pipe_format dstfmt, unsigned dstmask,
                               unsigned texdim, unsigned texisarray,
                               struct pan_shader_info *shader_info)
{
   nir_builder b =
      nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,
                                     pan_shader_get_compiler_options(pdev),
                                     "panvk_meta_copy_img2img(srcfmt=%s,dstfmt=%s)",
                                     util_format_name(srcfmt), util_format_name(dstfmt));

   b.shader->info.internal = true;

   nir_variable *coord_var =
      nir_variable_create(b.shader, nir_var_shader_in,
                          glsl_vector_type(GLSL_TYPE_FLOAT, texdim + texisarray),
                          "coord");
   coord_var->data.location = VARYING_SLOT_TEX0;
   nir_ssa_def *coord = nir_f2u32(&b, nir_load_var(&b, coord_var));

   nir_tex_instr *tex = nir_tex_instr_create(b.shader, 1);
   tex->op = nir_texop_txf;
   tex->texture_index = 0;
   tex->is_array = texisarray;
   tex->dest_type = util_format_is_unorm(srcfmt) ?
                    nir_type_float32 : nir_type_uint32;

   switch (texdim) {
   case 1: tex->sampler_dim = GLSL_SAMPLER_DIM_1D; break;
   case 2: tex->sampler_dim = GLSL_SAMPLER_DIM_2D; break;
   case 3: tex->sampler_dim = GLSL_SAMPLER_DIM_3D; break;
   default: unreachable("Invalid texture dimension");
   }

   tex->src[0].src_type = nir_tex_src_coord;
   tex->src[0].src = nir_src_for_ssa(coord);
   tex->coord_components = texdim + texisarray;
   nir_ssa_dest_init(&tex->instr, &tex->dest, 4,
                     nir_alu_type_get_type_size(tex->dest_type), NULL);
   nir_builder_instr_insert(&b, &tex->instr);

   nir_ssa_def *texel = &tex->dest.ssa;

   unsigned dstcompsz =
      util_format_get_component_bits(dstfmt, UTIL_FORMAT_COLORSPACE_RGB, 0);
   unsigned ndstcomps = util_format_get_nr_components(dstfmt);
   const struct glsl_type *outtype = NULL;

   if (srcfmt == PIPE_FORMAT_R5G6B5_UNORM && dstfmt == PIPE_FORMAT_R8G8_UNORM) {
      nir_ssa_def *rgb =
         nir_f2u32(&b, nir_fmul(&b, texel,
                                nir_vec3(&b,
                                         nir_imm_float(&b, 31),
                                         nir_imm_float(&b, 63),
                                         nir_imm_float(&b, 31))));
      nir_ssa_def *rg =
         nir_vec2(&b,
                  nir_ior(&b, nir_channel(&b, rgb, 0),
                          nir_ishl(&b, nir_channel(&b, rgb, 1),
                                   nir_imm_int(&b, 5))),
                  nir_ior(&b,
                          nir_ushr_imm(&b, nir_channel(&b, rgb, 1), 3),
                          nir_ishl(&b, nir_channel(&b, rgb, 2),
                                   nir_imm_int(&b, 3))));
      rg = nir_iand_imm(&b, rg, 255);
      texel = nir_fmul_imm(&b, nir_u2f32(&b, rg), 1.0 / 255);
      outtype = glsl_vector_type(GLSL_TYPE_FLOAT, 2);
   } else if (srcfmt == PIPE_FORMAT_R8G8_UNORM && dstfmt == PIPE_FORMAT_R5G6B5_UNORM) {
      nir_ssa_def *rg = nir_f2u32(&b, nir_fmul_imm(&b, texel, 255));
      nir_ssa_def *rgb =
         nir_vec3(&b,
                  nir_channel(&b, rg, 0),
                  nir_ior(&b,
                          nir_ushr_imm(&b, nir_channel(&b, rg, 0), 5),
                          nir_ishl(&b, nir_channel(&b, rg, 1),
                                   nir_imm_int(&b, 3))),
                  nir_ushr_imm(&b, nir_channel(&b, rg, 1), 3));
      rgb = nir_iand(&b, rgb,
                     nir_vec3(&b,
                              nir_imm_int(&b, 31),
                              nir_imm_int(&b, 63),
                              nir_imm_int(&b, 31)));
      texel = nir_fmul(&b, nir_u2f32(&b, rgb),
                       nir_vec3(&b,
                                nir_imm_float(&b, 1.0 / 31),
                                nir_imm_float(&b, 1.0 / 63),
                                nir_imm_float(&b, 1.0 / 31)));
      outtype = glsl_vector_type(GLSL_TYPE_FLOAT, 3);
   } else {
      assert(srcfmt == dstfmt);
      enum glsl_base_type basetype;
      if (util_format_is_unorm(dstfmt)) {
         basetype = GLSL_TYPE_FLOAT;
      } else if (dstcompsz == 16) {
         basetype = GLSL_TYPE_UINT16;
      } else {
         assert(dstcompsz == 32);
         basetype = GLSL_TYPE_UINT;
      }

      if (dstcompsz == 16)
         texel = nir_u2u16(&b, texel);

      texel = nir_channels(&b, texel, (1 << ndstcomps) - 1);
      outtype = glsl_vector_type(basetype, ndstcomps);
   }

   nir_variable *out =
      nir_variable_create(b.shader, nir_var_shader_out, outtype, "out");
   out->data.location = FRAG_RESULT_DATA0;

   unsigned fullmask = (1 << ndstcomps) - 1;
   if (dstcompsz > 8 && dstmask != fullmask) {
      nir_ssa_def *oldtexel = nir_load_var(&b, out);
      nir_ssa_def *dstcomps[4];

      for (unsigned i = 0; i < ndstcomps; i++) {
         if (dstmask & BITFIELD_BIT(i))
            dstcomps[i] = nir_channel(&b, texel, i);
         else
            dstcomps[i] = nir_channel(&b, oldtexel, i);
      }

      texel = nir_vec(&b, dstcomps, ndstcomps);
   }

   nir_store_var(&b, out, texel, 0xff);

   uint32_t rt_conv;
   pan_pack(&rt_conv, BIFROST_INTERNAL_CONVERSION, cfg) {
      cfg.memory_format = (dstcompsz == 2 ? MALI_RG16UI : MALI_RG32UI) << 12;
      cfg.raw = true;
      cfg.register_format = dstcompsz == 2 ?
                            MALI_BIFROST_REGISTER_FILE_FORMAT_U16 :
                            MALI_BIFROST_REGISTER_FILE_FORMAT_U32;
   }

   struct panfrost_compile_inputs inputs = {
      .gpu_id = pdev->gpu_id,
      .is_blit = true,
      .bifrost.static_rt_conv = true,
      .bifrost.rt_conv[0] = rt_conv,
   };

   struct util_dynarray binary;

   util_dynarray_init(&binary, NULL);
   pan_shader_compile(pdev, b.shader, &inputs, &binary, shader_info);

   mali_ptr shader =
      panfrost_pool_upload_aligned(bin_pool, binary.data, binary.size,
                                   pan_is_bifrost(pdev) ? 128 : 64);

   util_dynarray_fini(&binary);
   ralloc_free(b.shader);

   return shader;
}

static uint32_t
panvk_meta_copy_img_bifrost_raw_format(unsigned texelsize)
{
   switch (texelsize) {
   case 6: return MALI_RGB16UI << 12;
   case 8: return MALI_RG32UI << 12;
   case 12: return MALI_RGB32UI << 12;
   case 16: return MALI_RGBA32UI << 12;
   default: unreachable("Invalid texel size\n");
   }
}

static mali_ptr
panvk_meta_copy_img2img_emit_rsd(struct panfrost_device *pdev,
                                 struct pan_pool *bin_pool,
                                 struct pan_pool *desc_pool,
                                 enum pipe_format srcfmt,
                                 enum pipe_format dstfmt,
                                 unsigned dstmask,
                                 unsigned texdim, bool texisarray)
{
   bool raw = util_format_get_blocksize(dstfmt) > 4;
   struct pan_shader_info shader_info;
   mali_ptr shader =
      panvk_meta_copy_img2img_shader(pdev, bin_pool, srcfmt, dstfmt,
                                     dstmask,
                                     texdim, texisarray,
                                     &shader_info);

   struct panfrost_ptr rsd_ptr =
      panfrost_pool_alloc_desc_aggregate(desc_pool,
                                         PAN_DESC(RENDERER_STATE),
                                         PAN_DESC_ARRAY(1, BLEND));
   unsigned fullmask = (1 << util_format_get_nr_components(dstfmt)) - 1;
   bool partialwrite = fullmask != dstmask && !raw;
   bool readstb = fullmask != dstmask && raw;

   pan_pack(rsd_ptr.cpu, RENDERER_STATE, cfg) {
      pan_shader_prepare_rsd(pdev, &shader_info, shader, &cfg);
      cfg.shader.varying_count = 1;
      cfg.shader.texture_count = 1;
      cfg.shader.sampler_count = 1;
      cfg.properties.depth_source = MALI_DEPTH_SOURCE_FIXED_FUNCTION;
      cfg.multisample_misc.sample_mask = UINT16_MAX;
      cfg.multisample_misc.depth_function = MALI_FUNC_ALWAYS;
      cfg.stencil_mask_misc.stencil_mask_front = 0xFF;
      cfg.stencil_mask_misc.stencil_mask_back = 0xFF;
      cfg.stencil_front.compare_function = MALI_FUNC_ALWAYS;
      cfg.stencil_front.stencil_fail = MALI_STENCIL_OP_REPLACE;
      cfg.stencil_front.depth_fail = MALI_STENCIL_OP_REPLACE;
      cfg.stencil_front.depth_pass = MALI_STENCIL_OP_REPLACE;
      cfg.stencil_front.mask = 0xFF;
      cfg.stencil_back = cfg.stencil_front;

      if (pan_is_bifrost(pdev)) {
         cfg.properties.bifrost.allow_forward_pixel_to_be_killed = true;
         cfg.properties.bifrost.allow_forward_pixel_to_kill =
            !partialwrite && !readstb;
         cfg.properties.bifrost.zs_update_operation =
            MALI_PIXEL_KILL_STRONG_EARLY;
         cfg.properties.bifrost.pixel_kill_operation =
            MALI_PIXEL_KILL_FORCE_EARLY;
      } else {
         cfg.properties.midgard.shader_reads_tilebuffer = readstb;
         cfg.properties.midgard.work_register_count = shader_info.work_reg_count;
         cfg.properties.midgard.force_early_z = true;
         cfg.stencil_mask_misc.alpha_test_compare_function = MALI_FUNC_ALWAYS;
      }
   }

   pan_pack(rsd_ptr.cpu + MALI_RENDERER_STATE_LENGTH, BLEND, cfg) {
      cfg.round_to_fb_precision = true;
      cfg.load_destination = partialwrite;
      if (pan_is_bifrost(pdev)) {
         cfg.bifrost.internal.mode =
            partialwrite ?
            MALI_BIFROST_BLEND_MODE_FIXED_FUNCTION :
            MALI_BIFROST_BLEND_MODE_OPAQUE;
         cfg.bifrost.equation.rgb.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.bifrost.equation.rgb.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.bifrost.equation.rgb.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.bifrost.equation.alpha.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.bifrost.equation.alpha.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.bifrost.equation.alpha.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.bifrost.internal.fixed_function.num_comps = 4;
         cfg.bifrost.equation.color_mask = partialwrite ? dstmask : 0xf;
         if (!raw) {
            cfg.bifrost.internal.fixed_function.conversion.memory_format =
               panfrost_format_to_bifrost_blend(pdev, dstfmt);
            cfg.bifrost.internal.fixed_function.conversion.register_format =
               MALI_BIFROST_REGISTER_FILE_FORMAT_F32;
         } else {
            unsigned imgtexelsz = util_format_get_blocksize(dstfmt);

            cfg.bifrost.internal.fixed_function.conversion.memory_format =
               panvk_meta_copy_img_bifrost_raw_format(imgtexelsz);
            cfg.bifrost.internal.fixed_function.conversion.raw = true;
            cfg.bifrost.internal.fixed_function.conversion.register_format =
               (imgtexelsz & 2) ?
               MALI_BIFROST_REGISTER_FILE_FORMAT_U16 :
               MALI_BIFROST_REGISTER_FILE_FORMAT_U32;
         }
      } else {
         cfg.midgard.equation.rgb.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.midgard.equation.rgb.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.midgard.equation.rgb.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.midgard.equation.alpha.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.midgard.equation.alpha.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.midgard.equation.alpha.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.midgard.equation.color_mask = dstmask;
      }
   }

   return rsd_ptr.gpu;
}

static enum pipe_format
panvk_meta_copy_img_format(enum pipe_format fmt)
{
   /* We can't use a non-compressed format when handling a tiled/AFBC
    * compressed format because the tile size differ (4x4 blocks for
    * compressed formats and 16x16 texels for non-compressed ones).
    */
   assert(!util_format_is_compressed(fmt));

   /* Pick blendable formats when we can, otherwise pick the UINT variant
    * matching the texel size.
    */
   switch (util_format_get_blocksize(fmt)) {
   case 16: return PIPE_FORMAT_R32G32B32A32_UINT;
   case 12: return PIPE_FORMAT_R32G32B32_UINT;
   case 8: return PIPE_FORMAT_R32G32_UINT;
   case 6: return PIPE_FORMAT_R16G16B16_UINT;
   case 4: return PIPE_FORMAT_R8G8B8A8_UNORM;
   case 2: return (fmt == PIPE_FORMAT_R5G6B5_UNORM ||
                   fmt == PIPE_FORMAT_B5G6R5_UNORM) ?
                  PIPE_FORMAT_R5G6B5_UNORM : PIPE_FORMAT_R8G8_UNORM;
   case 1: return PIPE_FORMAT_R8_UNORM;
   default: unreachable("Unsupported format\n");
   }
}

struct panvk_meta_copy_img2img_format_info {
   enum pipe_format srcfmt;
   enum pipe_format dstfmt;
   unsigned dstmask;
};

static const struct panvk_meta_copy_img2img_format_info panvk_meta_copy_img2img_fmts[] = {
   { PIPE_FORMAT_R8_UNORM, PIPE_FORMAT_R8_UNORM, 0x1},
   { PIPE_FORMAT_R5G6B5_UNORM, PIPE_FORMAT_R5G6B5_UNORM, 0x7},
   { PIPE_FORMAT_R5G6B5_UNORM, PIPE_FORMAT_R8G8_UNORM, 0x3},
   { PIPE_FORMAT_R8G8_UNORM, PIPE_FORMAT_R5G6B5_UNORM, 0x7},
   { PIPE_FORMAT_R8G8_UNORM, PIPE_FORMAT_R8G8_UNORM, 0x3},
   /* Z24S8(depth) */
   { PIPE_FORMAT_R8G8B8A8_UNORM, PIPE_FORMAT_R8G8B8A8_UNORM, 0x7 },
   /* Z24S8(stencil) */
   { PIPE_FORMAT_R8G8B8A8_UNORM, PIPE_FORMAT_R8G8B8A8_UNORM, 0x8 },
   { PIPE_FORMAT_R8G8B8A8_UNORM, PIPE_FORMAT_R8G8B8A8_UNORM, 0xf },
   { PIPE_FORMAT_R16G16B16_UINT, PIPE_FORMAT_R16G16B16_UINT, 0x7 },
   { PIPE_FORMAT_R32G32_UINT, PIPE_FORMAT_R32G32_UINT, 0x3 },
   /* Z32S8X24(depth) */
   { PIPE_FORMAT_R32G32_UINT, PIPE_FORMAT_R32G32_UINT, 0x1 },
   /* Z32S8X24(stencil) */
   { PIPE_FORMAT_R32G32_UINT, PIPE_FORMAT_R32G32_UINT, 0x2 },
   { PIPE_FORMAT_R32G32B32_UINT, PIPE_FORMAT_R32G32B32_UINT, 0x7 },
   { PIPE_FORMAT_R32G32B32A32_UINT, PIPE_FORMAT_R32G32B32A32_UINT, 0xf },
};

static unsigned
panvk_meta_copy_img2img_format_idx(struct panvk_meta_copy_img2img_format_info key)
{
   STATIC_ASSERT(ARRAY_SIZE(panvk_meta_copy_img2img_fmts) == PANVK_META_COPY_IMG2IMG_NUM_FORMATS);

   for (unsigned i = 0; i < ARRAY_SIZE(panvk_meta_copy_img2img_fmts); i++) {
      if (!memcmp(&key, &panvk_meta_copy_img2img_fmts[i], sizeof(key)))
         return i;
   }

   unreachable("Invalid image format\n");
}

static void
panvk_meta_copy_emit_varying(struct pan_pool *pool,
                             mali_ptr coordinates,
                             mali_ptr *varying_bufs,
                             mali_ptr *varyings)
{
   /* Bifrost needs an empty desc to mark end of prefetching */
   bool padding_buffer = pan_is_bifrost(pool->dev);

   struct panfrost_ptr varying =
      panfrost_pool_alloc_desc(pool, ATTRIBUTE);
   struct panfrost_ptr varying_buffer =
      panfrost_pool_alloc_desc_array(pool, (padding_buffer ? 2 : 1),
                                     ATTRIBUTE_BUFFER);

   pan_pack(varying_buffer.cpu, ATTRIBUTE_BUFFER, cfg) {
      cfg.pointer = coordinates;
      cfg.stride = 4 * sizeof(uint32_t);
      cfg.size = cfg.stride * 4;
   }

   if (padding_buffer) {
      pan_pack(varying_buffer.cpu + MALI_ATTRIBUTE_BUFFER_LENGTH,
               ATTRIBUTE_BUFFER, cfg);
   }

   pan_pack(varying.cpu, ATTRIBUTE, cfg) {
      cfg.buffer_index = 0;
      cfg.offset_enable = !pan_is_bifrost(pool->dev);
      cfg.format = pool->dev->formats[PIPE_FORMAT_R32G32B32_FLOAT].hw;
   }

   *varyings = varying.gpu;
   *varying_bufs = varying_buffer.gpu;
}

static void
panvk_meta_copy_img2img_emit_dcd(struct pan_pool *pool,
                                 mali_ptr src_coords, mali_ptr dst_coords,
                                 mali_ptr texture, mali_ptr sampler,
                                 mali_ptr vpd, mali_ptr tsd, mali_ptr rsd,
                                 void *out)
{
   pan_pack(out, DRAW, cfg) {
      cfg.four_components_per_vertex = true;
      cfg.draw_descriptor_is_64b = true;
      cfg.thread_storage = tsd;
      cfg.state = rsd;
      cfg.position = dst_coords;
      panvk_meta_copy_emit_varying(pool, src_coords,
                                   &cfg.varying_buffers,
                                   &cfg.varyings);
      cfg.viewport = vpd;
      cfg.texture_descriptor_is_64b = !pan_is_bifrost(pool->dev);
      cfg.textures = texture;
      cfg.samplers = sampler;
   }
}

static struct panfrost_ptr
panvk_meta_copy_img2img_emit_midgard_tiler_job(struct pan_pool *desc_pool,
                                               struct pan_scoreboard *scoreboard,
                                               mali_ptr src_coords, mali_ptr dst_coords,
                                               mali_ptr texture, mali_ptr sampler,
                                               mali_ptr vpd, mali_ptr rsd, mali_ptr tsd)
{
   struct panfrost_ptr job =
      panfrost_pool_alloc_desc(desc_pool, MIDGARD_TILER_JOB);

   panvk_meta_copy_img2img_emit_dcd(desc_pool,
                                    src_coords, dst_coords,
                                    texture, sampler,
                                    vpd, tsd, rsd,
                                    pan_section_ptr(job.cpu, MIDGARD_TILER_JOB, DRAW));

   pan_section_pack(job.cpu, MIDGARD_TILER_JOB, PRIMITIVE, cfg) {
      cfg.draw_mode = MALI_DRAW_MODE_TRIANGLE_STRIP;
      cfg.index_count = 4;
      cfg.job_task_split = 6;
   }

   pan_section_pack(job.cpu, MIDGARD_TILER_JOB, PRIMITIVE_SIZE, cfg) {
      cfg.constant = 1.0f;
   }

   void *invoc = pan_section_ptr(job.cpu,
                                 MIDGARD_TILER_JOB,
                                 INVOCATION);
   panfrost_pack_work_groups_compute(invoc, 1, 4,
                                     1, 1, 1, 1, true);

   panfrost_add_job(desc_pool, scoreboard, MALI_JOB_TYPE_TILER,
                    false, false, 0, 0, &job, false);
   return job;
}

static struct panfrost_ptr
panvk_meta_copy_img2img_emit_bifrost_tiler_job(struct pan_pool *desc_pool,
                                               struct pan_scoreboard *scoreboard,
                                               mali_ptr src_coords, mali_ptr dst_coords,
                                               mali_ptr texture, mali_ptr sampler,
                                               mali_ptr vpd, mali_ptr rsd,
                                               mali_ptr tsd, mali_ptr tiler)
{
   struct panfrost_ptr job =
      panfrost_pool_alloc_desc(desc_pool, BIFROST_TILER_JOB);

   panvk_meta_copy_img2img_emit_dcd(desc_pool,
                                    src_coords, dst_coords,
                                    texture, sampler,
                                    vpd, tsd, rsd,
                                    pan_section_ptr(job.cpu, BIFROST_TILER_JOB, DRAW));

   pan_section_pack(job.cpu, BIFROST_TILER_JOB, PRIMITIVE, cfg) {
      cfg.draw_mode = MALI_DRAW_MODE_TRIANGLE_STRIP;
      cfg.index_count = 4;
      cfg.job_task_split = 6;
   }

   pan_section_pack(job.cpu, BIFROST_TILER_JOB, PRIMITIVE_SIZE, cfg) {
      cfg.constant = 1.0f;
   }

   void *invoc = pan_section_ptr(job.cpu,
                                 BIFROST_TILER_JOB,
                                 INVOCATION);
   panfrost_pack_work_groups_compute(invoc, 1, 4,
                                     1, 1, 1, 1, true);

   pan_section_pack(job.cpu, BIFROST_TILER_JOB, PADDING, cfg);
   pan_section_pack(job.cpu, BIFROST_TILER_JOB, TILER, cfg) {
      cfg.address = tiler;
   }

   panfrost_add_job(desc_pool, scoreboard, MALI_JOB_TYPE_TILER,
                    false, false, 0, 0, &job, false);
   return job;
}

static mali_ptr
panvk_meta_copy_img_emit_viewport(struct pan_pool *pool,
                                  uint16_t minx, uint16_t miny,
                                  uint16_t maxx, uint16_t maxy)
{
   struct panfrost_ptr vp = panfrost_pool_alloc_desc(pool, VIEWPORT);

   pan_pack(vp.cpu, VIEWPORT, cfg) {
      cfg.scissor_minimum_x = minx;
      cfg.scissor_minimum_y = miny;
      cfg.scissor_maximum_x = maxx;
      cfg.scissor_maximum_y = maxy;
   }

   return vp.gpu;
}

static unsigned
panvk_meta_copy_img_mask(enum pipe_format imgfmt, VkImageAspectFlags aspectMask)
{
   if (aspectMask != VK_IMAGE_ASPECT_DEPTH_BIT &&
       aspectMask != VK_IMAGE_ASPECT_STENCIL_BIT) {
      enum pipe_format outfmt = panvk_meta_copy_img_format(imgfmt);

      return (1 << util_format_get_nr_components(outfmt)) - 1;
   }

   switch (imgfmt) {
   case PIPE_FORMAT_S8_UINT:
      return 1;
   case PIPE_FORMAT_Z16_UNORM:
      return 3;
   case PIPE_FORMAT_Z16_UNORM_S8_UINT:
      return aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT ? 3 : 8;
   case PIPE_FORMAT_Z24_UNORM_S8_UINT:
      return aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT ? 7 : 8;
   case PIPE_FORMAT_Z24X8_UNORM:
      assert(aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT);
      return 7;
   case PIPE_FORMAT_Z32_FLOAT:
      return 0xf;
   case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
      return aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT ? 1 : 2;
   default:
      unreachable("Invalid depth format\n");
   }
}

static void
panvk_meta_copy_img2img(struct panvk_cmd_buffer *cmdbuf,
                        const struct panvk_image *src,
                        const struct panvk_image *dst,
                        const VkImageCopy *region)
{
   struct panfrost_device *pdev = &cmdbuf->device->physical_device->pdev;
   struct panvk_meta_copy_img2img_format_info key = {
      .srcfmt = panvk_meta_copy_img_format(src->pimage.layout.format),
      .dstfmt = panvk_meta_copy_img_format(dst->pimage.layout.format),
      .dstmask = panvk_meta_copy_img_mask(dst->pimage.layout.format,
                                          region->dstSubresource.aspectMask),
   };

   unsigned texdimidx =
      panvk_meta_copy_tex_type(src->pimage.layout.dim,
                               src->pimage.layout.array_size > 1);
   unsigned fmtidx =
      panvk_meta_copy_img2img_format_idx(key);

   mali_ptr rsd =
      cmdbuf->device->physical_device->meta.copy.img2img[texdimidx][fmtidx].rsd;

   struct pan_image_view srcview = {
      .format = key.srcfmt,
      .dim = src->pimage.layout.dim == MALI_TEXTURE_DIMENSION_CUBE ?
             MALI_TEXTURE_DIMENSION_2D : src->pimage.layout.dim,
      .image = &src->pimage,
      .nr_samples = 1,
      .first_level = region->srcSubresource.mipLevel,
      .last_level = region->srcSubresource.mipLevel,
      .first_layer = region->srcSubresource.baseArrayLayer,
      .last_layer = region->srcSubresource.baseArrayLayer + region->srcSubresource.layerCount - 1,
      .swizzle = { PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y, PIPE_SWIZZLE_Z, PIPE_SWIZZLE_W },
   };

   /* TODO: don't force preloads of dst resources if unneeded */
   struct pan_image_state dststate = { 0 };
   dststate.slices[region->dstSubresource.mipLevel].data_valid = true;

   struct pan_image_view dstview = {
      .format = key.dstfmt,
      .dim = MALI_TEXTURE_DIMENSION_2D,
      .image = &dst->pimage,
      .nr_samples = 1,
      .first_level = region->dstSubresource.mipLevel,
      .last_level = region->dstSubresource.mipLevel,
      .swizzle = { PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y, PIPE_SWIZZLE_Z, PIPE_SWIZZLE_W },
   };

   unsigned minx = MAX2(region->dstOffset.x, 0);
   unsigned miny = MAX2(region->dstOffset.y, 0);
   unsigned maxx = MAX2(region->dstOffset.x + region->extent.width - 1, 0);
   unsigned maxy = MAX2(region->dstOffset.y + region->extent.height - 1, 0);

   mali_ptr vpd =
      panvk_meta_copy_img_emit_viewport(&cmdbuf->desc_pool,
                                        minx, miny, maxx, maxy);

   float dst_rect[] = {
      minx, miny, 0.0, 1.0,
      maxx + 1, miny, 0.0, 1.0,
      minx, maxy + 1, 0.0, 1.0,
      maxx + 1, maxy + 1, 0.0, 1.0,
   };

   mali_ptr dst_coords =
      panfrost_pool_upload_aligned(&cmdbuf->desc_pool, dst_rect,
                                   sizeof(dst_rect), 64);

   struct pan_fb_info fbinfo = {
      .width = u_minify(dst->pimage.layout.width, region->dstSubresource.mipLevel),
      .height = u_minify(dst->pimage.layout.height, region->dstSubresource.mipLevel),
      .extent.minx = minx,
      .extent.maxx = maxx,
      .extent.miny = miny,
      .extent.maxy = maxy,
      .nr_samples = 1,
      .rt_count = 1,
      .rts[0].view = &dstview,
      .rts[0].state = &dststate,
      .rts[0].preload = true,
   };
   struct pan_tls_info tlsinfo = { 0 };

   mali_ptr texture =
      panvk_meta_copy_img_emit_texture(pdev, &cmdbuf->desc_pool, &srcview);
   mali_ptr sampler =
      panvk_meta_copy_img_emit_sampler(pdev, &cmdbuf->desc_pool);

   if (cmdbuf->state.batch)
      panvk_cmd_close_batch(cmdbuf);

   minx = MAX2(region->srcOffset.x, 0);
   miny = MAX2(region->srcOffset.y, 0);
   maxx = MAX2(region->srcOffset.x + region->extent.width - 1, 0);
   maxy = MAX2(region->srcOffset.y + region->extent.height - 1, 0);
   assert(region->dstOffset.z >= 0);

   unsigned first_src_layer = MAX2(region->srcSubresource.baseArrayLayer, region->srcOffset.z);
   unsigned first_dst_layer = MAX2(region->dstSubresource.baseArrayLayer, region->dstOffset.z);
   unsigned nlayers = MAX2(region->dstSubresource.layerCount, region->extent.depth);
   for (unsigned l = 0; l < nlayers; l++) {
      unsigned src_l = l + first_src_layer;
      float src_rect[] = {
         minx, miny, src_l, 1.0,
         maxx + 1, miny, src_l, 1.0,
         minx, maxy + 1, src_l, 1.0,
         maxx + 1, maxy + 1, src_l, 1.0,
      };

      mali_ptr src_coords =
         panfrost_pool_upload_aligned(&cmdbuf->desc_pool, src_rect,
                                      sizeof(src_rect), 64);

      panvk_cmd_open_batch(cmdbuf);

      struct panvk_batch *batch = cmdbuf->state.batch;

      dstview.first_layer = dstview.last_layer = l + first_dst_layer;
      panvk_meta_blit_emit_tls(cmdbuf, &tlsinfo);
      panvk_meta_blit_emit_fb(cmdbuf, &fbinfo, &tlsinfo);

      mali_ptr tsd, tiler;

      if (pan_is_bifrost(pdev)) {
         tsd = batch->tls.gpu;
         tiler = batch->tiler.bifrost_descs.gpu;
      } else {
         tsd = batch->fb.desc.gpu;
         tiler = 0;
      }

      struct panfrost_ptr job;

      if (pan_is_bifrost(pdev)) {
         job = panvk_meta_copy_img2img_emit_bifrost_tiler_job(&cmdbuf->desc_pool,
                                                              &batch->scoreboard,
                                                              src_coords, dst_coords,
                                                              texture, sampler,
                                                              vpd, rsd, tsd, tiler);
      } else {
         job = panvk_meta_copy_img2img_emit_midgard_tiler_job(&cmdbuf->desc_pool,
                                                              &batch->scoreboard,
                                                              src_coords, dst_coords,
                                                              texture, sampler,
                                                              vpd, rsd, tsd);
      }

      util_dynarray_append(&batch->jobs, void *, job.cpu);
      panvk_meta_blit_emit_fragment_job(cmdbuf, &fbinfo);
      batch->blit.src = src->pimage.data.bo;
      batch->blit.dst = dst->pimage.data.bo;
      panvk_meta_blit_close_batch(cmdbuf);
   }
}

static void
panvk_meta_copy_img2img_init(struct panvk_physical_device *dev)
{
   STATIC_ASSERT(ARRAY_SIZE(panvk_meta_copy_img2img_fmts) == PANVK_META_COPY_IMG2IMG_NUM_FORMATS);

   for (unsigned i = 0; i < ARRAY_SIZE(panvk_meta_copy_img2img_fmts); i++) {
      for (unsigned texdim = 1; texdim <= 3; texdim++) {
         unsigned texdimidx = panvk_meta_copy_tex_type(texdim, false);
         assert(texdimidx < ARRAY_SIZE(dev->meta.copy.img2img));

         dev->meta.copy.img2img[texdimidx][i].rsd =
            panvk_meta_copy_img2img_emit_rsd(&dev->pdev, &dev->meta.bin_pool,
                                             &dev->meta.desc_pool,
                                             panvk_meta_copy_img2img_fmts[i].srcfmt,
                                             panvk_meta_copy_img2img_fmts[i].dstfmt,
                                             panvk_meta_copy_img2img_fmts[i].dstmask,
                                             texdim, false);
         if (texdim == 3)
            continue;

         texdimidx = panvk_meta_copy_tex_type(texdim, true);
         assert(texdimidx < ARRAY_SIZE(dev->meta.copy.img2img));
         dev->meta.copy.img2img[texdimidx][i].rsd =
            panvk_meta_copy_img2img_emit_rsd(&dev->pdev, &dev->meta.bin_pool,
                                             &dev->meta.desc_pool,
                                             panvk_meta_copy_img2img_fmts[i].srcfmt,
                                             panvk_meta_copy_img2img_fmts[i].dstfmt,
                                             panvk_meta_copy_img2img_fmts[i].dstmask,
                                             texdim, true);
      }
   }
}

void
panvk_CmdCopyImage(VkCommandBuffer commandBuffer,
                   VkImage srcImage,
                   VkImageLayout srcImageLayout,
                   VkImage destImage,
                   VkImageLayout destImageLayout,
                   uint32_t regionCount,
                   const VkImageCopy *pRegions)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_image, dst, destImage);
   VK_FROM_HANDLE(panvk_image, src, srcImage);

   for (unsigned i = 0; i < regionCount; i++) {
      panvk_meta_copy_img2img(cmdbuf, src, dst, &pRegions[i]);
   }
}

static unsigned
panvk_meta_copy_buf_texelsize(enum pipe_format imgfmt, unsigned mask)
{
   unsigned imgtexelsz = util_format_get_blocksize(imgfmt);
   unsigned nbufcomps = util_bitcount(mask);

   if (nbufcomps == util_format_get_nr_components(imgfmt))
      return imgtexelsz;

   /* Special case for Z24 buffers which are not tightly packed */
   if (mask == 7 && imgtexelsz == 4)
      return 4;

   /* Special case for S8 extraction from Z32_S8X24 */
   if (mask == 2 && imgtexelsz == 8)
      return 1;

   unsigned compsz =
      util_format_get_component_bits(imgfmt, UTIL_FORMAT_COLORSPACE_RGB, 0);

   assert(!(compsz % 8));

   return nbufcomps * compsz / 8;
}

static enum pipe_format
panvk_meta_copy_buf2img_format(enum pipe_format imgfmt)
{
   /* Pick blendable formats when we can, and the FLOAT variant matching the
    * texelsize otherwise.
    */
   switch (util_format_get_blocksize(imgfmt)) {
   case 1: return PIPE_FORMAT_R8_UNORM;
   /* AFBC stores things differently for RGB565,
    * we can't simply map to R8G8 in that case */
   case 2: return (imgfmt == PIPE_FORMAT_R5G6B5_UNORM ||
                   imgfmt == PIPE_FORMAT_B5G6R5_UNORM) ?
                  PIPE_FORMAT_R5G6B5_UNORM : PIPE_FORMAT_R8G8_UNORM;
   case 4: return PIPE_FORMAT_R8G8B8A8_UNORM;
   case 6: return PIPE_FORMAT_R16G16B16_UINT;
   case 8: return PIPE_FORMAT_R32G32_UINT;
   case 12: return PIPE_FORMAT_R32G32B32_UINT;
   case 16: return PIPE_FORMAT_R32G32B32A32_UINT;
   default: unreachable("Invalid format\n");
   }
}

struct panvk_meta_copy_format_info {
   enum pipe_format imgfmt;
   unsigned mask;
};

static const struct panvk_meta_copy_format_info panvk_meta_copy_buf2img_fmts[] = {
   { PIPE_FORMAT_R8_UNORM, 0x1 },
   { PIPE_FORMAT_R8G8_UNORM, 0x3 },
   { PIPE_FORMAT_R5G6B5_UNORM, 0x7 },
   { PIPE_FORMAT_R8G8B8A8_UNORM, 0xf },
   { PIPE_FORMAT_R16G16B16_UINT, 0x7 },
   { PIPE_FORMAT_R32G32_UINT, 0x3 },
   { PIPE_FORMAT_R32G32B32_UINT, 0x7 },
   { PIPE_FORMAT_R32G32B32A32_UINT, 0xf },
   /* S8 -> Z24S8 */
   { PIPE_FORMAT_R8G8B8A8_UNORM, 0x8 },
   /* S8 -> Z32_S8X24 */
   { PIPE_FORMAT_R32G32_UINT, 0x2 },
   /* Z24X8 -> Z24S8 */
   { PIPE_FORMAT_R8G8B8A8_UNORM, 0x7 },
   /* Z32 -> Z32_S8X24 */
   { PIPE_FORMAT_R32G32_UINT, 0x1 },
};

struct panvk_meta_copy_buf2img_info {
   struct {
      mali_ptr ptr;
      struct {
         unsigned line;
         unsigned surf;
      } stride;
   } buf;
};

#define panvk_meta_copy_buf2img_get_info_field(b, field) \
        nir_load_ubo((b), 1, \
                     sizeof(((struct panvk_meta_copy_buf2img_info *)0)->field) * 8, \
                     nir_imm_int(b, 0), \
                     nir_imm_int(b, offsetof(struct panvk_meta_copy_buf2img_info, field)), \
                     .align_mul = 4, \
                     .align_offset = 0, \
                     .range_base = 0, \
                     .range = ~0)

static mali_ptr
panvk_meta_copy_buf2img_shader(struct panfrost_device *pdev,
                               struct pan_pool *bin_pool,
                               struct panvk_meta_copy_format_info key,
                               struct pan_shader_info *shader_info)
{
   /* FIXME: Won't work on compute queues, but we can't do that with
    * a compute shader if the destination is an AFBC surface.
    */
   nir_builder b =
      nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,
                                     pan_shader_get_compiler_options(pdev),
                                     "panvk_meta_copy_buf2img(imgfmt=%s,mask=%x)",
                                     util_format_name(key.imgfmt),
                                     key.mask);

   b.shader->info.internal = true;
   b.shader->info.num_ubos = 1;

   nir_variable *coord_var =
      nir_variable_create(b.shader, nir_var_shader_in,
                          glsl_vector_type(GLSL_TYPE_FLOAT, 3),
                          "coord");
   coord_var->data.location = VARYING_SLOT_TEX0;
   nir_ssa_def *coord = nir_load_var(&b, coord_var);

   coord = nir_f2u32(&b, coord);

   nir_ssa_def *bufptr =
      panvk_meta_copy_buf2img_get_info_field(&b, buf.ptr);
   nir_ssa_def *buflinestride =
      panvk_meta_copy_buf2img_get_info_field(&b, buf.stride.line);
   nir_ssa_def *bufsurfstride =
      panvk_meta_copy_buf2img_get_info_field(&b, buf.stride.surf);

   unsigned imgtexelsz = util_format_get_blocksize(key.imgfmt);
   unsigned buftexelsz = panvk_meta_copy_buf_texelsize(key.imgfmt, key.mask);
   unsigned writemask = key.mask;

   /* FIXME: doesn't work for tiled+compressed formats since blocks are 4x4
    * blocks instead of 16x16 texels in that case, and there's nothing we can
    * do to force the tile size to 4x4 in the render path.
    * This being said, compressed textures are not compatible with AFBC, so we
    * could use a compute shader arranging the blocks properly.
    */
   nir_ssa_def *offset =
      nir_imul(&b, nir_channel(&b, coord, 0), nir_imm_int(&b, buftexelsz));
   offset = nir_iadd(&b, offset,
                     nir_imul(&b, nir_channel(&b, coord, 1), buflinestride));
   offset = nir_iadd(&b, offset,
                     nir_imul(&b, nir_channel(&b, coord, 2), bufsurfstride));
   bufptr = nir_iadd(&b, bufptr, nir_u2u64(&b, offset));

   unsigned imgcompsz =
      (imgtexelsz <= 4 && key.imgfmt != PIPE_FORMAT_R5G6B5_UNORM) ?
      1 : MIN2(1 << (ffs(imgtexelsz) - 1), 4);

   unsigned nimgcomps = imgtexelsz / imgcompsz;
   unsigned bufcompsz = MIN2(buftexelsz, imgcompsz);
   unsigned nbufcomps = buftexelsz / bufcompsz;

   assert(bufcompsz == 1 || bufcompsz == 2 || bufcompsz == 4);
   assert(nbufcomps <= 4 && nimgcomps <= 4);

   nir_ssa_def *texel =
      nir_load_global(&b, bufptr, bufcompsz, nbufcomps, bufcompsz * 8);

   enum glsl_base_type basetype;
   if (key.imgfmt == PIPE_FORMAT_R5G6B5_UNORM) {
      texel = nir_vec3(&b,
                       nir_iand_imm(&b, texel, BITFIELD_MASK(5)),
                       nir_iand_imm(&b, nir_ushr_imm(&b, texel, 5), BITFIELD_MASK(6)),
                       nir_iand_imm(&b, nir_ushr_imm(&b, texel, 11), BITFIELD_MASK(5)));
      texel = nir_fmul(&b,
                       nir_u2f32(&b, texel),
                       nir_vec3(&b,
                                nir_imm_float(&b, 1.0f / 31),
                                nir_imm_float(&b, 1.0f / 63),
                                nir_imm_float(&b, 1.0f / 31)));
      nimgcomps = 3;
      basetype = GLSL_TYPE_FLOAT;
   } else if (imgcompsz == 1) {
      assert(bufcompsz == 1);
      /* Blendable formats are unorm and the fixed-function blend unit
       * takes float values.
       */
      texel = nir_fmul(&b, nir_u2f32(&b, texel),
                       nir_imm_float(&b, 1.0f / 255));
      basetype = GLSL_TYPE_FLOAT;
   } else {
      texel = nir_u2uN(&b, texel, imgcompsz * 8);
      basetype = imgcompsz == 2 ? GLSL_TYPE_UINT16 : GLSL_TYPE_UINT;
   }

   /* We always pass the texel using 32-bit regs for now */
   nir_variable *out =
      nir_variable_create(b.shader, nir_var_shader_out,
                          glsl_vector_type(basetype, nimgcomps),
                          "out");
   out->data.location = FRAG_RESULT_DATA0;

   uint16_t fullmask = (1 << nimgcomps) - 1;

   assert(fullmask >= writemask);

   if (fullmask != writemask) {
      unsigned first_written_comp = ffs(writemask) - 1;
      nir_ssa_def *oldtexel = NULL;
      if (imgcompsz > 1)
         oldtexel = nir_load_var(&b, out);

      nir_ssa_def *texel_comps[4];
      for (unsigned i = 0; i < nimgcomps; i++) {
         if (writemask & BITFIELD_BIT(i))
            texel_comps[i] = nir_channel(&b, texel, i - first_written_comp);
         else if (imgcompsz > 1)
            texel_comps[i] = nir_channel(&b, oldtexel, i);
         else
            texel_comps[i] = nir_imm_intN_t(&b, 0, texel->bit_size);
      }

      texel = nir_vec(&b, texel_comps, nimgcomps);
   }

   nir_store_var(&b, out, texel, 0xff);

   uint32_t rt_conv;
   pan_pack(&rt_conv, BIFROST_INTERNAL_CONVERSION, cfg) {
      cfg.memory_format = (imgcompsz == 2 ? MALI_RG16UI : MALI_RG32UI) << 12;
      cfg.raw = true;
      cfg.register_format = imgcompsz == 2 ?
                            MALI_BIFROST_REGISTER_FILE_FORMAT_U16 :
                            MALI_BIFROST_REGISTER_FILE_FORMAT_U32;
   }

   struct panfrost_compile_inputs inputs = {
      .gpu_id = pdev->gpu_id,
      .is_blit = true,
      .bifrost.static_rt_conv = true,
      .bifrost.rt_conv[0] = rt_conv,
   };

   struct util_dynarray binary;

   util_dynarray_init(&binary, NULL);
   pan_shader_compile(pdev, b.shader, &inputs, &binary, shader_info);

   /* Make sure UBO words have been upgraded to push constants */
   assert(shader_info->ubo_count == 1);

   mali_ptr shader =
      panfrost_pool_upload_aligned(bin_pool, binary.data, binary.size,
                                   pan_is_bifrost(pdev) ? 128 : 64);

   util_dynarray_fini(&binary);
   ralloc_free(b.shader);

   return shader;
}

static const struct panvk_meta_copy_format_info panvk_meta_copy_img2buf_fmts[] = {
   { PIPE_FORMAT_R8_UINT, 0x1 },
   { PIPE_FORMAT_R8G8_UINT, 0x3 },
   { PIPE_FORMAT_R5G6B5_UNORM, 0x7 },
   { PIPE_FORMAT_R8G8B8A8_UINT, 0xf },
   { PIPE_FORMAT_R16G16B16_UINT, 0x7 },
   { PIPE_FORMAT_R32G32_UINT, 0x3 },
   { PIPE_FORMAT_R32G32B32_UINT, 0x7 },
   { PIPE_FORMAT_R32G32B32A32_UINT, 0xf },
   /* S8 -> Z24S8 */
   { PIPE_FORMAT_R8G8B8A8_UINT, 0x8 },
   /* S8 -> Z32_S8X24 */
   { PIPE_FORMAT_R32G32_UINT, 0x2 },
   /* Z24X8 -> Z24S8 */
   { PIPE_FORMAT_R8G8B8A8_UINT, 0x7 },
   /* Z32 -> Z32_S8X24 */
   { PIPE_FORMAT_R32G32_UINT, 0x1 },
};

static enum pipe_format
panvk_meta_copy_img2buf_format(enum pipe_format imgfmt)
{
   /* Pick blendable formats when we can, and the FLOAT variant matching the
    * texelsize otherwise.
    */
   switch (util_format_get_blocksize(imgfmt)) {
   case 1: return PIPE_FORMAT_R8_UINT;
   /* AFBC stores things differently for RGB565,
    * we can't simply map to R8G8 in that case */
   case 2: return (imgfmt == PIPE_FORMAT_R5G6B5_UNORM ||
                   imgfmt == PIPE_FORMAT_B5G6R5_UNORM) ?
                  PIPE_FORMAT_R5G6B5_UNORM : PIPE_FORMAT_R8G8_UINT;
   case 4: return PIPE_FORMAT_R8G8B8A8_UINT;
   case 6: return PIPE_FORMAT_R16G16B16_UINT;
   case 8: return PIPE_FORMAT_R32G32_UINT;
   case 12: return PIPE_FORMAT_R32G32B32_UINT;
   case 16: return PIPE_FORMAT_R32G32B32A32_UINT;
   default: unreachable("Invalid format\n");
   }
}

struct panvk_meta_copy_img2buf_info {
   struct {
      mali_ptr ptr;
      struct {
         unsigned line;
         unsigned surf;
      } stride;
   } buf;
   struct {
      struct {
         unsigned x, y, z;
      } offset;
      struct {
         unsigned minx, miny, maxx, maxy;
      } extent;
   } img;
};

#define panvk_meta_copy_img2buf_get_info_field(b, field) \
        nir_load_ubo((b), 1, \
                     sizeof(((struct panvk_meta_copy_img2buf_info *)0)->field) * 8, \
                     nir_imm_int(b, 0), \
                     nir_imm_int(b, offsetof(struct panvk_meta_copy_img2buf_info, field)), \
                     .align_mul = 4, \
                     .align_offset = 0, \
                     .range_base = 0, \
                     .range = ~0)

static mali_ptr
panvk_meta_copy_img2buf_shader(struct panfrost_device *pdev,
                               struct pan_pool *bin_pool,
                               struct panvk_meta_copy_format_info key,
                               unsigned texdim, unsigned texisarray,
                               struct pan_shader_info *shader_info)
{
   unsigned imgtexelsz = util_format_get_blocksize(key.imgfmt);
   unsigned buftexelsz = panvk_meta_copy_buf_texelsize(key.imgfmt, key.mask);

   /* FIXME: Won't work on compute queues, but we can't do that with
    * a compute shader if the destination is an AFBC surface.
    */
   nir_builder b =
      nir_builder_init_simple_shader(MESA_SHADER_COMPUTE,
                                     pan_shader_get_compiler_options(pdev),
                                     "panvk_meta_copy_img2buf(dim=%dD%s,imgfmt=%s,mask=%x)",
                                     texdim, texisarray ? "[]" : "",
                                     util_format_name(key.imgfmt),
                                     key.mask);

   b.shader->info.internal = true;
   b.shader->info.num_ubos = 1;

   nir_ssa_def *coord = nir_load_global_invocation_id(&b, 32);
   nir_ssa_def *bufptr =
      panvk_meta_copy_img2buf_get_info_field(&b, buf.ptr);
   nir_ssa_def *buflinestride =
      panvk_meta_copy_img2buf_get_info_field(&b, buf.stride.line);
   nir_ssa_def *bufsurfstride =
      panvk_meta_copy_img2buf_get_info_field(&b, buf.stride.surf);

   nir_ssa_def *imgminx =
      panvk_meta_copy_img2buf_get_info_field(&b, img.extent.minx);
   nir_ssa_def *imgminy =
      panvk_meta_copy_img2buf_get_info_field(&b, img.extent.miny);
   nir_ssa_def *imgmaxx =
      panvk_meta_copy_img2buf_get_info_field(&b, img.extent.maxx);
   nir_ssa_def *imgmaxy =
      panvk_meta_copy_img2buf_get_info_field(&b, img.extent.maxy);

   nir_ssa_def *imgcoords, *inbounds;

   switch (texdim + texisarray) {
   case 1:
      imgcoords =
         nir_iadd(&b,
                  nir_channel(&b, coord, 0),
                  panvk_meta_copy_img2buf_get_info_field(&b, img.offset.x));
      inbounds =
         nir_iand(&b,
                  nir_uge(&b, imgmaxx, nir_channel(&b, imgcoords, 0)),
                  nir_uge(&b, nir_channel(&b, imgcoords, 0), imgminx));
      break;
   case 2:
      imgcoords =
         nir_vec2(&b,
                  nir_iadd(&b,
                           nir_channel(&b, coord, 0),
                           panvk_meta_copy_img2buf_get_info_field(&b, img.offset.x)),
                  nir_iadd(&b,
                           nir_channel(&b, coord, 1),
                           panvk_meta_copy_img2buf_get_info_field(&b, img.offset.y)));
      inbounds =
         nir_iand(&b,
                  nir_iand(&b,
                           nir_uge(&b, imgmaxx, nir_channel(&b, imgcoords, 0)),
                           nir_uge(&b, imgmaxy, nir_channel(&b, imgcoords, 1))),
                  nir_iand(&b,
                           nir_uge(&b, nir_channel(&b, imgcoords, 0), imgminx),
                           nir_uge(&b, nir_channel(&b, imgcoords, 1), imgminy)));
      break;
   case 3:
      imgcoords =
         nir_vec3(&b,
                  nir_iadd(&b,
                           nir_channel(&b, coord, 0),
                           panvk_meta_copy_img2buf_get_info_field(&b, img.offset.x)),
                  nir_iadd(&b,
                           nir_channel(&b, coord, 1),
                           panvk_meta_copy_img2buf_get_info_field(&b, img.offset.y)),
                  nir_iadd(&b,
                           nir_channel(&b, coord, 2),
                           panvk_meta_copy_img2buf_get_info_field(&b, img.offset.y)));
      inbounds =
         nir_iand(&b,
                  nir_iand(&b,
                           nir_uge(&b, imgmaxx, nir_channel(&b, imgcoords, 0)),
                           nir_uge(&b, imgmaxy, nir_channel(&b, imgcoords, 1))),
                  nir_iand(&b,
                           nir_uge(&b, nir_channel(&b, imgcoords, 0), imgminx),
                           nir_uge(&b, nir_channel(&b, imgcoords, 1), imgminy)));
      break;
   default:
      unreachable("Invalid texture dimension\n");
   }

   nir_push_if(&b, inbounds);

   /* FIXME: doesn't work for tiled+compressed formats since blocks are 4x4
    * blocks instead of 16x16 texels in that case, and there's nothing we can
    * do to force the tile size to 4x4 in the render path.
    * This being said, compressed textures are not compatible with AFBC, so we
    * could use a compute shader arranging the blocks properly.
    */
   nir_ssa_def *offset =
      nir_imul(&b, nir_channel(&b, coord, 0), nir_imm_int(&b, buftexelsz));
   offset = nir_iadd(&b, offset,
                     nir_imul(&b, nir_channel(&b, coord, 1), buflinestride));
   offset = nir_iadd(&b, offset,
                     nir_imul(&b, nir_channel(&b, coord, 2), bufsurfstride));
   bufptr = nir_iadd(&b, bufptr, nir_u2u64(&b, offset));

   unsigned imgcompsz = imgtexelsz <= 4 ?
                        1 : MIN2(1 << (ffs(imgtexelsz) - 1), 4);
   unsigned nimgcomps = imgtexelsz / imgcompsz;
   assert(nimgcomps <= 4);

   nir_tex_instr *tex = nir_tex_instr_create(b.shader, 1);
   tex->op = nir_texop_txf;
   tex->texture_index = 0;
   tex->is_array = texisarray;
   tex->dest_type = util_format_is_unorm(key.imgfmt) ?
                    nir_type_float32 : nir_type_uint32;

   switch (texdim) {
   case 1: tex->sampler_dim = GLSL_SAMPLER_DIM_1D; break;
   case 2: tex->sampler_dim = GLSL_SAMPLER_DIM_2D; break;
   case 3: tex->sampler_dim = GLSL_SAMPLER_DIM_3D; break;
   default: unreachable("Invalid texture dimension");
   }

   tex->src[0].src_type = nir_tex_src_coord;
   tex->src[0].src = nir_src_for_ssa(imgcoords);
   tex->coord_components = texdim + texisarray;
   nir_ssa_dest_init(&tex->instr, &tex->dest, 4,
                     nir_alu_type_get_type_size(tex->dest_type), NULL);
   nir_builder_instr_insert(&b, &tex->instr);

   nir_ssa_def *texel = &tex->dest.ssa;

   unsigned fullmask = (1 << util_format_get_nr_components(key.imgfmt)) - 1;
   unsigned nbufcomps = util_bitcount(fullmask);
   if (key.mask != fullmask) {
      nir_ssa_def *bufcomps[4];
      nbufcomps = 0;
      for (unsigned i = 0; i < nimgcomps; i++) {
         if (key.mask & BITFIELD_BIT(i))
            bufcomps[nbufcomps++] = nir_channel(&b, texel, i);
      }

      texel = nir_vec(&b, bufcomps, nbufcomps);
   }

   unsigned bufcompsz = buftexelsz / nbufcomps;

   if (key.imgfmt == PIPE_FORMAT_R5G6B5_UNORM) {
      texel = nir_fmul(&b, texel,
                       nir_vec3(&b,
                                nir_imm_float(&b, 31),
                                nir_imm_float(&b, 63),
                                nir_imm_float(&b, 31)));
      texel = nir_f2u16(&b, texel);
      texel = nir_ior(&b, nir_channel(&b, texel, 0),
                      nir_ior(&b,
                              nir_ishl(&b, nir_channel(&b, texel, 1), nir_imm_int(&b, 5)),
                              nir_ishl(&b, nir_channel(&b, texel, 2), nir_imm_int(&b, 11))));
      imgcompsz = 2;
      bufcompsz = 2;
      nbufcomps = 1;
      nimgcomps = 1;
   } else if (imgcompsz == 1) {
      nir_ssa_def *packed = nir_channel(&b, texel, 0);
      for (unsigned i = 1; i < nbufcomps; i++) {
         packed = nir_ior(&b, packed,
                          nir_ishl(&b, nir_iand_imm(&b, nir_channel(&b, texel, i), 0xff),
                                   nir_imm_int(&b, i * 8)));
      }
      texel = packed;

      bufcompsz = nbufcomps == 3 ? 4 : nbufcomps;
      nbufcomps = 1;
   }

   assert(bufcompsz == 1 || bufcompsz == 2 || bufcompsz == 4);
   assert(nbufcomps <= 4 && nimgcomps <= 4);
   texel = nir_u2uN(&b, texel, bufcompsz * 8);

   nir_store_global(&b, bufptr, bufcompsz, texel, (1 << nbufcomps) - 1);
   nir_pop_if(&b, NULL);

   struct panfrost_compile_inputs inputs = {
      .gpu_id = pdev->gpu_id,
      .is_blit = true,
   };

   struct util_dynarray binary;

   util_dynarray_init(&binary, NULL);
   pan_shader_compile(pdev, b.shader, &inputs, &binary, shader_info);

   /* Make sure UBO words have been upgraded to push constants and everything
    * is at the right place.
    */
   assert(shader_info->ubo_count == 1);
   assert(shader_info->push.count <= (sizeof(struct panvk_meta_copy_img2buf_info) / 4));

   mali_ptr shader =
      panfrost_pool_upload_aligned(bin_pool, binary.data, binary.size,
                                   pan_is_bifrost(pdev) ? 128 : 64);

   util_dynarray_fini(&binary);
   ralloc_free(b.shader);

   return shader;
}

static mali_ptr
panvk_meta_copy_buf2img_emit_rsd(struct panfrost_device *pdev,
                                 struct pan_pool *bin_pool,
                                 struct pan_pool *desc_pool,
                                 struct panfrost_ubo_push *pushmap,
                                 struct panvk_meta_copy_format_info key)
{
   bool raw = util_format_get_blocksize(key.imgfmt) > 4;
   struct pan_shader_info shader_info;
   mali_ptr shader =
      panvk_meta_copy_buf2img_shader(pdev, bin_pool, key, &shader_info);

   struct panfrost_ptr rsd_ptr =
      panfrost_pool_alloc_desc_aggregate(desc_pool,
                                         PAN_DESC(RENDERER_STATE),
                                         PAN_DESC_ARRAY(1, BLEND));

   unsigned fullmask = (1 << util_format_get_nr_components(key.imgfmt)) - 1;
   bool partialwrite = fullmask != key.mask && !raw;
   bool readstb = fullmask != key.mask && raw;

   pan_pack(rsd_ptr.cpu, RENDERER_STATE, cfg) {
      pan_shader_prepare_rsd(pdev, &shader_info, shader, &cfg);
      cfg.properties.depth_source = MALI_DEPTH_SOURCE_FIXED_FUNCTION;
      cfg.multisample_misc.sample_mask = UINT16_MAX;
      cfg.multisample_misc.depth_function = MALI_FUNC_ALWAYS;
      cfg.stencil_mask_misc.stencil_mask_front = 0xFF;
      cfg.stencil_mask_misc.stencil_mask_back = 0xFF;
      cfg.stencil_front.compare_function = MALI_FUNC_ALWAYS;
      cfg.stencil_front.stencil_fail = MALI_STENCIL_OP_REPLACE;
      cfg.stencil_front.depth_fail = MALI_STENCIL_OP_REPLACE;
      cfg.stencil_front.depth_pass = MALI_STENCIL_OP_REPLACE;
      cfg.stencil_front.mask = 0xFF;
      cfg.stencil_back = cfg.stencil_front;

      if (pan_is_bifrost(pdev)) {
         cfg.properties.bifrost.allow_forward_pixel_to_be_killed = true;
         cfg.properties.bifrost.allow_forward_pixel_to_kill =
            !partialwrite && !readstb;
         cfg.properties.bifrost.zs_update_operation =
            MALI_PIXEL_KILL_STRONG_EARLY;
         cfg.properties.bifrost.pixel_kill_operation =
            MALI_PIXEL_KILL_FORCE_EARLY;
      } else {
         cfg.properties.midgard.shader_reads_tilebuffer = readstb;
         cfg.properties.midgard.work_register_count = shader_info.work_reg_count;
         cfg.properties.midgard.force_early_z = true;
         cfg.stencil_mask_misc.alpha_test_compare_function = MALI_FUNC_ALWAYS;
      }
   }

   pan_pack(rsd_ptr.cpu + MALI_RENDERER_STATE_LENGTH, BLEND, cfg) {
      cfg.round_to_fb_precision = true;
      cfg.load_destination = partialwrite;
      if (pan_is_bifrost(pdev)) {
         cfg.bifrost.internal.mode =
            partialwrite ?
            MALI_BIFROST_BLEND_MODE_FIXED_FUNCTION :
            MALI_BIFROST_BLEND_MODE_OPAQUE;
         cfg.bifrost.equation.rgb.a = (key.mask & 7) ?
                                      MALI_BLEND_OPERAND_A_SRC :
                                      MALI_BLEND_OPERAND_A_ZERO;
         cfg.bifrost.equation.rgb.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.bifrost.equation.rgb.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.bifrost.equation.rgb.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.bifrost.equation.alpha.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.bifrost.equation.alpha.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.bifrost.equation.alpha.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.bifrost.equation.color_mask = partialwrite ? key.mask : 0xf;
         cfg.bifrost.internal.fixed_function.num_comps = 4;
         if (!raw) {
            cfg.bifrost.internal.fixed_function.conversion.memory_format =
               panfrost_format_to_bifrost_blend(pdev, key.imgfmt);
            cfg.bifrost.internal.fixed_function.conversion.register_format =
               MALI_BIFROST_REGISTER_FILE_FORMAT_F32;
         } else {
            unsigned imgtexelsz = util_format_get_blocksize(key.imgfmt);

            cfg.bifrost.internal.fixed_function.conversion.memory_format =
               panvk_meta_copy_img_bifrost_raw_format(imgtexelsz);
            cfg.bifrost.internal.fixed_function.conversion.raw = true;
            cfg.bifrost.internal.fixed_function.conversion.register_format =
               (imgtexelsz & 2) ?
               MALI_BIFROST_REGISTER_FILE_FORMAT_U16 :
               MALI_BIFROST_REGISTER_FILE_FORMAT_U32;
         }
      } else {
         cfg.midgard.equation.rgb.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.midgard.equation.rgb.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.midgard.equation.rgb.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.midgard.equation.alpha.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.midgard.equation.alpha.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.midgard.equation.alpha.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.midgard.equation.color_mask = key.mask;
      }
   }

   *pushmap = shader_info.push;
   return rsd_ptr.gpu;
}

static mali_ptr
panvk_meta_copy_img2buf_emit_rsd(struct panfrost_device *pdev,
                                 struct pan_pool *bin_pool,
                                 struct pan_pool *desc_pool,
                                 struct panfrost_ubo_push *pushmap,
                                 struct panvk_meta_copy_format_info key,
                                 unsigned texdim, unsigned texisarray)
{
   struct pan_shader_info shader_info;

   mali_ptr shader =
      panvk_meta_copy_img2buf_shader(pdev, bin_pool, key, texdim, texisarray,
                                     &shader_info);

   struct panfrost_ptr rsd_ptr =
      panfrost_pool_alloc_desc_aggregate(desc_pool,
                                         PAN_DESC(RENDERER_STATE));

   pan_pack(rsd_ptr.cpu, RENDERER_STATE, cfg) {
      pan_shader_prepare_rsd(pdev, &shader_info, shader, &cfg);
      cfg.shader.texture_count = 1;
      cfg.shader.sampler_count = 1;
   }

   *pushmap = shader_info.push;
   return rsd_ptr.gpu;
}

static mali_ptr
panvk_meta_copy_buf2img_emit_push_constants(struct panfrost_device *pdev,
                                            const struct panfrost_ubo_push *pushmap,
                                            struct pan_pool *pool,
                                            const struct panvk_meta_copy_buf2img_info *info)
{
   assert(pushmap->count <= (sizeof(*info) / 4));

   uint32_t *in = (uint32_t *)info;
   uint32_t pushvals[sizeof(*info) / 4];

   for (unsigned i = 0; i < pushmap->count; i++) {
      assert(i < ARRAY_SIZE(pushvals));
      assert(pushmap->words[i].ubo == 0);
      assert(pushmap->words[i].offset < sizeof(*info));
      pushvals[i] = in[pushmap->words[i].offset / 4];
   }

   return panfrost_pool_upload_aligned(pool, pushvals, sizeof(pushvals), 16);
}

static mali_ptr
panvk_meta_copy_buf2img_emit_ubo(struct panfrost_device *pdev,
                                 const struct panfrost_ubo_push *pushmap,
                                 struct pan_pool *pool,
                                 const struct panvk_meta_copy_buf2img_info *info)
{
   struct panfrost_ptr ubo = panfrost_pool_alloc_desc(pool, UNIFORM_BUFFER);

   pan_pack(ubo.cpu, UNIFORM_BUFFER, cfg) {
      cfg.entries = DIV_ROUND_UP(sizeof(*info), 16);
      cfg.pointer = panfrost_pool_upload_aligned(pool, info, sizeof(*info), 16);
   }

   return ubo.gpu;
}

static mali_ptr
panvk_meta_copy_img2buf_emit_push_constants(struct panfrost_device *pdev,
                                            const struct panfrost_ubo_push *pushmap,
                                            struct pan_pool *pool,
                                            const struct panvk_meta_copy_img2buf_info *info)
{
   assert(pushmap->count <= (sizeof(*info) / 4));

   uint32_t *in = (uint32_t *)info;
   uint32_t pushvals[sizeof(*info) / 4];

   for (unsigned i = 0; i < pushmap->count; i++) {
      assert(i < ARRAY_SIZE(pushvals));
      assert(pushmap->words[i].ubo == 0);
      assert(pushmap->words[i].offset < sizeof(*info));
      pushvals[i] = in[pushmap->words[i].offset / 4];
   }

   return panfrost_pool_upload_aligned(pool, pushvals, sizeof(pushvals), 16);
}

static mali_ptr
panvk_meta_copy_img2buf_emit_ubo(struct panfrost_device *pdev,
                                 const struct panfrost_ubo_push *pushmap,
                                 struct pan_pool *pool,
                                 const struct panvk_meta_copy_img2buf_info *info)
{
   struct panfrost_ptr ubo = panfrost_pool_alloc_desc(pool, UNIFORM_BUFFER);

   pan_pack(ubo.cpu, UNIFORM_BUFFER, cfg) {
      cfg.entries = DIV_ROUND_UP(sizeof(*info), 16);
      cfg.pointer = panfrost_pool_upload_aligned(pool, info, sizeof(*info), 16);
   }

   return ubo.gpu;
}

static void
panvk_meta_copy_buf2img_emit_dcd(struct pan_pool *pool,
                                 mali_ptr src_coords, mali_ptr dst_coords,
                                 mali_ptr ubo, mali_ptr push_constants,
                                 mali_ptr vpd, mali_ptr tsd, mali_ptr rsd,
                                 void *out)
{
   pan_pack(out, DRAW, cfg) {
      cfg.four_components_per_vertex = true;
      cfg.draw_descriptor_is_64b = true;
      cfg.thread_storage = tsd;
      cfg.state = rsd;
      cfg.uniform_buffers = ubo;
      cfg.push_uniforms = push_constants;
      cfg.position = dst_coords;
      panvk_meta_copy_emit_varying(pool, src_coords,
                                   &cfg.varying_buffers,
                                   &cfg.varyings);
      cfg.viewport = vpd;
      cfg.texture_descriptor_is_64b = !pan_is_bifrost(pool->dev);
   }
}

static void
panvk_meta_copy_img2buf_emit_dcd(struct pan_pool *pool,
                                 mali_ptr texture, mali_ptr sampler,
                                 mali_ptr ubo, mali_ptr push_constants,
                                 mali_ptr tsd, mali_ptr rsd,
                                 void *out)
{
   pan_pack(out, DRAW, cfg) {
      cfg.four_components_per_vertex = true;
      cfg.draw_descriptor_is_64b = true;
      cfg.thread_storage = tsd;
      cfg.state = rsd;
      cfg.uniform_buffers = ubo;
      cfg.push_uniforms = push_constants;
      cfg.texture_descriptor_is_64b = !pan_is_bifrost(pool->dev);
      cfg.textures = texture;
      cfg.samplers = sampler;
   }
}

static struct panfrost_ptr
panvk_meta_copy_buf2img_emit_midgard_tiler_job(struct pan_pool *desc_pool,
                                               struct pan_scoreboard *scoreboard,
                                               mali_ptr src_coords, mali_ptr dst_coords,
                                               mali_ptr ubo, mali_ptr push_constants,
                                               mali_ptr vpd, mali_ptr rsd, mali_ptr tsd)
{
   struct panfrost_ptr job =
      panfrost_pool_alloc_desc(desc_pool, MIDGARD_TILER_JOB);

   panvk_meta_copy_buf2img_emit_dcd(desc_pool,
                                    src_coords, dst_coords,
                                    ubo, push_constants,
                                    vpd, tsd, rsd,
                                    pan_section_ptr(job.cpu, MIDGARD_TILER_JOB, DRAW));

   pan_section_pack(job.cpu, MIDGARD_TILER_JOB, PRIMITIVE, cfg) {
      cfg.draw_mode = MALI_DRAW_MODE_TRIANGLE_STRIP;
      cfg.index_count = 4;
      cfg.job_task_split = 6;
   }

   pan_section_pack(job.cpu, MIDGARD_TILER_JOB, PRIMITIVE_SIZE, cfg) {
      cfg.constant = 1.0f;
   }

   void *invoc = pan_section_ptr(job.cpu,
                                 MIDGARD_TILER_JOB,
                                 INVOCATION);
   panfrost_pack_work_groups_compute(invoc, 1, 4,
                                     1, 1, 1, 1, true);

   panfrost_add_job(desc_pool, scoreboard, MALI_JOB_TYPE_TILER,
                    false, false, 0, 0, &job, false);
   return job;
}

static struct panfrost_ptr
panvk_meta_copy_buf2img_emit_bifrost_tiler_job(struct pan_pool *desc_pool,
                                               struct pan_scoreboard *scoreboard,
                                               mali_ptr src_coords, mali_ptr dst_coords,
                                               mali_ptr ubo, mali_ptr push_constants,
                                               mali_ptr vpd, mali_ptr rsd,
                                               mali_ptr tsd, mali_ptr tiler)
{
   struct panfrost_ptr job =
      panfrost_pool_alloc_desc(desc_pool, BIFROST_TILER_JOB);

   panvk_meta_copy_buf2img_emit_dcd(desc_pool,
                                    src_coords, dst_coords,
                                    ubo, push_constants,
                                    vpd, tsd, rsd,
                                    pan_section_ptr(job.cpu, BIFROST_TILER_JOB, DRAW));

   pan_section_pack(job.cpu, BIFROST_TILER_JOB, PRIMITIVE, cfg) {
      cfg.draw_mode = MALI_DRAW_MODE_TRIANGLE_STRIP;
      cfg.index_count = 4;
      cfg.job_task_split = 6;
   }

   pan_section_pack(job.cpu, BIFROST_TILER_JOB, PRIMITIVE_SIZE, cfg) {
      cfg.constant = 1.0f;
   }

   void *invoc = pan_section_ptr(job.cpu,
                                 BIFROST_TILER_JOB,
                                 INVOCATION);
   panfrost_pack_work_groups_compute(invoc, 1, 4,
                                     1, 1, 1, 1, true);

   pan_section_pack(job.cpu, BIFROST_TILER_JOB, PADDING, cfg);
   pan_section_pack(job.cpu, BIFROST_TILER_JOB, TILER, cfg) {
      cfg.address = tiler;
   }

   panfrost_add_job(desc_pool, scoreboard, MALI_JOB_TYPE_TILER,
                    false, false, 0, 0, &job, false);
   return job;
}

static struct panfrost_ptr
panvk_meta_copy_img2buf_emit_compute_job(struct pan_pool *desc_pool,
                                         struct pan_scoreboard *scoreboard,
                                         unsigned num_x, unsigned num_y,
                                         unsigned num_z,
                                         mali_ptr texture, mali_ptr sampler,
                                         mali_ptr ubo, mali_ptr push_constants,
                                         mali_ptr rsd, mali_ptr tsd)
{
   struct panfrost_ptr job =
      panfrost_pool_alloc_desc(desc_pool, COMPUTE_JOB);

   void *invoc = pan_section_ptr(job.cpu,
                                 COMPUTE_JOB,
                                 INVOCATION);
   panfrost_pack_work_groups_compute(invoc, num_x, num_y, num_z,
                                     16, 16, 1, false);

   pan_section_pack(job.cpu, COMPUTE_JOB, PARAMETERS, cfg) {
      cfg.job_task_split = 8;
   }

   panvk_meta_copy_img2buf_emit_dcd(desc_pool,
                                    texture, sampler,
                                    ubo, push_constants,
                                    tsd, rsd,
                                    pan_section_ptr(job.cpu, COMPUTE_JOB, DRAW));

   pan_section_pack(job.cpu, COMPUTE_JOB, DRAW_PADDING, cfg);

   panfrost_add_job(desc_pool, scoreboard, MALI_JOB_TYPE_COMPUTE,
                    false, false, 0, 0, &job, false);
   return job;
}

static unsigned
panvk_meta_copy_buf2img_format_idx(struct panvk_meta_copy_format_info key)
{
   for (unsigned i = 0; i < ARRAY_SIZE(panvk_meta_copy_buf2img_fmts); i++) {
      if (!memcmp(&key, &panvk_meta_copy_buf2img_fmts[i], sizeof(key)))
         return i;
   }

   unreachable("Invalid image format\n");
}

static void
panvk_meta_copy_buf2img(struct panvk_cmd_buffer *cmdbuf,
                        const struct panvk_buffer *buf,
                        const struct panvk_image *img,
                        const VkBufferImageCopy *region)
{
   struct panfrost_device *pdev = &cmdbuf->device->physical_device->pdev;
   unsigned minx = MAX2(region->imageOffset.x, 0);
   unsigned miny = MAX2(region->imageOffset.y, 0);
   unsigned maxx = MAX2(region->imageOffset.x + region->imageExtent.width - 1, 0);
   unsigned maxy = MAX2(region->imageOffset.y + region->imageExtent.height - 1, 0);

   mali_ptr vpd =
      panvk_meta_copy_img_emit_viewport(&cmdbuf->desc_pool,
                                        minx, miny, maxx, maxy);

   float dst_rect[] = {
      minx, miny, 0.0, 1.0,
      maxx + 1, miny, 0.0, 1.0,
      minx, maxy + 1, 0.0, 1.0,
      maxx + 1, maxy + 1, 0.0, 1.0,
   };
   mali_ptr dst_coords =
      panfrost_pool_upload_aligned(&cmdbuf->desc_pool, dst_rect,
                                   sizeof(dst_rect), 64);

   struct panvk_meta_copy_format_info key = {
      .imgfmt = panvk_meta_copy_buf2img_format(img->pimage.layout.format),
      .mask = panvk_meta_copy_img_mask(img->pimage.layout.format,
                                       region->imageSubresource.aspectMask),
   };

   unsigned fmtidx = panvk_meta_copy_buf2img_format_idx(key);

   mali_ptr rsd =
      cmdbuf->device->physical_device->meta.copy.buf2img[fmtidx].rsd;
   const struct panfrost_ubo_push *pushmap =
      &cmdbuf->device->physical_device->meta.copy.buf2img[fmtidx].pushmap;

   unsigned buftexelsz = panvk_meta_copy_buf_texelsize(key.imgfmt, key.mask);
   struct panvk_meta_copy_buf2img_info info = {
      .buf.ptr = buf->bo->ptr.gpu + buf->bo_offset + region->bufferOffset,
      .buf.stride.line = (region->bufferRowLength ? : region->imageExtent.width) * buftexelsz,
   };

   info.buf.stride.surf =
      (region->bufferImageHeight ? : region->imageExtent.height) * info.buf.stride.line;

   mali_ptr pushconsts =
      panvk_meta_copy_buf2img_emit_push_constants(pdev, pushmap, &cmdbuf->desc_pool, &info);
   mali_ptr ubo =
      panvk_meta_copy_buf2img_emit_ubo(pdev, pushmap, &cmdbuf->desc_pool, &info);

   /* TODO: don't force preloads of dst resources if unneeded */
   struct pan_image_state state = { 0 };
   state.slices[region->imageSubresource.mipLevel].data_valid = true;

   struct pan_image_view view = {
      .format = key.imgfmt,
      .dim = MALI_TEXTURE_DIMENSION_2D,
      .image = &img->pimage,
      .nr_samples = img->pimage.layout.nr_samples,
      .first_level = region->imageSubresource.mipLevel,
      .last_level = region->imageSubresource.mipLevel,
      .swizzle = { PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y, PIPE_SWIZZLE_Z, PIPE_SWIZZLE_W },
   };

   struct pan_fb_info fbinfo = {
      .width = u_minify(img->pimage.layout.width, region->imageSubresource.mipLevel),
      .height = u_minify(img->pimage.layout.height, region->imageSubresource.mipLevel),
      .extent.minx = minx,
      .extent.maxx = maxx,
      .extent.miny = miny,
      .extent.maxy = maxy,
      .nr_samples = 1,
      .rt_count = 1,
      .rts[0].view = &view,
      .rts[0].state = &state,
      .rts[0].preload = true,
   };

   struct pan_tls_info tlsinfo = { 0 };

   if (cmdbuf->state.batch)
      panvk_cmd_close_batch(cmdbuf);

   assert(region->imageSubresource.layerCount == 1 ||
          region->imageExtent.depth == 1);
   assert(region->imageOffset.z >= 0);
   unsigned first_layer = MAX2(region->imageSubresource.baseArrayLayer, region->imageOffset.z);
   unsigned nlayers = MAX2(region->imageSubresource.layerCount, region->imageExtent.depth);
   for (unsigned l = 0; l < nlayers; l++) {
      float src_rect[] = {
         0, 0, l, 1.0,
         region->imageExtent.width, 0, l, 1.0,
         0, region->imageExtent.height, l, 1.0,
         region->imageExtent.width, region->imageExtent.height, l, 1.0,
      };

      mali_ptr src_coords =
         panfrost_pool_upload_aligned(&cmdbuf->desc_pool, src_rect,
                                      sizeof(src_rect), 64);

      panvk_cmd_open_batch(cmdbuf);

      struct panvk_batch *batch = cmdbuf->state.batch;

      view.first_layer = view.last_layer = l + first_layer;
      panvk_meta_blit_emit_tls(cmdbuf, &tlsinfo);
      panvk_meta_blit_emit_fb(cmdbuf, &fbinfo, &tlsinfo);

      mali_ptr tsd, tiler;

      if (pan_is_bifrost(pdev)) {
         tsd = batch->tls.gpu;
         tiler = batch->tiler.bifrost_descs.gpu;
      } else {
         tsd = batch->fb.desc.gpu;
         tiler = 0;
      }

      struct panfrost_ptr job;

      if (pan_is_bifrost(pdev)) {
         job = panvk_meta_copy_buf2img_emit_bifrost_tiler_job(&cmdbuf->desc_pool,
                                                              &batch->scoreboard,
                                                              src_coords, dst_coords,
                                                              ubo, pushconsts,
                                                              vpd, rsd, tsd, tiler);
      } else {
         job = panvk_meta_copy_buf2img_emit_midgard_tiler_job(&cmdbuf->desc_pool,
                                                              &batch->scoreboard,
                                                              src_coords, dst_coords,
                                                              ubo, pushconsts,
                                                              vpd, rsd, tsd);
      }

      util_dynarray_append(&batch->jobs, void *, job.cpu);
      panvk_meta_blit_emit_fragment_job(cmdbuf, &fbinfo);
      batch->blit.src = buf->bo;
      batch->blit.dst = img->pimage.data.bo;
      panvk_meta_blit_close_batch(cmdbuf);
   }

   if (cmdbuf->state.batch)
      panvk_meta_blit_close_batch(cmdbuf);
}

static void
panvk_meta_copy_buf2img_init(struct panvk_physical_device *dev)
{
   STATIC_ASSERT(ARRAY_SIZE(panvk_meta_copy_buf2img_fmts) == PANVK_META_COPY_BUF2IMG_NUM_FORMATS);

   for (unsigned i = 0; i < ARRAY_SIZE(panvk_meta_copy_buf2img_fmts); i++) {
      dev->meta.copy.buf2img[i].rsd =
         panvk_meta_copy_buf2img_emit_rsd(&dev->pdev, &dev->meta.bin_pool,
                                     &dev->meta.desc_pool,
                                     &dev->meta.copy.buf2img[i].pushmap,
                                     panvk_meta_copy_buf2img_fmts[i]);
   }
}

void
panvk_CmdCopyBufferToImage(VkCommandBuffer commandBuffer,
                           VkBuffer srcBuffer,
                           VkImage destImage,
                           VkImageLayout destImageLayout,
                           uint32_t regionCount,
                           const VkBufferImageCopy *pRegions)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_buffer, buf, srcBuffer);
   VK_FROM_HANDLE(panvk_image, img, destImage);

   for (unsigned i = 0; i < regionCount; i++) {
      panvk_meta_copy_buf2img(cmdbuf, buf, img, &pRegions[i]);
   }
}

static unsigned
panvk_meta_copy_img2buf_format_idx(struct panvk_meta_copy_format_info key)
{
   for (unsigned i = 0; i < ARRAY_SIZE(panvk_meta_copy_img2buf_fmts); i++) {
      if (!memcmp(&key, &panvk_meta_copy_img2buf_fmts[i], sizeof(key)))
         return i;
   }

   unreachable("Invalid texel size\n");
}

static void
panvk_meta_copy_img2buf(struct panvk_cmd_buffer *cmdbuf,
                        const struct panvk_buffer *buf,
                        const struct panvk_image *img,
                        const VkBufferImageCopy *region)
{
   struct panfrost_device *pdev = &cmdbuf->device->physical_device->pdev;
   struct panvk_meta_copy_format_info key = {
      .imgfmt = panvk_meta_copy_img2buf_format(img->pimage.layout.format),
      .mask = panvk_meta_copy_img_mask(img->pimage.layout.format,
                                       region->imageSubresource.aspectMask),
   };
   unsigned buftexelsz = panvk_meta_copy_buf_texelsize(key.imgfmt, key.mask);
   unsigned texdimidx =
      panvk_meta_copy_tex_type(img->pimage.layout.dim,
                               img->pimage.layout.array_size > 1);
   unsigned fmtidx = panvk_meta_copy_img2buf_format_idx(key);

   mali_ptr rsd =
      cmdbuf->device->physical_device->meta.copy.img2buf[texdimidx][fmtidx].rsd;
   const struct panfrost_ubo_push *pushmap =
      &cmdbuf->device->physical_device->meta.copy.img2buf[texdimidx][fmtidx].pushmap;

   struct panvk_meta_copy_img2buf_info info = {
      .buf.ptr = buf->bo->ptr.gpu + buf->bo_offset + region->bufferOffset,
      .buf.stride.line = (region->bufferRowLength ? : region->imageExtent.width) * buftexelsz,
      .img.offset.x = MAX2(region->imageOffset.x & ~15, 0),
      .img.offset.y = MAX2(region->imageOffset.y & ~15, 0),
      .img.offset.z = MAX2(region->imageOffset.z, 0),
      .img.extent.minx = MAX2(region->imageOffset.x, 0),
      .img.extent.miny = MAX2(region->imageOffset.y, 0),
      .img.extent.maxx = MAX2(region->imageOffset.x + region->imageExtent.width - 1, 0),
      .img.extent.maxy = MAX2(region->imageOffset.y + region->imageExtent.height - 1, 0),
   };

   info.buf.stride.surf = (region->bufferImageHeight ? : region->imageExtent.height) *
                          info.buf.stride.line;

   mali_ptr pushconsts =
      panvk_meta_copy_img2buf_emit_push_constants(pdev, pushmap, &cmdbuf->desc_pool, &info);
   mali_ptr ubo =
      panvk_meta_copy_img2buf_emit_ubo(pdev, pushmap, &cmdbuf->desc_pool, &info);

   struct pan_image_view view = {
      .format = key.imgfmt,
      .dim = img->pimage.layout.dim == MALI_TEXTURE_DIMENSION_CUBE ?
             MALI_TEXTURE_DIMENSION_2D : img->pimage.layout.dim,
      .image = &img->pimage,
      .nr_samples = img->pimage.layout.nr_samples,
      .first_level = region->imageSubresource.mipLevel,
      .last_level = region->imageSubresource.mipLevel,
      .first_layer = region->imageSubresource.baseArrayLayer,
      .last_layer = region->imageSubresource.baseArrayLayer + region->imageSubresource.layerCount - 1,
      .swizzle = { PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y, PIPE_SWIZZLE_Z, PIPE_SWIZZLE_W },
   };

   mali_ptr texture =
      panvk_meta_copy_img_emit_texture(pdev, &cmdbuf->desc_pool, &view);
   mali_ptr sampler =
      panvk_meta_copy_img_emit_sampler(pdev, &cmdbuf->desc_pool);

   if (cmdbuf->state.batch)
      panvk_cmd_close_batch(cmdbuf);

   panvk_cmd_open_batch(cmdbuf);

   struct panvk_batch *batch = cmdbuf->state.batch;

   struct pan_tls_info tlsinfo = { 0 };

   batch->blit.src = img->pimage.data.bo;
   batch->blit.dst = buf->bo;
   batch->tls =
      panfrost_pool_alloc_aligned(&cmdbuf->desc_pool,
                                  MALI_LOCAL_STORAGE_LENGTH, 64);
   pan_emit_tls(pdev, &tlsinfo, batch->tls.cpu);

   mali_ptr tsd = batch->tls.gpu;

   unsigned num_wg_x = (ALIGN_POT(info.img.extent.maxx + 1, 16) - info.img.offset.x) / 16;
   unsigned num_wg_y = (ALIGN_POT(info.img.extent.maxy + 1, 16) - info.img.offset.y) / 16;
   unsigned num_wg_z = MAX2(region->imageSubresource.layerCount, region->imageExtent.depth);
   struct panfrost_ptr job =
      panvk_meta_copy_img2buf_emit_compute_job(&cmdbuf->desc_pool,
                                               &batch->scoreboard,
                                               num_wg_x, num_wg_y, num_wg_z,
                                               texture, sampler,
                                               ubo, pushconsts,
                                               rsd, tsd);

   util_dynarray_append(&batch->jobs, void *, job.cpu);

   if (cmdbuf->state.batch)
      panvk_meta_blit_close_batch(cmdbuf);
}

static void
panvk_meta_copy_img2buf_init(struct panvk_physical_device *dev)
{
   STATIC_ASSERT(ARRAY_SIZE(panvk_meta_copy_img2buf_fmts) == PANVK_META_COPY_IMG2BUF_NUM_FORMATS);

   for (unsigned i = 0; i < ARRAY_SIZE(panvk_meta_copy_img2buf_fmts); i++) {
      for (unsigned texdim = 1; texdim <= 3; texdim++) {
         unsigned texdimidx = panvk_meta_copy_tex_type(texdim, false);
         assert(texdimidx < ARRAY_SIZE(dev->meta.copy.img2buf));
         dev->meta.copy.img2buf[texdimidx][i].rsd =
            panvk_meta_copy_img2buf_emit_rsd(&dev->pdev, &dev->meta.bin_pool,
                                        &dev->meta.desc_pool,
                                        &dev->meta.copy.img2buf[texdimidx][i].pushmap,
                                        panvk_meta_copy_img2buf_fmts[i], texdim, false);

         if (texdim == 3)
            continue;

         texdimidx = panvk_meta_copy_tex_type(texdim, true);
         assert(texdimidx < ARRAY_SIZE(dev->meta.copy.img2buf));
         dev->meta.copy.img2buf[texdimidx][i].rsd =
            panvk_meta_copy_img2buf_emit_rsd(&dev->pdev, &dev->meta.bin_pool,
                                        &dev->meta.desc_pool,
                                        &dev->meta.copy.img2buf[texdimidx][i].pushmap,
                                        panvk_meta_copy_img2buf_fmts[i], texdim, true);
      }
   }
}

void
panvk_CmdCopyImageToBuffer(VkCommandBuffer commandBuffer,
                           VkImage srcImage,
                           VkImageLayout srcImageLayout,
                           VkBuffer destBuffer,
                           uint32_t regionCount,
                           const VkBufferImageCopy *pRegions)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_buffer, buf, destBuffer);
   VK_FROM_HANDLE(panvk_image, img, srcImage);

   for (unsigned i = 0; i < regionCount; i++) {
      panvk_meta_copy_img2buf(cmdbuf, buf, img, &pRegions[i]);
   }
}

struct panvk_meta_copy_buf2buf_info {
   mali_ptr src;
   mali_ptr dst;
};

#define panvk_meta_copy_buf2buf_get_info_field(b, field) \
        nir_load_ubo((b), 1, \
                     sizeof(((struct panvk_meta_copy_buf2buf_info *)0)->field) * 8, \
                     nir_imm_int(b, 0), \
                     nir_imm_int(b, offsetof(struct panvk_meta_copy_buf2buf_info, field)), \
                     .align_mul = 4, \
                     .align_offset = 0, \
                     .range_base = 0, \
                     .range = ~0)

static mali_ptr
panvk_meta_copy_buf2buf_shader(struct panfrost_device *pdev,
                               struct pan_pool *bin_pool,
                               unsigned blksz,
                               struct pan_shader_info *shader_info)
{
   /* FIXME: Won't work on compute queues, but we can't do that with
    * a compute shader if the destination is an AFBC surface.
    */
   nir_builder b =
      nir_builder_init_simple_shader(MESA_SHADER_COMPUTE,
                                     pan_shader_get_compiler_options(pdev),
                                     "panvk_meta_copy_buf2buf(blksz=%d)",
                                     blksz);

   b.shader->info.internal = true;
   b.shader->info.num_ubos = 1;

   nir_ssa_def *coord = nir_load_global_invocation_id(&b, 32);

   nir_ssa_def *offset =
      nir_u2u64(&b, nir_imul(&b, nir_channel(&b, coord, 0), nir_imm_int(&b, blksz)));
   nir_ssa_def *srcptr =
      nir_iadd(&b, panvk_meta_copy_buf2buf_get_info_field(&b, src), offset);
   nir_ssa_def *dstptr =
      nir_iadd(&b, panvk_meta_copy_buf2buf_get_info_field(&b, dst), offset);

   unsigned compsz = blksz < 4 ? blksz : 4;
   unsigned ncomps = blksz / compsz;
   nir_store_global(&b, dstptr, blksz,
                    nir_load_global(&b, srcptr, blksz, ncomps, compsz * 8),
                    (1 << ncomps) - 1);

   struct panfrost_compile_inputs inputs = {
      .gpu_id = pdev->gpu_id,
      .is_blit = true,
   };

   struct util_dynarray binary;

   util_dynarray_init(&binary, NULL);
   pan_shader_compile(pdev, b.shader, &inputs, &binary, shader_info);

   /* Make sure UBO words have been upgraded to push constants and everything
    * is at the right place.
    */
   assert(shader_info->ubo_count == 1);
   assert(shader_info->push.count == (sizeof(struct panvk_meta_copy_buf2buf_info) / 4));

   mali_ptr shader =
      panfrost_pool_upload_aligned(bin_pool, binary.data, binary.size,
                                   pan_is_bifrost(pdev) ? 128 : 64);

   util_dynarray_fini(&binary);
   ralloc_free(b.shader);

   return shader;
}

static mali_ptr
panvk_meta_copy_buf2buf_emit_rsd(struct panfrost_device *pdev,
                                 struct pan_pool *bin_pool,
                                 struct pan_pool *desc_pool,
                                 struct panfrost_ubo_push *pushmap,
                                 unsigned blksz)
{
   struct pan_shader_info shader_info;

   mali_ptr shader =
      panvk_meta_copy_buf2buf_shader(pdev, bin_pool, blksz, &shader_info);

   struct panfrost_ptr rsd_ptr =
      panfrost_pool_alloc_desc_aggregate(desc_pool,
                                         PAN_DESC(RENDERER_STATE));

   pan_pack(rsd_ptr.cpu, RENDERER_STATE, cfg) {
      pan_shader_prepare_rsd(pdev, &shader_info, shader, &cfg);
   }

   *pushmap = shader_info.push;
   return rsd_ptr.gpu;
}

static mali_ptr
panvk_meta_copy_buf2buf_emit_push_constants(struct panfrost_device *pdev,
                                            const struct panfrost_ubo_push *pushmap,
                                            struct pan_pool *pool,
                                            const struct panvk_meta_copy_buf2buf_info *info)
{
   assert(pushmap->count <= (sizeof(*info) / 4));

   uint32_t *in = (uint32_t *)info;
   uint32_t pushvals[sizeof(*info) / 4];

   for (unsigned i = 0; i < pushmap->count; i++) {
      assert(i < ARRAY_SIZE(pushvals));
      assert(pushmap->words[i].ubo == 0);
      assert(pushmap->words[i].offset < sizeof(*info));
      pushvals[i] = in[pushmap->words[i].offset / 4];
   }

   return panfrost_pool_upload_aligned(pool, pushvals, sizeof(pushvals), 16);
}

static mali_ptr
panvk_meta_copy_buf2buf_emit_ubo(struct panfrost_device *pdev,
                                 const struct panfrost_ubo_push *pushmap,
                                 struct pan_pool *pool,
                                 const struct panvk_meta_copy_buf2buf_info *info)
{
   struct panfrost_ptr ubo = panfrost_pool_alloc_desc(pool, UNIFORM_BUFFER);

   pan_pack(ubo.cpu, UNIFORM_BUFFER, cfg) {
      cfg.entries = DIV_ROUND_UP(sizeof(*info), 16);
      cfg.pointer = panfrost_pool_upload_aligned(pool, info, sizeof(*info), 16);
   }

   return ubo.gpu;
}

static void
panvk_meta_copy_buf2buf_emit_dcd(struct pan_pool *pool,
                                 mali_ptr ubo, mali_ptr push_constants,
                                 mali_ptr tsd, mali_ptr rsd,
                                 void *out)
{
   pan_pack(out, DRAW, cfg) {
      cfg.four_components_per_vertex = true;
      cfg.draw_descriptor_is_64b = true;
      cfg.thread_storage = tsd;
      cfg.state = rsd;
      cfg.push_uniforms = push_constants;
      cfg.uniform_buffers = ubo;
      cfg.texture_descriptor_is_64b = !pan_is_bifrost(pool->dev);
   }
}

static struct panfrost_ptr
panvk_meta_copy_buf2buf_emit_compute_job(struct pan_pool *desc_pool,
                                         struct pan_scoreboard *scoreboard,
                                         unsigned nblocks,
                                         mali_ptr ubo, mali_ptr push_constants,
                                         mali_ptr rsd, mali_ptr tsd)
{
   struct panfrost_ptr job =
      panfrost_pool_alloc_desc(desc_pool, COMPUTE_JOB);

   void *invoc = pan_section_ptr(job.cpu,
                                 COMPUTE_JOB,
                                 INVOCATION);
   panfrost_pack_work_groups_compute(invoc, nblocks, 1, 1,
                                     1, 1, 1, false);

   pan_section_pack(job.cpu, COMPUTE_JOB, PARAMETERS, cfg) {
      cfg.job_task_split = 8;
   }

   panvk_meta_copy_buf2buf_emit_dcd(desc_pool,
                                    ubo, push_constants,
                                    tsd, rsd,
                                    pan_section_ptr(job.cpu, COMPUTE_JOB, DRAW));

   pan_section_pack(job.cpu, COMPUTE_JOB, DRAW_PADDING, cfg);

   panfrost_add_job(desc_pool, scoreboard, MALI_JOB_TYPE_COMPUTE,
                    false, false, 0, 0, &job, false);
   return job;
}

static void
panvk_meta_copy_buf2buf_init(struct panvk_physical_device *dev)
{
   for (unsigned i = 0; i < ARRAY_SIZE(dev->meta.copy.buf2buf); i++) {
      dev->meta.copy.buf2buf[i].rsd =
         panvk_meta_copy_buf2buf_emit_rsd(&dev->pdev, &dev->meta.bin_pool,
                                          &dev->meta.desc_pool,
                                          &dev->meta.copy.buf2buf[i].pushmap,
                                          1 << i);
   }
}

static void
panvk_meta_copy_buf2buf(struct panvk_cmd_buffer *cmdbuf,
                        const struct panvk_buffer *src,
                        const struct panvk_buffer *dst,
                        const VkBufferCopy *region)
{
   struct panfrost_device *pdev = &cmdbuf->device->physical_device->pdev;

   struct panvk_meta_copy_buf2buf_info info = {
      .src = src->bo->ptr.gpu + src->bo_offset + region->srcOffset,
      .dst = dst->bo->ptr.gpu + dst->bo_offset + region->dstOffset,
   };

   unsigned alignment = ffs((info.src | info.dst | region->size) & 15);
   unsigned log2blksz = alignment ? alignment - 1 : 4;

   assert(log2blksz < ARRAY_SIZE(cmdbuf->device->physical_device->meta.copy.buf2buf));
   mali_ptr rsd =
      cmdbuf->device->physical_device->meta.copy.buf2buf[log2blksz].rsd;
   const struct panfrost_ubo_push *pushmap =
      &cmdbuf->device->physical_device->meta.copy.buf2buf[log2blksz].pushmap;

   mali_ptr pushconsts =
      panvk_meta_copy_buf2buf_emit_push_constants(pdev, pushmap, &cmdbuf->desc_pool, &info);
   mali_ptr ubo =
      panvk_meta_copy_buf2buf_emit_ubo(pdev, pushmap, &cmdbuf->desc_pool, &info);

   if (cmdbuf->state.batch)
      panvk_cmd_close_batch(cmdbuf);

   panvk_cmd_open_batch(cmdbuf);

   struct panvk_batch *batch = cmdbuf->state.batch;

   struct pan_tls_info tlsinfo = { 0 };

   batch->tls =
      panfrost_pool_alloc_aligned(&cmdbuf->desc_pool,
                                  MALI_LOCAL_STORAGE_LENGTH, 64);
   pan_emit_tls(pdev, &tlsinfo, batch->tls.cpu);

   mali_ptr tsd = batch->tls.gpu;

   unsigned nblocks = region->size >> log2blksz;
   struct panfrost_ptr job =
      panvk_meta_copy_buf2buf_emit_compute_job(&cmdbuf->desc_pool,
                                               &batch->scoreboard,
                                               nblocks,
                                               ubo, pushconsts,
                                               rsd, tsd);

   util_dynarray_append(&batch->jobs, void *, job.cpu);

   batch->blit.src = src->bo;
   batch->blit.dst = dst->bo;
   panvk_meta_blit_close_batch(cmdbuf);
}

void
panvk_CmdCopyBuffer(VkCommandBuffer commandBuffer,
                    VkBuffer srcBuffer,
                    VkBuffer destBuffer,
                    uint32_t regionCount,
                    const VkBufferCopy *pRegions)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_buffer, src, srcBuffer);
   VK_FROM_HANDLE(panvk_buffer, dst, destBuffer);

   for (unsigned i = 0; i < regionCount; i++) {
      panvk_meta_copy_buf2buf(cmdbuf, src, dst, &pRegions[i]);
   }
}

void
panvk_CmdResolveImage(VkCommandBuffer cmd_buffer_h,
                      VkImage src_image_h,
                      VkImageLayout src_image_layout,
                      VkImage dest_image_h,
                      VkImageLayout dest_image_layout,
                      uint32_t region_count,
                      const VkImageResolve *regions)
{
   panvk_stub();
}

void
panvk_CmdFillBuffer(VkCommandBuffer commandBuffer,
                    VkBuffer dstBuffer,
                    VkDeviceSize dstOffset,
                    VkDeviceSize fillSize,
                    uint32_t data)
{
   panvk_stub();
}

void
panvk_CmdUpdateBuffer(VkCommandBuffer commandBuffer,
                      VkBuffer dstBuffer,
                      VkDeviceSize dstOffset,
                      VkDeviceSize dataSize,
                      const void *pData)
{
   panvk_stub();
}

void
panvk_CmdClearColorImage(VkCommandBuffer commandBuffer,
                         VkImage image_h,
                         VkImageLayout imageLayout,
                         const VkClearColorValue *pColor,
                         uint32_t rangeCount,
                         const VkImageSubresourceRange *pRanges)
{
   panvk_stub();
}

void
panvk_CmdClearDepthStencilImage(VkCommandBuffer commandBuffer,
                                VkImage image_h,
                                VkImageLayout imageLayout,
                                const VkClearDepthStencilValue *pDepthStencil,
                                uint32_t rangeCount,
                                const VkImageSubresourceRange *pRanges)
{
   panvk_stub();
}

void
panvk_CmdClearAttachments(VkCommandBuffer commandBuffer,
                          uint32_t attachmentCount,
                          const VkClearAttachment *pAttachments,
                          uint32_t rectCount,
                          const VkClearRect *pRects)
{
   panvk_stub();
}

void
panvk_meta_init(struct panvk_physical_device *dev)
{
   panfrost_pool_init(&dev->meta.bin_pool, NULL, &dev->pdev, PAN_BO_EXECUTE,
                      16 * 1024, "panvk_meta binary pool", false, true);
   panfrost_pool_init(&dev->meta.desc_pool, NULL, &dev->pdev, 0,
                      16 * 1024, "panvk_meta descriptor pool", false, true);
   panvk_meta_copy_img2img_init(dev);
   panvk_meta_copy_buf2img_init(dev);
   panvk_meta_copy_img2buf_init(dev);
   panvk_meta_copy_buf2buf_init(dev);
}

void
panvk_meta_cleanup(struct panvk_physical_device *dev)
{
   panfrost_pool_cleanup(&dev->meta.desc_pool);
   panfrost_pool_cleanup(&dev->meta.bin_pool);
}
