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
#include "pan_blitter.h"
#include "pan_encoder.h"

#include "panvk_private.h"

#include "vk_format.h"

struct panvk_meta_copy_format_info {
   enum pipe_format imgfmt;
   unsigned mask;
};

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

static mali_ptr
panvk_meta_copy_img_emit_texture(struct panfrost_device *pdev,
                                 struct pan_pool *desc_pool,
                                 const struct pan_image_view *view)
{
   if (pan_is_bifrost(pdev)) {
      struct panfrost_ptr texture =
         pan_pool_alloc_desc(desc_pool, BIFROST_TEXTURE);
      size_t payload_size =
         panfrost_estimate_texture_payload_size(pdev, view);
      struct panfrost_ptr surfaces =
         pan_pool_alloc_aligned(desc_pool, payload_size,
                                MALI_SURFACE_WITH_STRIDE_ALIGN);

      panfrost_new_texture(pdev, view, texture.cpu, &surfaces);

      return texture.gpu;
   } else {
      size_t sz = MALI_MIDGARD_TEXTURE_LENGTH +
                  panfrost_estimate_texture_payload_size(pdev, view);
      struct panfrost_ptr texture =
         pan_pool_alloc_aligned(desc_pool, sz, MALI_MIDGARD_TEXTURE_ALIGN);
      struct panfrost_ptr surfaces = {
         .cpu = texture.cpu + MALI_MIDGARD_TEXTURE_LENGTH,
         .gpu = texture.gpu + MALI_MIDGARD_TEXTURE_LENGTH,
      };

      panfrost_new_texture(pdev, view, texture.cpu, &surfaces);

      return pan_pool_upload_aligned(desc_pool, &texture.gpu,
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
         pan_pool_alloc_desc(desc_pool, BIFROST_SAMPLER);

      pan_pack(sampler.cpu, BIFROST_SAMPLER, cfg) {
         cfg.seamless_cube_map = false;
         cfg.normalized_coordinates = false;
         cfg.point_sample_minify = true;
         cfg.point_sample_magnify = true;
      }

      return sampler.gpu;
   } else {
      struct panfrost_ptr sampler =
         pan_pool_alloc_desc(desc_pool, MIDGARD_SAMPLER);

      pan_pack(sampler.cpu, MIDGARD_SAMPLER, cfg) {
         cfg.normalized_coordinates = false;
         cfg.magnify_nearest = true;
         cfg.minify_nearest = true;
      }

      return sampler.gpu;
   }
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
panvk_meta_blit_close_batch(struct panvk_cmd_buffer *cmdbuf)
{
   const struct panfrost_device *pdev =
      &cmdbuf->device->physical_device->pdev;
   struct panvk_batch *batch = cmdbuf->state.batch;

   if (!pan_is_bifrost(pdev) && batch->scoreboard.first_tiler) {
      mali_ptr polygon_list =
         batch->tiler.ctx.midgard.polygon_list->ptr.gpu;
      struct panfrost_ptr writeval_job =
         panfrost_scoreboard_initialize_tiler(&cmdbuf->desc_pool.base,
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
   panvk_stub();
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
   panvk_stub();
}

void
panvk_CmdCopyBufferToImage(VkCommandBuffer commandBuffer,
                           VkBuffer srcBuffer,
                           VkImage destImage,
                           VkImageLayout destImageLayout,
                           uint32_t regionCount,
                           const VkBufferImageCopy *pRegions)
{
   panvk_stub();
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
      pan_pool_upload_aligned(bin_pool, binary.data, binary.size,
                              pan_is_bifrost(pdev) ? 128 : 64);

   util_dynarray_fini(&binary);
   ralloc_free(b.shader);

   return shader;
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
      pan_pool_alloc_desc_aggregate(desc_pool,
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

   return pan_pool_upload_aligned(pool, pushvals, sizeof(pushvals), 16);
}

static mali_ptr
panvk_meta_copy_img2buf_emit_ubo(struct panfrost_device *pdev,
                                 const struct panfrost_ubo_push *pushmap,
                                 struct pan_pool *pool,
                                 const struct panvk_meta_copy_img2buf_info *info)
{
   struct panfrost_ptr ubo = pan_pool_alloc_desc(pool, UNIFORM_BUFFER);

   pan_pack(ubo.cpu, UNIFORM_BUFFER, cfg) {
      cfg.entries = DIV_ROUND_UP(sizeof(*info), 16);
      cfg.pointer = pan_pool_upload_aligned(pool, info, sizeof(*info), 16);
   }

   return ubo.gpu;
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
panvk_meta_copy_img2buf_emit_compute_job(struct pan_pool *desc_pool,
                                         struct pan_scoreboard *scoreboard,
                                         unsigned num_x, unsigned num_y,
                                         unsigned num_z,
                                         mali_ptr texture, mali_ptr sampler,
                                         mali_ptr ubo, mali_ptr push_constants,
                                         mali_ptr rsd, mali_ptr tsd)
{
   struct panfrost_ptr job =
      pan_pool_alloc_desc(desc_pool, COMPUTE_JOB);

   void *invoc = pan_section_ptr(job.cpu,
                                 COMPUTE_JOB,
                                 INVOCATION);
   panfrost_pack_work_groups_compute(invoc, num_x, num_y, num_z,
                                     16, 16, 1, false, false);

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
      panvk_meta_copy_img2buf_emit_push_constants(pdev, pushmap, &cmdbuf->desc_pool.base, &info);
   mali_ptr ubo =
      panvk_meta_copy_img2buf_emit_ubo(pdev, pushmap, &cmdbuf->desc_pool.base, &info);

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
      panvk_meta_copy_img_emit_texture(pdev, &cmdbuf->desc_pool.base, &view);
   mali_ptr sampler =
      panvk_meta_copy_img_emit_sampler(pdev, &cmdbuf->desc_pool.base);

   if (cmdbuf->state.batch)
      panvk_cmd_close_batch(cmdbuf);

   panvk_cmd_open_batch(cmdbuf);

   struct panvk_batch *batch = cmdbuf->state.batch;

   struct pan_tls_info tlsinfo = { 0 };

   batch->blit.src = img->pimage.data.bo;
   batch->blit.dst = buf->bo;
   batch->tls =
      pan_pool_alloc_aligned(&cmdbuf->desc_pool.base,
                             MALI_LOCAL_STORAGE_LENGTH, 64);
   pan_emit_tls(pdev, &tlsinfo, batch->tls.cpu);

   mali_ptr tsd = batch->tls.gpu;

   unsigned num_wg_x = (ALIGN_POT(info.img.extent.maxx + 1, 16) - info.img.offset.x) / 16;
   unsigned num_wg_y = (ALIGN_POT(info.img.extent.maxy + 1, 16) - info.img.offset.y) / 16;
   unsigned num_wg_z = MAX2(region->imageSubresource.layerCount, region->imageExtent.depth);
   struct panfrost_ptr job =
      panvk_meta_copy_img2buf_emit_compute_job(&cmdbuf->desc_pool.base,
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
            panvk_meta_copy_img2buf_emit_rsd(&dev->pdev, &dev->meta.bin_pool.base,
                                        &dev->meta.desc_pool.base,
                                        &dev->meta.copy.img2buf[texdimidx][i].pushmap,
                                        panvk_meta_copy_img2buf_fmts[i], texdim, false);

         if (texdim == 3)
            continue;

         texdimidx = panvk_meta_copy_tex_type(texdim, true);
         assert(texdimidx < ARRAY_SIZE(dev->meta.copy.img2buf));
         dev->meta.copy.img2buf[texdimidx][i].rsd =
            panvk_meta_copy_img2buf_emit_rsd(&dev->pdev, &dev->meta.bin_pool.base,
                                        &dev->meta.desc_pool.base,
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

void
panvk_CmdCopyBuffer(VkCommandBuffer commandBuffer,
                    VkBuffer srcBuffer,
                    VkBuffer destBuffer,
                    uint32_t regionCount,
                    const VkBufferCopy *pRegions)
{
   panvk_stub();
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
                         VkImage image,
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

static mali_ptr
panvk_meta_emit_viewport(struct pan_pool *pool,
                         uint16_t minx, uint16_t miny,
                         uint16_t maxx, uint16_t maxy)
{
   struct panfrost_ptr vp = pan_pool_alloc_desc(pool, VIEWPORT);

   pan_pack(vp.cpu, VIEWPORT, cfg) {
      cfg.scissor_minimum_x = minx;
      cfg.scissor_minimum_y = miny;
      cfg.scissor_maximum_x = maxx;
      cfg.scissor_maximum_y = maxy;
   }

   return vp.gpu;
}

static mali_ptr
panvk_meta_clear_attachments_shader(struct panfrost_device *pdev,
                                    struct pan_pool *bin_pool,
                                    unsigned rt,
                                    enum glsl_base_type base_type,
                                    struct pan_shader_info *shader_info)
{
   nir_builder b = nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,
                                     pan_shader_get_compiler_options(pdev),
                                     "panvk_meta_clear_attachment(base_type=%d,rt=%d)",
                                     base_type,
                                     rt);

   b.shader->info.internal = true;
   b.shader->info.num_ubos = 1;

   const struct glsl_type *out_type = glsl_vector_type(base_type, 4);
   nir_variable *out =
      nir_variable_create(b.shader, nir_var_shader_out, out_type, "out");
   out->data.location = FRAG_RESULT_DATA0 + rt;

   nir_ssa_def *clear_values = nir_load_ubo(&b, 4, 32, nir_imm_int(&b, 0),
                                            nir_imm_int(&b, 0),
                                            .align_mul = 4,
                                            .align_offset = 0,
                                            .range_base = 0,
                                            .range = ~0);
   nir_store_var(&b, out, clear_values, 0xff);

   struct panfrost_compile_inputs inputs = {
      .gpu_id = pdev->gpu_id,
      .is_blit = true,
   };

   struct util_dynarray binary;

   util_dynarray_init(&binary, NULL);
   pan_shader_compile(pdev, b.shader, &inputs, &binary, shader_info);

   /* Make sure UBO words have been upgraded to push constants */
   assert(shader_info->ubo_count == 1);
   assert(shader_info->push.count == 4);

   mali_ptr shader =
      pan_pool_upload_aligned(bin_pool, binary.data, binary.size,
                              pan_is_bifrost(pdev) ? 128 : 64);

   util_dynarray_fini(&binary);
   ralloc_free(b.shader);

   return shader;
}

static mali_ptr
panvk_meta_clear_attachments_emit_rsd(struct panfrost_device *pdev,
                                      struct pan_pool *desc_pool,
                                      enum pipe_format format,
                                      unsigned rt,
                                      struct pan_shader_info *shader_info,
                                      mali_ptr shader)
{
   struct panfrost_ptr rsd_ptr =
      pan_pool_alloc_desc_aggregate(desc_pool,
                                    PAN_DESC(RENDERER_STATE),
                                    PAN_DESC(BLEND));

   unsigned fullmask = (1 << util_format_get_nr_components(format)) - 1;

   /* TODO: Support multiple render targets */
   assert(rt == 0);

   pan_pack(rsd_ptr.cpu, RENDERER_STATE, cfg) {
      pan_shader_prepare_rsd(pdev, shader_info, shader, &cfg);
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
         cfg.properties.bifrost.allow_forward_pixel_to_kill = true;
         cfg.properties.bifrost.zs_update_operation =
            MALI_PIXEL_KILL_STRONG_EARLY;
         cfg.properties.bifrost.pixel_kill_operation =
            MALI_PIXEL_KILL_FORCE_EARLY;
      } else {
         cfg.properties.midgard.shader_reads_tilebuffer = false;
         cfg.properties.midgard.work_register_count = shader_info->work_reg_count;
         cfg.properties.midgard.force_early_z = true;
         cfg.stencil_mask_misc.alpha_test_compare_function = MALI_FUNC_ALWAYS;
      }
   }

   pan_pack(rsd_ptr.cpu + MALI_RENDERER_STATE_LENGTH, BLEND, cfg) {
      cfg.round_to_fb_precision = true;
      cfg.load_destination = false;
      if (pan_is_bifrost(pdev)) {
         cfg.bifrost.internal.mode = MALI_BIFROST_BLEND_MODE_OPAQUE;
         cfg.bifrost.equation.rgb.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.bifrost.equation.rgb.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.bifrost.equation.rgb.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.bifrost.equation.alpha.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.bifrost.equation.alpha.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.bifrost.equation.alpha.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.bifrost.equation.color_mask = 0xf;
         cfg.bifrost.internal.fixed_function.num_comps = 4;
         cfg.bifrost.internal.fixed_function.conversion.memory_format =
            panfrost_format_to_bifrost_blend(pdev, format);
         cfg.bifrost.internal.fixed_function.conversion.register_format =
            shader_info->bifrost.blend[rt].format;
      } else {
         cfg.midgard.equation.rgb.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.midgard.equation.rgb.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.midgard.equation.rgb.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.midgard.equation.alpha.a = MALI_BLEND_OPERAND_A_SRC;
         cfg.midgard.equation.alpha.b = MALI_BLEND_OPERAND_B_SRC;
         cfg.midgard.equation.alpha.c = MALI_BLEND_OPERAND_C_ZERO;
         cfg.midgard.equation.color_mask = fullmask;
      }
   }

   return rsd_ptr.gpu;
}

static mali_ptr
panvk_meta_clear_attachment_emit_push_constants(struct panfrost_device *pdev,
                                                const struct panfrost_ubo_push *pushmap,
                                                struct pan_pool *pool,
                                                const VkClearValue *clear_value)
{
   assert(pushmap->count <= (sizeof(*clear_value) / 4));

   uint32_t *in = (uint32_t *)clear_value;
   uint32_t pushvals[sizeof(*clear_value) / 4];

   for (unsigned i = 0; i < pushmap->count; i++) {
      assert(i < ARRAY_SIZE(pushvals));
      assert(pushmap->words[i].ubo == 0);
      assert(pushmap->words[i].offset < sizeof(*clear_value));
      pushvals[i] = in[pushmap->words[i].offset / 4];
   }

   return pan_pool_upload_aligned(pool, pushvals, sizeof(pushvals), 16);
}

static mali_ptr
panvk_meta_clear_attachment_emit_ubo(struct panfrost_device *pdev,
                                     const struct panfrost_ubo_push *pushmap,
                                     struct pan_pool *pool,
                                     const VkClearValue *clear_value)
{
   struct panfrost_ptr ubo = pan_pool_alloc_desc(pool, UNIFORM_BUFFER);

   pan_pack(ubo.cpu, UNIFORM_BUFFER, cfg) {
      cfg.entries = DIV_ROUND_UP(sizeof(*clear_value), 16);
      cfg.pointer = pan_pool_upload_aligned(pool, clear_value, sizeof(*clear_value), 16);
   }

   return ubo.gpu;
}

static void
panvk_meta_clear_attachment_emit_dcd(struct pan_pool *pool,
                                     mali_ptr coords,
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
      cfg.position = coords;
      cfg.viewport = vpd;
      cfg.texture_descriptor_is_64b = !pan_is_bifrost(pool->dev);
   }
}

static struct panfrost_ptr
panvk_meta_clear_attachment_emit_bifrost_tiler_job(struct pan_pool *desc_pool,
                                                   struct pan_scoreboard *scoreboard,
                                                   mali_ptr coords,
                                                   mali_ptr ubo, mali_ptr push_constants,
                                                   mali_ptr vpd, mali_ptr rsd,
                                                   mali_ptr tsd, mali_ptr tiler)
{
   struct panfrost_ptr job =
      pan_pool_alloc_desc(desc_pool, BIFROST_TILER_JOB);

   panvk_meta_clear_attachment_emit_dcd(desc_pool,
                                        coords,
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
                                     1, 1, 1, 1, true, false);

   pan_section_pack(job.cpu, BIFROST_TILER_JOB, PADDING, cfg);
   pan_section_pack(job.cpu, BIFROST_TILER_JOB, TILER, cfg) {
      cfg.address = tiler;
   }

   panfrost_add_job(desc_pool, scoreboard, MALI_JOB_TYPE_TILER,
                    false, false, 0, 0, &job, false);
   return job;
}

static struct panfrost_ptr
panvk_meta_clear_attachment_emit_midgard_tiler_job(struct pan_pool *desc_pool,
                                                   struct pan_scoreboard *scoreboard,
                                                   mali_ptr coords,
                                                   mali_ptr ubo, mali_ptr push_constants,
                                                   mali_ptr vpd, mali_ptr rsd,
                                                   mali_ptr tsd)
{
   struct panfrost_ptr job =
      pan_pool_alloc_desc(desc_pool, MIDGARD_TILER_JOB);

   panvk_meta_clear_attachment_emit_dcd(desc_pool,
                                        coords,
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
                                     1, 1, 1, 1, true, false);

   panfrost_add_job(desc_pool, scoreboard, MALI_JOB_TYPE_TILER,
                    false, false, 0, 0, &job, false);
   return job;
}

static enum glsl_base_type
panvk_meta_get_format_type(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);
   int i;

   i = util_format_get_first_non_void_channel(format);
   assert(i >= 0);

   if (desc->channel[i].normalized)
      return GLSL_TYPE_FLOAT;

   switch(desc->channel[i].type) {

   case UTIL_FORMAT_TYPE_UNSIGNED:
      return GLSL_TYPE_UINT;

   case UTIL_FORMAT_TYPE_SIGNED:
      return GLSL_TYPE_INT;

   case UTIL_FORMAT_TYPE_FLOAT:
      return GLSL_TYPE_FLOAT;

   default:
      unreachable("Unhandled format");
      return GLSL_TYPE_FLOAT;
   }
}

static void
panvk_meta_clear_attachment(struct panvk_cmd_buffer *cmdbuf,
                            uint32_t attachment,
                            VkImageAspectFlags mask,
                            const VkClearValue *clear_value,
                            const VkClearRect *clear_rect)
{
   struct panvk_physical_device *dev = cmdbuf->device->physical_device;
   struct panfrost_device *pdev = &dev->pdev;
   struct panvk_meta *meta = &cmdbuf->device->physical_device->meta;
   struct panvk_batch *batch = cmdbuf->state.batch;
   const struct panvk_render_pass *pass = cmdbuf->state.pass;
   const struct panvk_render_pass_attachment *att = &pass->attachments[attachment];
   unsigned minx = MAX2(clear_rect->rect.offset.x, 0);
   unsigned miny = MAX2(clear_rect->rect.offset.y, 0);
   unsigned maxx = MAX2(clear_rect->rect.offset.x + clear_rect->rect.extent.width - 1, 0);
   unsigned maxy = MAX2(clear_rect->rect.offset.y + clear_rect->rect.extent.height - 1, 0);

   /* TODO: Support depth/stencil */
   assert(mask == VK_IMAGE_ASPECT_COLOR_BIT);

   panvk_cmd_alloc_fb_desc(cmdbuf);
   panvk_cmd_alloc_tls_desc(cmdbuf);

   if (pan_is_bifrost(pdev)) {
      panvk_cmd_get_bifrost_tiler_context(cmdbuf,
                                          batch->fb.info->width,
                                          batch->fb.info->height);
   } else {
      panvk_cmd_get_midgard_polygon_list(cmdbuf,
                                         batch->fb.info->width,
                                         batch->fb.info->height,
                                         true);
   }

   mali_ptr vpd = panvk_meta_emit_viewport(&cmdbuf->desc_pool.base,
                                           minx, miny, maxx, maxy);

   float rect[] = {
      minx, miny, 0.0, 1.0,
      maxx + 1, miny, 0.0, 1.0,
      minx, maxy + 1, 0.0, 1.0,
      maxx + 1, maxy + 1, 0.0, 1.0,
   };
   mali_ptr coordinates = pan_pool_upload_aligned(&cmdbuf->desc_pool.base,
                                                  rect, sizeof(rect), 64);

   enum glsl_base_type base_type = panvk_meta_get_format_type(att->format);
   mali_ptr shader = meta->clear_attachment[attachment][base_type].shader;
   struct pan_shader_info *shader_info =
      &meta->clear_attachment[attachment][base_type].shader_info;

   mali_ptr rsd =
      panvk_meta_clear_attachments_emit_rsd(pdev,
                                            &cmdbuf->desc_pool.base,
                                            att->format,
                                            attachment,
                                            shader_info,
                                            shader);

   mali_ptr pushconsts =
      panvk_meta_clear_attachment_emit_push_constants(pdev, &shader_info->push,
                                                      &cmdbuf->desc_pool.base,
                                                      clear_value);
   mali_ptr ubo =
      panvk_meta_clear_attachment_emit_ubo(pdev, &shader_info->push,
                                           &cmdbuf->desc_pool.base,
                                           clear_value);

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
      job = panvk_meta_clear_attachment_emit_bifrost_tiler_job(&cmdbuf->desc_pool.base,
                                                               &batch->scoreboard,
                                                               coordinates,
                                                               ubo, pushconsts,
                                                               vpd, rsd, tsd, tiler);
   } else {
      job = panvk_meta_clear_attachment_emit_midgard_tiler_job(&cmdbuf->desc_pool.base,
                                                               &batch->scoreboard,
                                                               coordinates,
                                                               ubo, pushconsts,
                                                               vpd, rsd, tsd);
   }

   util_dynarray_append(&batch->jobs, void *, job.cpu);
}

static void
panvk_meta_clear_attachment_init(struct panvk_physical_device *dev)
{
   for (unsigned rt = 0; rt < MAX_RTS; rt++) {
      dev->meta.clear_attachment[rt][GLSL_TYPE_UINT].shader =
         panvk_meta_clear_attachments_shader(
               &dev->pdev,
               &dev->meta.bin_pool.base,
               rt,
               GLSL_TYPE_UINT,
               &dev->meta.clear_attachment[rt][GLSL_TYPE_UINT].shader_info);

      dev->meta.clear_attachment[rt][GLSL_TYPE_INT].shader =
         panvk_meta_clear_attachments_shader(
               &dev->pdev,
               &dev->meta.bin_pool.base,
               rt,
               GLSL_TYPE_INT,
               &dev->meta.clear_attachment[rt][GLSL_TYPE_INT].shader_info);

      dev->meta.clear_attachment[rt][GLSL_TYPE_FLOAT].shader =
         panvk_meta_clear_attachments_shader(
               &dev->pdev,
               &dev->meta.bin_pool.base,
               rt,
               GLSL_TYPE_FLOAT,
               &dev->meta.clear_attachment[rt][GLSL_TYPE_FLOAT].shader_info);
   }
}

void
panvk_CmdClearAttachments(VkCommandBuffer commandBuffer,
                          uint32_t attachmentCount,
                          const VkClearAttachment *pAttachments,
                          uint32_t rectCount,
                          const VkClearRect *pRects)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   const struct panvk_subpass *subpass = cmdbuf->state.subpass;

   for (unsigned i = 0; i < attachmentCount; i++) {
      for (unsigned j = 0; j < rectCount; j++) {

         uint32_t attachment;
         if (pAttachments[i].aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
            unsigned idx = pAttachments[i].colorAttachment;
            attachment = subpass->color_attachments[idx].idx;
         } else {
            attachment = subpass->zs_attachment.idx;
         }

         if (attachment == VK_ATTACHMENT_UNUSED)
               continue;

         panvk_meta_clear_attachment(cmdbuf, attachment,
                                     pAttachments[i].aspectMask,
                                     &pAttachments[i].clearValue,
                                     &pRects[j]);
      }
   }
}

void
panvk_meta_init(struct panvk_physical_device *dev)
{
   panvk_pool_init(&dev->meta.bin_pool, &dev->pdev, NULL, PAN_BO_EXECUTE,
                   16 * 1024, "panvk_meta binary pool", false);
   panvk_pool_init(&dev->meta.desc_pool, &dev->pdev, NULL, 0,
                   16 * 1024, "panvk_meta descriptor pool", false);
   panvk_pool_init(&dev->meta.blitter.bin_pool, &dev->pdev, NULL,
                   PAN_BO_EXECUTE, 16 * 1024,
                   "panvk_meta blitter binary pool", false);
   panvk_pool_init(&dev->meta.blitter.desc_pool, &dev->pdev, NULL,
                   0, 16 * 1024, "panvk_meta blitter descriptor pool",
                   false);
   pan_blitter_init(&dev->pdev, &dev->meta.blitter.bin_pool.base,
                    &dev->meta.blitter.desc_pool.base);
   panvk_meta_clear_attachment_init(dev);
   panvk_meta_copy_img2buf_init(dev);
}

void
panvk_meta_cleanup(struct panvk_physical_device *dev)
{
   pan_blitter_cleanup(&dev->pdev);
   panvk_pool_cleanup(&dev->meta.blitter.desc_pool);
   panvk_pool_cleanup(&dev->meta.blitter.bin_pool);
   panvk_pool_cleanup(&dev->meta.desc_pool);
   panvk_pool_cleanup(&dev->meta.bin_pool);
}
