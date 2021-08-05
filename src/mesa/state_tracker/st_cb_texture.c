/**************************************************************************
 *
 * Copyright 2007 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include <stdio.h>
#include "main/bufferobj.h"
#include "main/enums.h"
#include "main/errors.h"
#include "main/fbobject.h"
#include "main/formats.h"
#include "main/format_utils.h"
#include "main/glformats.h"
#include "main/image.h"

#include "main/macros.h"
#include "main/mipmap.h"
#include "main/pack.h"
#include "main/pbo.h"
#include "main/pixeltransfer.h"
#include "main/texcompress.h"
#include "main/texcompress_astc.h"
#include "main/texcompress_etc.h"
#include "main/texgetimage.h"
#include "main/teximage.h"
#include "main/texobj.h"
#include "main/texstore.h"

#include "state_tracker/st_debug.h"
#include "state_tracker/st_context.h"
#include "state_tracker/st_cb_bitmap.h"
#include "state_tracker/st_cb_drawpixels.h"
#include "state_tracker/st_cb_fbo.h"
#include "state_tracker/st_cb_flush.h"
#include "state_tracker/st_cb_texture.h"
#include "state_tracker/st_cb_bufferobjects.h"
#include "state_tracker/st_cb_memoryobjects.h"
#include "state_tracker/st_format.h"
#include "state_tracker/st_nir.h"
#include "state_tracker/st_pbo.h"
#include "state_tracker/st_texture.h"
#include "state_tracker/st_gen_mipmap.h"
#include "state_tracker/st_atom.h"
#include "state_tracker/st_sampler_view.h"
#include "state_tracker/st_util.h"

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "util/u_inlines.h"
#include "util/u_upload_mgr.h"
#include "pipe/p_shader_tokens.h"
#include "util/u_tile.h"
#include "util/format/u_format.h"
#include "util/u_surface.h"
#include "util/u_sampler.h"
#include "util/u_math.h"
#include "util/u_box.h"
#include "util/u_simple_shaders.h"
#include "cso_cache/cso_context.h"
#include "tgsi/tgsi_ureg.h"

#include "compiler/nir/nir_builder.h"
#include "compiler/nir/nir_format_convert.h"
#include "compiler/glsl/gl_nir.h"

#define DBG if (0) printf


enum pipe_texture_target
gl_target_to_pipe(GLenum target)
{
   switch (target) {
   case GL_TEXTURE_1D:
   case GL_PROXY_TEXTURE_1D:
      return PIPE_TEXTURE_1D;
   case GL_TEXTURE_2D:
   case GL_PROXY_TEXTURE_2D:
   case GL_TEXTURE_EXTERNAL_OES:
   case GL_TEXTURE_2D_MULTISAMPLE:
   case GL_PROXY_TEXTURE_2D_MULTISAMPLE:
      return PIPE_TEXTURE_2D;
   case GL_TEXTURE_RECTANGLE_NV:
   case GL_PROXY_TEXTURE_RECTANGLE_NV:
      return PIPE_TEXTURE_RECT;
   case GL_TEXTURE_3D:
   case GL_PROXY_TEXTURE_3D:
      return PIPE_TEXTURE_3D;
   case GL_TEXTURE_CUBE_MAP_ARB:
   case GL_PROXY_TEXTURE_CUBE_MAP_ARB:
   case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
   case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
   case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
   case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
   case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
   case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return PIPE_TEXTURE_CUBE;
   case GL_TEXTURE_1D_ARRAY_EXT:
   case GL_PROXY_TEXTURE_1D_ARRAY_EXT:
      return PIPE_TEXTURE_1D_ARRAY;
   case GL_TEXTURE_2D_ARRAY_EXT:
   case GL_PROXY_TEXTURE_2D_ARRAY_EXT:
   case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
   case GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY:
      return PIPE_TEXTURE_2D_ARRAY;
   case GL_TEXTURE_BUFFER:
      return PIPE_BUFFER;
   case GL_TEXTURE_CUBE_MAP_ARRAY:
   case GL_PROXY_TEXTURE_CUBE_MAP_ARRAY:
      return PIPE_TEXTURE_CUBE_ARRAY;
   default:
      assert(0);
      return 0;
   }
}

static enum pipe_format
get_src_format(struct pipe_screen *screen, enum pipe_format src_format, struct pipe_resource *src)
{
   /* Convert the source format to what is expected by GetTexImage
    * and see if it's supported.
    *
    * This only applies to glGetTexImage:
    * - Luminance must be returned as (L,0,0,1).
    * - Luminance alpha must be returned as (L,0,0,A).
    * - Intensity must be returned as (I,0,0,1)
    */
   src_format = util_format_linear(src_format);
   src_format = util_format_luminance_to_red(src_format);
   src_format = util_format_intensity_to_red(src_format);

   if (!src_format ||
       !screen->is_format_supported(screen, src_format, src->target,
                                    src->nr_samples, src->nr_storage_samples,
                                    PIPE_BIND_SAMPLER_VIEW)) {
      return PIPE_FORMAT_NONE;
   }
   return src_format;
}

static struct pipe_resource *
create_dst_texture(struct gl_context *ctx,
                   enum pipe_format dst_format, enum pipe_texture_target pipe_target,
                   GLsizei width, GLsizei height, GLint depth,
                   GLenum gl_target, unsigned bind)
{
   struct st_context *st = st_context(ctx);
   struct pipe_screen *screen = st->screen;
   struct pipe_resource dst_templ;

   /* create the destination texture of size (width X height X depth) */
   memset(&dst_templ, 0, sizeof(dst_templ));
   dst_templ.target = pipe_target;
   dst_templ.format = dst_format;
   dst_templ.bind = bind;
   dst_templ.usage = PIPE_USAGE_STAGING;

   st_gl_texture_dims_to_pipe_dims(gl_target, width, height, depth,
                                   &dst_templ.width0, &dst_templ.height0,
                                   &dst_templ.depth0, &dst_templ.array_size);

   return screen->resource_create(screen, &dst_templ);
}

static boolean
copy_to_staging_dest(struct gl_context * ctx, struct pipe_resource *dst,
                 GLint xoffset, GLint yoffset, GLint zoffset,
                 GLsizei width, GLsizei height, GLint depth,
                 GLenum format, GLenum type, void * pixels,
                 struct gl_texture_image *texImage)
{
   struct st_context *st = st_context(ctx);
   struct pipe_context *pipe = st->pipe;
   struct st_texture_object *stObj = st_texture_object(texImage->TexObject);
   struct pipe_resource *src = stObj->pt;
   enum pipe_format dst_format = dst->format;
   mesa_format mesa_format;
   GLenum gl_target = texImage->TexObject->Target;
   unsigned dims;
   struct pipe_transfer *tex_xfer;
   ubyte *map = NULL;
   boolean done = FALSE;

   pixels = _mesa_map_pbo_dest(ctx, &ctx->Pack, pixels);

   map = pipe_texture_map_3d(pipe, dst, 0, PIPE_MAP_READ,
                              0, 0, 0, width, height, depth, &tex_xfer);
   if (!map) {
      goto end;
   }

   mesa_format = st_pipe_format_to_mesa_format(dst_format);
   dims = _mesa_get_texture_dimensions(gl_target);

   /* copy/pack data into user buffer */
   if (_mesa_format_matches_format_and_type(mesa_format, format, type,
                                            ctx->Pack.SwapBytes, NULL)) {
      /* memcpy */
      const uint bytesPerRow = width * util_format_get_blocksize(dst_format);
      GLuint row, slice;

      for (slice = 0; slice < depth; slice++) {
         ubyte *slice_map = map;

         for (row = 0; row < height; row++) {
            void *dest = _mesa_image_address(dims, &ctx->Pack, pixels,
                                             width, height, format, type,
                                             slice, row, 0);

            memcpy(dest, slice_map, bytesPerRow);

            slice_map += tex_xfer->stride;
         }

         map += tex_xfer->layer_stride;
      }
   }
   else {
      /* format translation via floats */
      GLuint slice;
      GLfloat *rgba;
      uint32_t dstMesaFormat;
      int dstStride, srcStride;

      assert(util_format_is_compressed(src->format));

      rgba = malloc(width * height * 4 * sizeof(GLfloat));
      if (!rgba) {
         goto end;
      }

      if (ST_DEBUG & DEBUG_FALLBACK)
         debug_printf("%s: fallback format translation\n", __func__);

      dstMesaFormat = _mesa_format_from_format_and_type(format, type);
      dstStride = _mesa_image_row_stride(&ctx->Pack, width, format, type);
      srcStride = 4 * width * sizeof(GLfloat);
      for (slice = 0; slice < depth; slice++) {
         void *dest = _mesa_image_address(dims, &ctx->Pack, pixels,
                                          width, height, format, type,
                                          slice, 0, 0);

         /* get float[4] rgba row from surface */
         pipe_get_tile_rgba(tex_xfer, map, 0, 0, width, height, dst_format,
                            rgba);

         _mesa_format_convert(dest, dstMesaFormat, dstStride,
                              rgba, RGBA32_FLOAT, srcStride,
                              width, height, NULL);

         /* Handle byte swapping if required */
         if (ctx->Pack.SwapBytes) {
            _mesa_swap_bytes_2d_image(format, type, &ctx->Pack,
                                      width, height, dest, dest);
         }

         map += tex_xfer->layer_stride;
      }

      free(rgba);
   }
   done = TRUE;

end:
   if (map)
      pipe_texture_unmap(pipe, tex_xfer);

   _mesa_unmap_pbo_dest(ctx, &ctx->Pack);
   return done;
}

static enum pipe_format
get_dst_format(struct gl_context *ctx, enum pipe_texture_target target,
               enum pipe_format src_format, bool is_compressed,
               GLenum format, GLenum type, unsigned bind)
{
   struct st_context *st = st_context(ctx);
   struct pipe_screen *screen = st->screen;
   /* Choose the destination format by finding the best match
    * for the format+type combo. */
   enum pipe_format dst_format = st_choose_matching_format(st, bind, format, type,
                                                           ctx->Pack.SwapBytes);

   if (dst_format == PIPE_FORMAT_NONE) {
      GLenum dst_glformat;

      /* Fall back to _mesa_GetTexImage_sw except for compressed formats,
       * where decompression with a blit is always preferred. */
      if (!is_compressed) {
         return PIPE_FORMAT_NONE;
      }

      /* Set the appropriate format for the decompressed texture.
       * Luminance and sRGB formats shouldn't appear here.*/
      switch (src_format) {
      case PIPE_FORMAT_DXT1_RGB:
      case PIPE_FORMAT_DXT1_RGBA:
      case PIPE_FORMAT_DXT3_RGBA:
      case PIPE_FORMAT_DXT5_RGBA:
      case PIPE_FORMAT_RGTC1_UNORM:
      case PIPE_FORMAT_RGTC2_UNORM:
      case PIPE_FORMAT_ETC1_RGB8:
      case PIPE_FORMAT_ETC2_RGB8:
      case PIPE_FORMAT_ETC2_RGB8A1:
      case PIPE_FORMAT_ETC2_RGBA8:
      case PIPE_FORMAT_ASTC_4x4:
      case PIPE_FORMAT_ASTC_5x4:
      case PIPE_FORMAT_ASTC_5x5:
      case PIPE_FORMAT_ASTC_6x5:
      case PIPE_FORMAT_ASTC_6x6:
      case PIPE_FORMAT_ASTC_8x5:
      case PIPE_FORMAT_ASTC_8x6:
      case PIPE_FORMAT_ASTC_8x8:
      case PIPE_FORMAT_ASTC_10x5:
      case PIPE_FORMAT_ASTC_10x6:
      case PIPE_FORMAT_ASTC_10x8:
      case PIPE_FORMAT_ASTC_10x10:
      case PIPE_FORMAT_ASTC_12x10:
      case PIPE_FORMAT_ASTC_12x12:
      case PIPE_FORMAT_BPTC_RGBA_UNORM:
      case PIPE_FORMAT_FXT1_RGB:
      case PIPE_FORMAT_FXT1_RGBA:
         dst_glformat = GL_RGBA8;
         break;
      case PIPE_FORMAT_RGTC1_SNORM:
      case PIPE_FORMAT_RGTC2_SNORM:
         if (!ctx->Extensions.EXT_texture_snorm)
            return PIPE_FORMAT_NONE;
         dst_glformat = GL_RGBA8_SNORM;
         break;
      case PIPE_FORMAT_BPTC_RGB_FLOAT:
      case PIPE_FORMAT_BPTC_RGB_UFLOAT:
         if (!ctx->Extensions.ARB_texture_float)
            return PIPE_FORMAT_NONE;
         dst_glformat = GL_RGBA32F;
         break;
      case PIPE_FORMAT_ETC2_R11_UNORM:
         if (bind && !screen->is_format_supported(screen, PIPE_FORMAT_R16_UNORM,
                                                  target, 0, 0, bind))
            return PIPE_FORMAT_NONE;
         dst_glformat = GL_R16;
         break;
      case PIPE_FORMAT_ETC2_R11_SNORM:
         if (bind && !screen->is_format_supported(screen, PIPE_FORMAT_R16_SNORM,
                                                  target, 0, 0, bind))
            return PIPE_FORMAT_NONE;
         dst_glformat = GL_R16_SNORM;
         break;
      case PIPE_FORMAT_ETC2_RG11_UNORM:
         if (bind && !screen->is_format_supported(screen, PIPE_FORMAT_R16G16_UNORM,
                                                  target, 0, 0, bind))
            return PIPE_FORMAT_NONE;
         dst_glformat = GL_RG16;
         break;
      case PIPE_FORMAT_ETC2_RG11_SNORM:
         if (bind && !screen->is_format_supported(screen, PIPE_FORMAT_R16G16_SNORM,
                                                  target, 0, 0, bind))
            return PIPE_FORMAT_NONE;
         dst_glformat = GL_RG16_SNORM;
         break;
      default:
         assert(0);
         return PIPE_FORMAT_NONE;
      }

      dst_format = st_choose_format(st, dst_glformat, format, type,
                                    target, 0, 0, bind,
                                    false, false);
   }
   return dst_format;
}

#define BGR_FORMAT(NAME) \
    {{ \
     [0] = PIPE_FORMAT_##NAME##_SNORM, \
     [1] = PIPE_FORMAT_##NAME##_SINT, \
    }, \
    { \
     [0] = PIPE_FORMAT_##NAME##_UNORM, \
     [1] = PIPE_FORMAT_##NAME##_UINT, \
    }}

#define FORMAT(NAME, NAME16, NAME32) \
   {{ \
    [1] = PIPE_FORMAT_##NAME##_SNORM, \
    [2] = PIPE_FORMAT_##NAME16##_SNORM, \
    [4] = PIPE_FORMAT_##NAME32##_SNORM, \
   }, \
   { \
    [1] = PIPE_FORMAT_##NAME##_UNORM, \
    [2] = PIPE_FORMAT_##NAME16##_UNORM, \
    [4] = PIPE_FORMAT_##NAME32##_UNORM, \
   }}

/* don't try these at home */
static enum pipe_format
get_hack_format(struct gl_context *ctx,
                enum pipe_format src_format,
                GLenum format, GLenum type,
                bool *need_bgra_swizzle)
{
   struct st_context *st = st_context(ctx);
   GLint bpp = _mesa_bytes_per_pixel(format, type);
   if (_mesa_is_depth_format(format) ||
       format == GL_GREEN_INTEGER ||
       format == GL_BLUE_INTEGER) {
      switch (bpp) {
      case 1:
         return _mesa_is_type_unsigned(type) ? PIPE_FORMAT_R8_UINT : PIPE_FORMAT_R8_SINT;
      case 2:
         return _mesa_is_type_unsigned(type) ? PIPE_FORMAT_R16_UINT : PIPE_FORMAT_R16_SINT;
      case 4:
         return _mesa_is_type_unsigned(type) ? PIPE_FORMAT_R32_UINT : PIPE_FORMAT_R32_SINT;
      }
   }
   mesa_format mformat = _mesa_tex_format_from_format_and_type(ctx, format, type);
   enum pipe_format pformat = st_mesa_format_to_pipe_format(st, mformat);
   if (!pformat) {
      GLint dst_components = _mesa_components_in_format(format);
      bpp /= dst_components;
      if (format == GL_BGR || format == GL_BGRA) {
            pformat = get_dst_format(ctx, PIPE_TEXTURE_2D, src_format, false, format == GL_BGR ? GL_RGB : GL_RGBA, type, 0);
            if (!pformat)
               pformat = get_hack_format(ctx, src_format, format == GL_BGR ? GL_RGB : GL_RGBA, type, need_bgra_swizzle);
            assert(pformat);
            *need_bgra_swizzle = true;
      } else if (format == GL_BGR_INTEGER || format == GL_BGRA_INTEGER) {
            pformat = get_dst_format(ctx, PIPE_TEXTURE_2D, src_format, false, format == GL_BGR_INTEGER ? GL_RGB_INTEGER : GL_RGBA_INTEGER, type, 0);
            if (!pformat)
               pformat = get_hack_format(ctx, src_format, format == GL_BGR_INTEGER ? GL_RGB_INTEGER : GL_RGBA_INTEGER, type, need_bgra_swizzle);
            assert(pformat);
            *need_bgra_swizzle = true;
      } else {
         /* [signed,unsigned][bpp] */
         enum pipe_format rgb[5][2][5] = {
            [1] = FORMAT(R8, R16, R32),
            [2] = FORMAT(R8G8, R16G16, R32G32),
            [3] = FORMAT(R8G8B8, R16G16B16, R32G32B32),
            [4] = FORMAT(R8G8B8A8, R16G16B16A16, R32G32B32A32),
         };
         pformat = rgb[dst_components][_mesa_is_type_unsigned(type)][bpp];
      }
      assert(util_format_get_nr_components(pformat) == dst_components);
   }
   assert(pformat);
   return pformat;
}
#undef BGR_FORMAT
#undef FORMAT

/** called via ctx->Driver.NewTextureImage() */
static struct gl_texture_image *
st_NewTextureImage(struct gl_context * ctx)
{
   DBG("%s\n", __func__);
   (void) ctx;
   return (struct gl_texture_image *) ST_CALLOC_STRUCT(st_texture_image);
}


/** called via ctx->Driver.DeleteTextureImage() */
static void
st_DeleteTextureImage(struct gl_context * ctx, struct gl_texture_image *img)
{
   /* nothing special (yet) for st_texture_image */
   _mesa_delete_texture_image(ctx, img);
}


/** called via ctx->Driver.NewTextureObject() */
static struct gl_texture_object *
st_NewTextureObject(struct gl_context * ctx, GLuint name, GLenum target)
{
   struct st_texture_object *obj = ST_CALLOC_STRUCT(st_texture_object);
   if (!obj)
      return NULL;

   obj->level_override = -1;
   obj->layer_override = -1;

   /* Pre-allocate a sampler views container to save a branch in the
    * fast path.
    */
   obj->sampler_views = calloc(1, sizeof(struct st_sampler_views)
                               + sizeof(struct st_sampler_view));
   if (!obj->sampler_views) {
      free(obj);
      return NULL;
   }
   obj->sampler_views->max = 1;

   DBG("%s\n", __func__);
   _mesa_initialize_texture_object(ctx, &obj->base, name, target);

   simple_mtx_init(&obj->validate_mutex, mtx_plain);
   obj->needs_validation = true;

   return &obj->base;
}


/** called via ctx->Driver.DeleteTextureObject() */
static void
st_DeleteTextureObject(struct gl_context *ctx,
                       struct gl_texture_object *texObj)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_object *stObj = st_texture_object(texObj);

   pipe_resource_reference(&stObj->pt, NULL);
   st_delete_texture_sampler_views(st, stObj);
   simple_mtx_destroy(&stObj->validate_mutex);
   _mesa_delete_texture_object(ctx, texObj);
}

/**
 * Called via ctx->Driver.TextureRemovedFromShared()
 * When texture is removed from ctx->Shared->TexObjects we lose
 * the ability to clean up views on context destruction, which may
 * lead to dangling pointers to destroyed contexts.
 * Release the views to prevent this.
 */
static void
st_TextureReleaseAllSamplerViews(struct gl_context *ctx,
                                 struct gl_texture_object *texObj)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_object *stObj = st_texture_object(texObj);

   st_texture_release_all_sampler_views(st, stObj);
}

/** called via ctx->Driver.FreeTextureImageBuffer() */
static void
st_FreeTextureImageBuffer(struct gl_context *ctx,
                          struct gl_texture_image *texImage)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_object *stObj = st_texture_object(texImage->TexObject);
   struct st_texture_image *stImage = st_texture_image(texImage);

   DBG("%s\n", __func__);

   if (stImage->pt) {
      pipe_resource_reference(&stImage->pt, NULL);
   }

   free(stImage->transfer);
   stImage->transfer = NULL;
   stImage->num_transfers = 0;

   if (stImage->compressed_data &&
       pipe_reference(&stImage->compressed_data->reference, NULL)) {
      free(stImage->compressed_data->ptr);
      free(stImage->compressed_data);
      stImage->compressed_data = NULL;
   }

   /* if the texture image is being deallocated, the structure of the
    * texture is changing so we'll likely need a new sampler view.
    */
   st_texture_release_all_sampler_views(st, stObj);
}

bool
st_astc_format_fallback(const struct st_context *st, mesa_format format)
{
   if (!_mesa_is_format_astc_2d(format))
      return false;

   if (format == MESA_FORMAT_RGBA_ASTC_5x5 ||
       format == MESA_FORMAT_SRGB8_ALPHA8_ASTC_5x5)
      return !st->has_astc_5x5_ldr;

   return !st->has_astc_2d_ldr;
}

bool
st_compressed_format_fallback(struct st_context *st, mesa_format format)
{
   if (format == MESA_FORMAT_ETC1_RGB8)
      return !st->has_etc1;

   if (_mesa_is_format_etc2(format))
      return !st->has_etc2;

   if (st_astc_format_fallback(st, format))
      return true;

   return false;
}


static void
compressed_tex_fallback_allocate(struct st_context *st,
                                 struct st_texture_image *stImage)
{
   struct gl_texture_image *texImage = &stImage->base;

   if (!st_compressed_format_fallback(st, texImage->TexFormat))
      return;

   if (stImage->compressed_data &&
       pipe_reference(&stImage->compressed_data->reference, NULL)) {
      free(stImage->compressed_data->ptr);
      free(stImage->compressed_data);
   }

   unsigned data_size = _mesa_format_image_size(texImage->TexFormat,
                                                texImage->Width2,
                                                texImage->Height2,
                                                texImage->Depth2);

   stImage->compressed_data = ST_CALLOC_STRUCT(st_compressed_data);
   stImage->compressed_data->ptr =
      malloc(data_size * _mesa_num_tex_faces(texImage->TexObject->Target));
   pipe_reference_init(&stImage->compressed_data->reference, 1);
}


/** called via ctx->Driver.MapTextureImage() */
static void
st_MapTextureImage(struct gl_context *ctx,
                   struct gl_texture_image *texImage,
                   GLuint slice, GLuint x, GLuint y, GLuint w, GLuint h,
                   GLbitfield mode,
                   GLubyte **mapOut, GLint *rowStrideOut)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_image *stImage = st_texture_image(texImage);
   GLubyte *map;
   struct pipe_transfer *transfer;

   /* Check for unexpected flags */
   assert((mode & ~(GL_MAP_READ_BIT |
                    GL_MAP_WRITE_BIT |
                    GL_MAP_INVALIDATE_RANGE_BIT)) == 0);

   const enum pipe_map_flags transfer_flags =
      st_access_flags_to_transfer_flags(mode, false);

   map = st_texture_image_map(st, stImage, transfer_flags, x, y, slice, w, h, 1,
                              &transfer);
   if (map) {
      if (st_compressed_format_fallback(st, texImage->TexFormat)) {
         /* Some compressed formats don't have to be supported by drivers,
          * and st/mesa transparently handles decompression on upload (Unmap),
          * so that drivers don't see the compressed formats.
          *
          * We store the compressed data (it's needed for glGetCompressedTex-
          * Image and image copies in OES_copy_image).
          */
         unsigned z = transfer->box.z;
         struct st_texture_image_transfer *itransfer = &stImage->transfer[z];

         unsigned blk_w, blk_h;
         _mesa_get_format_block_size(texImage->TexFormat, &blk_w, &blk_h);

         unsigned y_blocks = DIV_ROUND_UP(texImage->Height2, blk_h);
         unsigned stride = *rowStrideOut = itransfer->temp_stride =
            _mesa_format_row_stride(texImage->TexFormat, texImage->Width2);
         unsigned block_size = _mesa_get_format_bytes(texImage->TexFormat);

         assert(stImage->compressed_data);
         *mapOut = itransfer->temp_data =
            stImage->compressed_data->ptr +
            (z * y_blocks + (y / blk_h)) * stride +
            (x / blk_w) * block_size;
         itransfer->map = map;
      }
      else {
         /* supported mapping */
         *mapOut = map;
         *rowStrideOut = transfer->stride;
      }
   }
   else {
      *mapOut = NULL;
      *rowStrideOut = 0;
   }
}


/** called via ctx->Driver.UnmapTextureImage() */
static void
st_UnmapTextureImage(struct gl_context *ctx,
                     struct gl_texture_image *texImage,
                     GLuint slice)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_image *stImage  = st_texture_image(texImage);

   if (st_compressed_format_fallback(st, texImage->TexFormat)) {
      /* Decompress the compressed image on upload if the driver doesn't
       * support the compressed format. */
      unsigned z = slice + stImage->base.Face;
      struct st_texture_image_transfer *itransfer = &stImage->transfer[z];
      struct pipe_transfer *transfer = itransfer->transfer;

      assert(z == transfer->box.z);

      if (transfer->usage & PIPE_MAP_WRITE) {
         if (util_format_is_compressed(stImage->pt->format)) {
            /* Transcode into a different compressed format. */
            unsigned size =
               _mesa_format_image_size(PIPE_FORMAT_R8G8B8A8_UNORM,
                                       transfer->box.width,
                                       transfer->box.height, 1);
            void *tmp = malloc(size);

            /* Decompress to tmp. */
            if (texImage->TexFormat == MESA_FORMAT_ETC1_RGB8) {
               _mesa_etc1_unpack_rgba8888(tmp, transfer->box.width * 4,
                                          itransfer->temp_data,
                                          itransfer->temp_stride,
                                          transfer->box.width,
                                          transfer->box.height);
            } else if (_mesa_is_format_etc2(texImage->TexFormat)) {
               bool bgra = stImage->pt->format == PIPE_FORMAT_B8G8R8A8_SRGB;

               _mesa_unpack_etc2_format(tmp, transfer->box.width * 4,
                                        itransfer->temp_data,
                                        itransfer->temp_stride,
                                        transfer->box.width,
                                        transfer->box.height,
                                        texImage->TexFormat,
                                        bgra);
            } else if (_mesa_is_format_astc_2d(texImage->TexFormat)) {
               _mesa_unpack_astc_2d_ldr(tmp, transfer->box.width * 4,
                                        itransfer->temp_data,
                                        itransfer->temp_stride,
                                        transfer->box.width,
                                        transfer->box.height,
                                        texImage->TexFormat);
            } else {
               unreachable("unexpected format for a compressed format fallback");
            }

            /* Compress it to the target format. */
            struct gl_pixelstore_attrib pack = {0};
            pack.Alignment = 4;

            _mesa_texstore(ctx, 2, GL_RGBA, stImage->pt->format,
                           transfer->stride, &itransfer->map,
                           transfer->box.width,
                           transfer->box.height, 1, GL_RGBA,
                           GL_UNSIGNED_BYTE, tmp, &pack);
            free(tmp);
         } else {
            /* Decompress into an uncompressed format. */
            if (texImage->TexFormat == MESA_FORMAT_ETC1_RGB8) {
               _mesa_etc1_unpack_rgba8888(itransfer->map, transfer->stride,
                                          itransfer->temp_data,
                                          itransfer->temp_stride,
                                          transfer->box.width,
                                          transfer->box.height);
            } else if (_mesa_is_format_etc2(texImage->TexFormat)) {
               bool bgra = stImage->pt->format == PIPE_FORMAT_B8G8R8A8_SRGB;

               _mesa_unpack_etc2_format(itransfer->map, transfer->stride,
                                        itransfer->temp_data,
                                        itransfer->temp_stride,
                                        transfer->box.width, transfer->box.height,
                                        texImage->TexFormat,
                                        bgra);
            } else if (_mesa_is_format_astc_2d(texImage->TexFormat)) {
               _mesa_unpack_astc_2d_ldr(itransfer->map, transfer->stride,
                                        itransfer->temp_data,
                                        itransfer->temp_stride,
                                        transfer->box.width, transfer->box.height,
                                        texImage->TexFormat);
            } else {
               unreachable("unexpected format for a compressed format fallback");
            }
         }
      }

      itransfer->temp_data = NULL;
      itransfer->temp_stride = 0;
      itransfer->map = 0;
   }

   st_texture_image_unmap(st, stImage, slice);
}


/**
 * Return default texture resource binding bitmask for the given format.
 */
static GLuint
default_bindings(struct st_context *st, enum pipe_format format)
{
   struct pipe_screen *screen = st->screen;
   const unsigned target = PIPE_TEXTURE_2D;
   unsigned bindings;

   if (util_format_is_depth_or_stencil(format))
      bindings = PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_DEPTH_STENCIL;
   else
      bindings = PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET;

   if (screen->is_format_supported(screen, format, target, 0, 0, bindings))
      return bindings;
   else {
      /* Try non-sRGB. */
      format = util_format_linear(format);

      if (screen->is_format_supported(screen, format, target, 0, 0, bindings))
         return bindings;
      else
         return PIPE_BIND_SAMPLER_VIEW;
   }
}


/**
 * Given the size of a mipmap image, try to compute the size of the level=0
 * mipmap image.
 *
 * Note that this isn't always accurate for odd-sized, non-POW textures.
 * For example, if level=1 and width=40 then the level=0 width may be 80 or 81.
 *
 * \return GL_TRUE for success, GL_FALSE for failure
 */
static GLboolean
guess_base_level_size(GLenum target,
                      GLuint width, GLuint height, GLuint depth, GLuint level,
                      GLuint *width0, GLuint *height0, GLuint *depth0)
{
   assert(width >= 1);
   assert(height >= 1);
   assert(depth >= 1);

   if (level > 0) {
      /* Guess the size of the base level.
       * Depending on the image's size, we can't always make a guess here.
       */
      switch (target) {
      case GL_TEXTURE_1D:
      case GL_TEXTURE_1D_ARRAY:
         width <<= level;
         break;

      case GL_TEXTURE_2D:
      case GL_TEXTURE_2D_ARRAY:
         /* We can't make a good guess here, because the base level dimensions
          * can be non-square.
          */
         if (width == 1 || height == 1) {
            return GL_FALSE;
         }
         width <<= level;
         height <<= level;
         break;

      case GL_TEXTURE_CUBE_MAP:
      case GL_TEXTURE_CUBE_MAP_ARRAY:
         width <<= level;
         height <<= level;
         break;

      case GL_TEXTURE_3D:
         /* We can't make a good guess here, because the base level dimensions
          * can be non-cube.
          */
         if (width == 1 || height == 1 || depth == 1) {
            return GL_FALSE;
         }
         width <<= level;
         height <<= level;
         depth <<= level;
         break;

      case GL_TEXTURE_RECTANGLE:
         break;

      default:
         assert(0);
      }
   }

   *width0 = width;
   *height0 = height;
   *depth0 = depth;

   return GL_TRUE;
}


/**
 * Try to determine whether we should allocate memory for a full texture
 * mipmap.  The problem is when we get a glTexImage(level=0) call, we
 * can't immediately know if other mipmap levels are coming next.  Here
 * we try to guess whether to allocate memory for a mipmap or just the
 * 0th level.
 *
 * If we guess incorrectly here we'll later reallocate the right amount of
 * memory either in st_AllocTextureImageBuffer() or st_finalize_texture().
 *
 * \param stObj  the texture object we're going to allocate memory for.
 * \param stImage  describes the incoming image which we need to store.
 */
static boolean
allocate_full_mipmap(const struct st_texture_object *stObj,
                     const struct st_texture_image *stImage)
{
   switch (stObj->base.Target) {
   case GL_TEXTURE_RECTANGLE_NV:
   case GL_TEXTURE_BUFFER:
   case GL_TEXTURE_EXTERNAL_OES:
   case GL_TEXTURE_2D_MULTISAMPLE:
   case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
      /* these texture types cannot be mipmapped */
      return FALSE;
   }

   if (stImage->base.Level > 0 || stObj->base.Attrib.GenerateMipmap)
      return TRUE;

   /* If the application has explicitly called glTextureParameter to set
    * GL_TEXTURE_MAX_LEVEL, such that (max - base) > 0, then they're trying
    * to communicate that they will have multiple miplevels.
    *
    * Core Mesa will initialize MaxLevel to value much larger than
    * MAX_TEXTURE_LEVELS, so we check that to see if it's been set at all.
    */
   if (stObj->base.Attrib.MaxLevel < MAX_TEXTURE_LEVELS &&
       stObj->base.Attrib.MaxLevel - stObj->base.Attrib.BaseLevel > 0)
      return TRUE;

   if (stImage->base._BaseFormat == GL_DEPTH_COMPONENT ||
       stImage->base._BaseFormat == GL_DEPTH_STENCIL_EXT)
      /* depth/stencil textures are seldom mipmapped */
      return FALSE;

   if (stObj->base.Attrib.BaseLevel == 0 && stObj->base.Attrib.MaxLevel == 0)
      return FALSE;

   if (stObj->base.Sampler.Attrib.MinFilter == GL_NEAREST ||
       stObj->base.Sampler.Attrib.MinFilter == GL_LINEAR)
      /* not a mipmap minification filter */
      return FALSE;

   /* If the following sequence of GL calls is used:
    *   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, ...
    *   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    *
    * we would needlessly allocate a mipmapped texture, because the initial
    * MinFilter is GL_NEAREST_MIPMAP_LINEAR. Catch this case and don't
    * allocate a mipmapped texture by default. This may cause texture
    * reallocation later, but GL_NEAREST_MIPMAP_LINEAR is pretty rare.
    */
   if (stObj->base.Sampler.Attrib.MinFilter == GL_NEAREST_MIPMAP_LINEAR)
      return FALSE;

   if (stObj->base.Target == GL_TEXTURE_3D)
      /* 3D textures are seldom mipmapped */
      return FALSE;

   return TRUE;
}


/**
 * Try to allocate a pipe_resource object for the given st_texture_object.
 *
 * We use the given st_texture_image as a clue to determine the size of the
 * mipmap image at level=0.
 *
 * \return GL_TRUE for success, GL_FALSE if out of memory.
 */
static GLboolean
guess_and_alloc_texture(struct st_context *st,
                        struct st_texture_object *stObj,
                        const struct st_texture_image *stImage)
{
   const struct gl_texture_image *firstImage;
   GLuint lastLevel, width, height, depth;
   GLuint bindings;
   unsigned ptWidth;
   uint16_t ptHeight, ptDepth, ptLayers;
   enum pipe_format fmt;
   bool guessed_box = false;

   DBG("%s\n", __func__);

   assert(!stObj->pt);

   /* If a base level image with compatible size exists, use that as our guess.
    */
   firstImage = _mesa_base_tex_image(&stObj->base);
   if (firstImage &&
       firstImage->Width2 > 0 &&
       firstImage->Height2 > 0 &&
       firstImage->Depth2 > 0 &&
       guess_base_level_size(stObj->base.Target,
                             firstImage->Width2,
                             firstImage->Height2,
                             firstImage->Depth2,
                             firstImage->Level,
                             &width, &height, &depth)) {
      if (stImage->base.Width2 == u_minify(width, stImage->base.Level) &&
          stImage->base.Height2 == u_minify(height, stImage->base.Level) &&
          stImage->base.Depth2 == u_minify(depth, stImage->base.Level))
         guessed_box = true;
   }

   if (!guessed_box)
      guessed_box = guess_base_level_size(stObj->base.Target,
                                          stImage->base.Width2,
                                          stImage->base.Height2,
                                          stImage->base.Depth2,
                                          stImage->base.Level,
                                          &width, &height, &depth);

   if (!guessed_box) {
      /* we can't determine the image size at level=0 */
      /* this is not an out of memory error */
      return GL_TRUE;
   }

   /* At this point, (width x height x depth) is the expected size of
    * the level=0 mipmap image.
    */

   /* Guess a reasonable value for lastLevel.  With OpenGL we have no
    * idea how many mipmap levels will be in a texture until we start
    * to render with it.  Make an educated guess here but be prepared
    * to re-allocating a texture buffer with space for more (or fewer)
    * mipmap levels later.
    */
   if (allocate_full_mipmap(stObj, stImage)) {
      /* alloc space for a full mipmap */
      lastLevel = _mesa_get_tex_max_num_levels(stObj->base.Target,
                                               width, height, depth) - 1;
   }
   else {
      /* only alloc space for a single mipmap level */
      lastLevel = 0;
   }

   fmt = st_mesa_format_to_pipe_format(st, stImage->base.TexFormat);

   bindings = default_bindings(st, fmt);

   st_gl_texture_dims_to_pipe_dims(stObj->base.Target,
                                   width, height, depth,
                                   &ptWidth, &ptHeight, &ptDepth, &ptLayers);

   stObj->pt = st_texture_create(st,
                                 gl_target_to_pipe(stObj->base.Target),
                                 fmt,
                                 lastLevel,
                                 ptWidth,
                                 ptHeight,
                                 ptDepth,
                                 ptLayers, 0,
                                 bindings);

   stObj->lastLevel = lastLevel;

   DBG("%s returning %d\n", __func__, (stObj->pt != NULL));

   return stObj->pt != NULL;
}


/**
 * Called via ctx->Driver.AllocTextureImageBuffer().
 * If the texture object/buffer already has space for the indicated image,
 * we're done.  Otherwise, allocate memory for the new texture image.
 */
static GLboolean
st_AllocTextureImageBuffer(struct gl_context *ctx,
                           struct gl_texture_image *texImage)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_image *stImage = st_texture_image(texImage);
   struct st_texture_object *stObj = st_texture_object(texImage->TexObject);
   GLuint width = texImage->Width;
   GLuint height = texImage->Height;
   GLuint depth = texImage->Depth;

   DBG("%s\n", __func__);

   assert(!stImage->pt); /* xxx this might be wrong */

   stObj->needs_validation = true;

   compressed_tex_fallback_allocate(st, stImage);
   const bool allowAllocateToStObj = !stObj->pt ||
                                     stObj->pt->last_level == 0 ||
                                     texImage->Level == 0;

   if (allowAllocateToStObj) {
      /* Look if the parent texture object has space for this image */
      if (stObj->pt &&
          st_texture_match_image(st, stObj->pt, texImage)) {
         /* this image will fit in the existing texture object's memory */
         pipe_resource_reference(&stImage->pt, stObj->pt);
         assert(stImage->pt);
         return GL_TRUE;
      }

      /* The parent texture object does not have space for this image */

      pipe_resource_reference(&stObj->pt, NULL);
      st_texture_release_all_sampler_views(st, stObj);

      if (!guess_and_alloc_texture(st, stObj, stImage)) {
         /* Probably out of memory.
         * Try flushing any pending rendering, then retry.
         */
         st_finish(st);
         if (!guess_and_alloc_texture(st, stObj, stImage)) {
            _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage");
            return GL_FALSE;
         }
      }
   }

   if (stObj->pt &&
       st_texture_match_image(st, stObj->pt, texImage)) {
      /* The image will live in the object's mipmap memory */
      pipe_resource_reference(&stImage->pt, stObj->pt);
      assert(stImage->pt);
      return GL_TRUE;
   }
   else {
      /* Create a new, temporary texture/resource/buffer to hold this
       * one texture image.  Note that when we later access this image
       * (either for mapping or copying) we'll want to always specify
       * mipmap level=0, even if the image represents some other mipmap
       * level.
       */
      enum pipe_format format =
         st_mesa_format_to_pipe_format(st, texImage->TexFormat);
      GLuint bindings = default_bindings(st, format);
      unsigned ptWidth;
      uint16_t ptHeight, ptDepth, ptLayers;

      st_gl_texture_dims_to_pipe_dims(stObj->base.Target,
                                      width, height, depth,
                                      &ptWidth, &ptHeight, &ptDepth, &ptLayers);

      stImage->pt = st_texture_create(st,
                                      gl_target_to_pipe(stObj->base.Target),
                                      format,
                                      0, /* lastLevel */
                                      ptWidth,
                                      ptHeight,
                                      ptDepth,
                                      ptLayers, 0,
                                      bindings);
      return stImage->pt != NULL;
   }
}


/**
 * Preparation prior to glTexImage.  Basically check the 'surface_based'
 * field and switch to a "normal" tex image if necessary.
 */
static void
prep_teximage(struct gl_context *ctx, struct gl_texture_image *texImage,
              GLenum format, GLenum type)
{
   struct gl_texture_object *texObj = texImage->TexObject;
   struct st_texture_object *stObj = st_texture_object(texObj);

   /* switch to "normal" */
   if (stObj->surface_based) {
      const GLenum target = texObj->Target;
      const GLuint level = texImage->Level;
      mesa_format texFormat;

      assert(!st_texture_image(texImage)->pt);
      _mesa_clear_texture_object(ctx, texObj, texImage);
      stObj->layer_override = -1;
      stObj->level_override = -1;
      pipe_resource_reference(&stObj->pt, NULL);

      /* oops, need to init this image again */
      texFormat = _mesa_choose_texture_format(ctx, texObj, target, level,
                                              texImage->InternalFormat, format,
                                              type);

      _mesa_init_teximage_fields(ctx, texImage,
                                 texImage->Width, texImage->Height,
                                 texImage->Depth, texImage->Border,
                                 texImage->InternalFormat, texFormat);

      stObj->surface_based = GL_FALSE;
   }
}


/**
 * Return a writemask for the gallium blit. The parameters can be base
 * formats or "format" from glDrawPixels/glTexImage/glGetTexImage.
 */
unsigned
st_get_blit_mask(GLenum srcFormat, GLenum dstFormat)
{
   switch (dstFormat) {
   case GL_DEPTH_STENCIL:
      switch (srcFormat) {
      case GL_DEPTH_STENCIL:
         return PIPE_MASK_ZS;
      case GL_DEPTH_COMPONENT:
         return PIPE_MASK_Z;
      case GL_STENCIL_INDEX:
         return PIPE_MASK_S;
      default:
         assert(0);
         return 0;
      }

   case GL_DEPTH_COMPONENT:
      switch (srcFormat) {
      case GL_DEPTH_STENCIL:
      case GL_DEPTH_COMPONENT:
         return PIPE_MASK_Z;
      default:
         assert(0);
         return 0;
      }

   case GL_STENCIL_INDEX:
      switch (srcFormat) {
      case GL_DEPTH_STENCIL:
      case GL_STENCIL_INDEX:
         return PIPE_MASK_S;
      default:
         assert(0);
         return 0;
      }

   default:
      return PIPE_MASK_RGBA;
   }
}

/**
 * Converts format to a format with the same components, types
 * and sizes, but with the components in RGBA order.
 */
static enum pipe_format
unswizzle_format(enum pipe_format format)
{
   switch (format)
   {
   case PIPE_FORMAT_B8G8R8A8_UNORM:
   case PIPE_FORMAT_A8R8G8B8_UNORM:
   case PIPE_FORMAT_A8B8G8R8_UNORM:
      return PIPE_FORMAT_R8G8B8A8_UNORM;

   case PIPE_FORMAT_B10G10R10A2_UNORM:
      return PIPE_FORMAT_R10G10B10A2_UNORM;

   case PIPE_FORMAT_B10G10R10A2_SNORM:
      return PIPE_FORMAT_R10G10B10A2_SNORM;

   case PIPE_FORMAT_B10G10R10A2_UINT:
      return PIPE_FORMAT_R10G10B10A2_UINT;

   default:
      return format;
   }
}


/**
 * Converts PIPE_FORMAT_A* to PIPE_FORMAT_R*.
 */
static enum pipe_format
alpha_to_red(enum pipe_format format)
{
   switch (format)
   {
   case PIPE_FORMAT_A8_UNORM:
      return PIPE_FORMAT_R8_UNORM;
   case PIPE_FORMAT_A8_SNORM:
      return PIPE_FORMAT_R8_SNORM;
   case PIPE_FORMAT_A8_UINT:
      return PIPE_FORMAT_R8_UINT;
   case PIPE_FORMAT_A8_SINT:
      return PIPE_FORMAT_R8_SINT;

   case PIPE_FORMAT_A16_UNORM:
      return PIPE_FORMAT_R16_UNORM;
   case PIPE_FORMAT_A16_SNORM:
      return PIPE_FORMAT_R16_SNORM;
   case PIPE_FORMAT_A16_UINT:
      return PIPE_FORMAT_R16_UINT;
   case PIPE_FORMAT_A16_SINT:
      return PIPE_FORMAT_R16_SINT;
   case PIPE_FORMAT_A16_FLOAT:
      return PIPE_FORMAT_R16_FLOAT;

   case PIPE_FORMAT_A32_UINT:
      return PIPE_FORMAT_R32_UINT;
   case PIPE_FORMAT_A32_SINT:
      return PIPE_FORMAT_R32_SINT;
   case PIPE_FORMAT_A32_FLOAT:
      return PIPE_FORMAT_R32_FLOAT;

   default:
      return format;
   }
}


/**
 * Converts PIPE_FORMAT_R*A* to PIPE_FORMAT_R*G*.
 */
static enum pipe_format
red_alpha_to_red_green(enum pipe_format format)
{
   switch (format)
   {
   case PIPE_FORMAT_R8A8_UNORM:
      return PIPE_FORMAT_R8G8_UNORM;
   case PIPE_FORMAT_R8A8_SNORM:
      return PIPE_FORMAT_R8G8_SNORM;
   case PIPE_FORMAT_R8A8_UINT:
      return PIPE_FORMAT_R8G8_UINT;
   case PIPE_FORMAT_R8A8_SINT:
      return PIPE_FORMAT_R8G8_SINT;

   case PIPE_FORMAT_R16A16_UNORM:
      return PIPE_FORMAT_R16G16_UNORM;
   case PIPE_FORMAT_R16A16_SNORM:
      return PIPE_FORMAT_R16G16_SNORM;
   case PIPE_FORMAT_R16A16_UINT:
      return PIPE_FORMAT_R16G16_UINT;
   case PIPE_FORMAT_R16A16_SINT:
      return PIPE_FORMAT_R16G16_SINT;
   case PIPE_FORMAT_R16A16_FLOAT:
      return PIPE_FORMAT_R16G16_FLOAT;

   case PIPE_FORMAT_R32A32_UINT:
      return PIPE_FORMAT_R32G32_UINT;
   case PIPE_FORMAT_R32A32_SINT:
      return PIPE_FORMAT_R32G32_SINT;
   case PIPE_FORMAT_R32A32_FLOAT:
      return PIPE_FORMAT_R32G32_FLOAT;

   default:
       return format;
   }
}


/**
 * Converts PIPE_FORMAT_L*A* to PIPE_FORMAT_R*G*.
 */
static enum pipe_format
luminance_alpha_to_red_green(enum pipe_format format)
{
   switch (format)
   {
   case PIPE_FORMAT_L8A8_UNORM:
      return PIPE_FORMAT_R8G8_UNORM;
   case PIPE_FORMAT_L8A8_SNORM:
      return PIPE_FORMAT_R8G8_SNORM;
   case PIPE_FORMAT_L8A8_UINT:
      return PIPE_FORMAT_R8G8_UINT;
   case PIPE_FORMAT_L8A8_SINT:
      return PIPE_FORMAT_R8G8_SINT;

   case PIPE_FORMAT_L16A16_UNORM:
      return PIPE_FORMAT_R16G16_UNORM;
   case PIPE_FORMAT_L16A16_SNORM:
      return PIPE_FORMAT_R16G16_SNORM;
   case PIPE_FORMAT_L16A16_UINT:
      return PIPE_FORMAT_R16G16_UINT;
   case PIPE_FORMAT_L16A16_SINT:
      return PIPE_FORMAT_R16G16_SINT;
   case PIPE_FORMAT_L16A16_FLOAT:
      return PIPE_FORMAT_R16G16_FLOAT;

   case PIPE_FORMAT_L32A32_UINT:
      return PIPE_FORMAT_R32G32_UINT;
   case PIPE_FORMAT_L32A32_SINT:
      return PIPE_FORMAT_R32G32_SINT;
   case PIPE_FORMAT_L32A32_FLOAT:
      return PIPE_FORMAT_R32G32_FLOAT;

   default:
       return format;
   }
}


/**
 * Returns true if format is a PIPE_FORMAT_A* format, and false otherwise.
 */
static bool
format_is_alpha(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   if (desc->nr_channels == 1 &&
       desc->swizzle[0] == PIPE_SWIZZLE_0 &&
       desc->swizzle[1] == PIPE_SWIZZLE_0 &&
       desc->swizzle[2] == PIPE_SWIZZLE_0 &&
       desc->swizzle[3] == PIPE_SWIZZLE_X)
      return true;

   return false;
}


/**
 * Returns true if format is a PIPE_FORMAT_R* format, and false otherwise.
 */
static bool
format_is_red(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   if (desc->nr_channels == 1 &&
       desc->swizzle[0] == PIPE_SWIZZLE_X &&
       desc->swizzle[1] == PIPE_SWIZZLE_0 &&
       desc->swizzle[2] == PIPE_SWIZZLE_0 &&
       desc->swizzle[3] == PIPE_SWIZZLE_1)
      return true;

   return false;
}


/**
 * Returns true if format is a PIPE_FORMAT_L* format, and false otherwise.
 */
static bool
format_is_luminance(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   if (desc->nr_channels == 1 &&
       desc->swizzle[0] == PIPE_SWIZZLE_X &&
       desc->swizzle[1] == PIPE_SWIZZLE_X &&
       desc->swizzle[2] == PIPE_SWIZZLE_X &&
       desc->swizzle[3] == PIPE_SWIZZLE_1)
      return true;

   return false;
}

/**
 * Returns true if format is a PIPE_FORMAT_R*A* format, and false otherwise.
 */
static bool
format_is_red_alpha(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   if (desc->nr_channels == 2 &&
       desc->swizzle[0] == PIPE_SWIZZLE_X &&
       desc->swizzle[1] == PIPE_SWIZZLE_0 &&
       desc->swizzle[2] == PIPE_SWIZZLE_0 &&
       desc->swizzle[3] == PIPE_SWIZZLE_Y)
      return true;

   return false;
}


static bool
format_is_swizzled_rgba(enum pipe_format format)
{
    const struct util_format_description *desc = util_format_description(format);

    if ((desc->swizzle[0] == TGSI_SWIZZLE_X || desc->swizzle[0] == PIPE_SWIZZLE_0) &&
        (desc->swizzle[1] == TGSI_SWIZZLE_Y || desc->swizzle[1] == PIPE_SWIZZLE_0) &&
        (desc->swizzle[2] == TGSI_SWIZZLE_Z || desc->swizzle[2] == PIPE_SWIZZLE_0) &&
        (desc->swizzle[3] == TGSI_SWIZZLE_W || desc->swizzle[3] == PIPE_SWIZZLE_1))
       return false;

    return true;
}


struct format_table
{
   unsigned char swizzle[4];
   enum pipe_format format;
};

static const struct format_table table_8888_unorm[] = {
   { { 0, 1, 2, 3 }, PIPE_FORMAT_R8G8B8A8_UNORM },
   { { 2, 1, 0, 3 }, PIPE_FORMAT_B8G8R8A8_UNORM },
   { { 3, 0, 1, 2 }, PIPE_FORMAT_A8R8G8B8_UNORM },
   { { 3, 2, 1, 0 }, PIPE_FORMAT_A8B8G8R8_UNORM }
};

static const struct format_table table_1010102_unorm[] = {
   { { 0, 1, 2, 3 }, PIPE_FORMAT_R10G10B10A2_UNORM },
   { { 2, 1, 0, 3 }, PIPE_FORMAT_B10G10R10A2_UNORM }
};

static const struct format_table table_1010102_snorm[] = {
   { { 0, 1, 2, 3 }, PIPE_FORMAT_R10G10B10A2_SNORM },
   { { 2, 1, 0, 3 }, PIPE_FORMAT_B10G10R10A2_SNORM }
};

static const struct format_table table_1010102_uint[] = {
   { { 0, 1, 2, 3 }, PIPE_FORMAT_R10G10B10A2_UINT },
   { { 2, 1, 0, 3 }, PIPE_FORMAT_B10G10R10A2_UINT }
};

static enum pipe_format
swizzle_format(enum pipe_format format, const int * const swizzle)
{
   unsigned i;

   switch (format) {
   case PIPE_FORMAT_R8G8B8A8_UNORM:
   case PIPE_FORMAT_B8G8R8A8_UNORM:
   case PIPE_FORMAT_A8R8G8B8_UNORM:
   case PIPE_FORMAT_A8B8G8R8_UNORM:
      for (i = 0; i < ARRAY_SIZE(table_8888_unorm); i++) {
         if (swizzle[0] == table_8888_unorm[i].swizzle[0] &&
             swizzle[1] == table_8888_unorm[i].swizzle[1] &&
             swizzle[2] == table_8888_unorm[i].swizzle[2] &&
             swizzle[3] == table_8888_unorm[i].swizzle[3])
            return table_8888_unorm[i].format;
      }
      break;

   case PIPE_FORMAT_R10G10B10A2_UNORM:
   case PIPE_FORMAT_B10G10R10A2_UNORM:
      for (i = 0; i < ARRAY_SIZE(table_1010102_unorm); i++) {
         if (swizzle[0] == table_1010102_unorm[i].swizzle[0] &&
             swizzle[1] == table_1010102_unorm[i].swizzle[1] &&
             swizzle[2] == table_1010102_unorm[i].swizzle[2] &&
             swizzle[3] == table_1010102_unorm[i].swizzle[3])
            return table_1010102_unorm[i].format;
      }
      break;

   case PIPE_FORMAT_R10G10B10A2_SNORM:
   case PIPE_FORMAT_B10G10R10A2_SNORM:
      for (i = 0; i < ARRAY_SIZE(table_1010102_snorm); i++) {
         if (swizzle[0] == table_1010102_snorm[i].swizzle[0] &&
             swizzle[1] == table_1010102_snorm[i].swizzle[1] &&
             swizzle[2] == table_1010102_snorm[i].swizzle[2] &&
             swizzle[3] == table_1010102_snorm[i].swizzle[3])
            return table_1010102_snorm[i].format;
      }
      break;

   case PIPE_FORMAT_R10G10B10A2_UINT:
   case PIPE_FORMAT_B10G10R10A2_UINT:
      for (i = 0; i < ARRAY_SIZE(table_1010102_uint); i++) {
         if (swizzle[0] == table_1010102_uint[i].swizzle[0] &&
             swizzle[1] == table_1010102_uint[i].swizzle[1] &&
             swizzle[2] == table_1010102_uint[i].swizzle[2] &&
             swizzle[3] == table_1010102_uint[i].swizzle[3])
            return table_1010102_uint[i].format;
      }
      break;

   default:
      break;
   }

   return PIPE_FORMAT_NONE;
}

static bool
reinterpret_formats(enum pipe_format *src_format, enum pipe_format *dst_format)
{
   enum pipe_format src = *src_format;
   enum pipe_format dst = *dst_format;

   /* Note: dst_format has already been transformed from luminance/intensity
    *       to red when this function is called.  The source format will never
    *       be an intensity format, because GL_INTENSITY is not a legal value
    *       for the format parameter in glTex(Sub)Image(). */

   if (format_is_alpha(src)) {
      if (!format_is_alpha(dst))
         return false;

      src = alpha_to_red(src);
      dst = alpha_to_red(dst);
   } else if (format_is_luminance(src)) {
      if (!format_is_red(dst) && !format_is_red_alpha(dst))
         return false;

      src = util_format_luminance_to_red(src);
   } else if (util_format_is_luminance_alpha(src)) {
      src = luminance_alpha_to_red_green(src);

      if (format_is_red_alpha(dst)) {
         dst = red_alpha_to_red_green(dst);
      } else if (!format_is_red(dst))
         return false;
   } else if (format_is_swizzled_rgba(src)) {
      const struct util_format_description *src_desc = util_format_description(src);
      const struct util_format_description *dst_desc = util_format_description(dst);
      int swizzle[4];
      unsigned i;

      /* Make sure the format is an RGBA and not an RGBX format */
      if (src_desc->nr_channels != 4 || src_desc->swizzle[3] == PIPE_SWIZZLE_1)
         return false;

      if (dst_desc->nr_channels != 4 || dst_desc->swizzle[3] == PIPE_SWIZZLE_1)
         return false;

      for (i = 0; i < 4; i++)
         swizzle[i] = dst_desc->swizzle[src_desc->swizzle[i]];

      dst = swizzle_format(dst, swizzle);
      if (dst == PIPE_FORMAT_NONE)
         return false;

      src = unswizzle_format(src);
   }

   *src_format = src;
   *dst_format = dst;
   return true;
}

static bool
try_pbo_upload_common(struct gl_context *ctx,
                      struct pipe_surface *surface,
                      const struct st_pbo_addresses *addr,
                      enum pipe_format src_format)
{
   struct st_context *st = st_context(ctx);
   struct cso_context *cso = st->cso_context;
   struct pipe_context *pipe = st->pipe;
   bool success = false;
   void *fs;

   fs = st_pbo_get_upload_fs(st, src_format, surface->format, addr->depth != 1);
   if (!fs)
      return false;

   cso_save_state(cso, (CSO_BIT_VERTEX_ELEMENTS |
                        CSO_BIT_FRAMEBUFFER |
                        CSO_BIT_VIEWPORT |
                        CSO_BIT_BLEND |
                        CSO_BIT_DEPTH_STENCIL_ALPHA |
                        CSO_BIT_RASTERIZER |
                        CSO_BIT_STREAM_OUTPUTS |
                        (st->active_queries ? CSO_BIT_PAUSE_QUERIES : 0) |
                        CSO_BIT_SAMPLE_MASK |
                        CSO_BIT_MIN_SAMPLES |
                        CSO_BIT_RENDER_CONDITION |
                        CSO_BITS_ALL_SHADERS));

   cso_set_sample_mask(cso, ~0);
   cso_set_min_samples(cso, 1);
   cso_set_render_condition(cso, NULL, FALSE, 0);

   /* Set up the sampler_view */
   {
      struct pipe_sampler_view templ;
      struct pipe_sampler_view *sampler_view;

      memset(&templ, 0, sizeof(templ));
      templ.target = PIPE_BUFFER;
      templ.format = src_format;
      templ.u.buf.offset = addr->first_element * addr->bytes_per_pixel;
      templ.u.buf.size = (addr->last_element - addr->first_element + 1) *
                         addr->bytes_per_pixel;
      templ.swizzle_r = PIPE_SWIZZLE_X;
      templ.swizzle_g = PIPE_SWIZZLE_Y;
      templ.swizzle_b = PIPE_SWIZZLE_Z;
      templ.swizzle_a = PIPE_SWIZZLE_W;

      sampler_view = pipe->create_sampler_view(pipe, addr->buffer, &templ);
      if (sampler_view == NULL)
         goto fail;

      pipe->set_sampler_views(pipe, PIPE_SHADER_FRAGMENT, 0, 1, 0,
                              false, &sampler_view);
      st->state.num_sampler_views[PIPE_SHADER_FRAGMENT] =
         MAX2(st->state.num_sampler_views[PIPE_SHADER_FRAGMENT], 1);

      pipe_sampler_view_reference(&sampler_view, NULL);
   }

   /* Framebuffer_state */
   {
      struct pipe_framebuffer_state fb;
      memset(&fb, 0, sizeof(fb));
      fb.width = surface->width;
      fb.height = surface->height;
      fb.nr_cbufs = 1;
      fb.cbufs[0] = surface;

      cso_set_framebuffer(cso, &fb);
   }

   cso_set_viewport_dims(cso, surface->width, surface->height, FALSE);

   /* Blend state */
   cso_set_blend(cso, &st->pbo.upload_blend);

   /* Depth/stencil/alpha state */
   {
      struct pipe_depth_stencil_alpha_state dsa;
      memset(&dsa, 0, sizeof(dsa));
      cso_set_depth_stencil_alpha(cso, &dsa);
   }

   /* Set up the fragment shader */
   cso_set_fragment_shader_handle(cso, fs);

   success = st_pbo_draw(st, addr, surface->width, surface->height);

fail:
   /* Unbind all because st/mesa won't do it if the current shader doesn't
    * use them.
    */
   cso_restore_state(cso, CSO_UNBIND_FS_SAMPLERVIEWS);
   st->state.num_sampler_views[PIPE_SHADER_FRAGMENT] = 0;

   st->dirty |= ST_NEW_VERTEX_ARRAYS |
                ST_NEW_FS_CONSTANTS |
                ST_NEW_FS_SAMPLER_VIEWS;

   return success;
}


static bool
try_pbo_upload(struct gl_context *ctx, GLuint dims,
               struct gl_texture_image *texImage,
               GLenum format, GLenum type,
               enum pipe_format dst_format,
               GLint xoffset, GLint yoffset, GLint zoffset,
               GLint width, GLint height, GLint depth,
               const void *pixels,
               const struct gl_pixelstore_attrib *unpack)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_image *stImage = st_texture_image(texImage);
   struct st_texture_object *stObj = st_texture_object(texImage->TexObject);
   struct pipe_resource *texture = stImage->pt;
   struct pipe_context *pipe = st->pipe;
   struct pipe_screen *screen = st->screen;
   struct pipe_surface *surface = NULL;
   struct st_pbo_addresses addr;
   enum pipe_format src_format;
   const struct util_format_description *desc;
   GLenum gl_target = texImage->TexObject->Target;
   bool success;

   if (!st->pbo.upload_enabled)
      return false;

   /* From now on, we need the gallium representation of dimensions. */
   if (gl_target == GL_TEXTURE_1D_ARRAY) {
      depth = height;
      height = 1;
      zoffset = yoffset;
      yoffset = 0;
   }

   if (depth != 1 && !st->pbo.layers)
      return false;

   /* Choose the source format. Initially, we do so without checking driver
    * support at all because of the remapping we later perform and because
    * at least the Radeon driver actually supports some formats for texture
    * buffers which it doesn't support for regular textures. */
   src_format = st_choose_matching_format(st, 0, format, type,
                                          unpack->SwapBytes);
   if (!src_format) {
      return false;
   }

   src_format = util_format_linear(src_format);
   desc = util_format_description(src_format);

   if (desc->layout != UTIL_FORMAT_LAYOUT_PLAIN)
      return false;

   if (desc->colorspace != UTIL_FORMAT_COLORSPACE_RGB)
      return false;

   if (st->pbo.rgba_only) {
      enum pipe_format orig_dst_format = dst_format;

      if (!reinterpret_formats(&src_format, &dst_format)) {
         return false;
      }

      if (dst_format != orig_dst_format &&
          !screen->is_format_supported(screen, dst_format, PIPE_TEXTURE_2D, 0,
                                       0, PIPE_BIND_RENDER_TARGET)) {
         return false;
      }
   }

   if (!src_format ||
       !screen->is_format_supported(screen, src_format, PIPE_BUFFER, 0, 0,
                                    PIPE_BIND_SAMPLER_VIEW)) {
      return false;
   }

   /* Compute buffer addresses */
   addr.xoffset = xoffset;
   addr.yoffset = yoffset;
   addr.width = width;
   addr.height = height;
   addr.depth = depth;
   addr.bytes_per_pixel = desc->block.bits / 8;

   if (!st_pbo_addresses_pixelstore(st, gl_target, dims == 3, unpack, pixels,
                                    &addr))
      return false;

   /* Set up the surface */
   {
      unsigned level = stObj->pt != stImage->pt
         ? 0 : texImage->TexObject->Attrib.MinLevel + texImage->Level;
      unsigned max_layer = util_max_layer(texture, level);

      zoffset += texImage->Face + texImage->TexObject->Attrib.MinLayer;

      struct pipe_surface templ;
      memset(&templ, 0, sizeof(templ));
      templ.format = dst_format;
      templ.u.tex.level = level;
      templ.u.tex.first_layer = MIN2(zoffset, max_layer);
      templ.u.tex.last_layer = MIN2(zoffset + depth - 1, max_layer);

      surface = pipe->create_surface(pipe, texture, &templ);
      if (!surface)
         return false;
   }

   success = try_pbo_upload_common(ctx, surface, &addr, src_format);

   pipe_surface_reference(&surface, NULL);

   return success;
}

static bool
try_pbo_download(struct st_context *st,
                   struct gl_texture_image *texImage,
                   enum pipe_format src_format, enum pipe_format dst_format,
                   GLint xoffset, GLint yoffset, GLint zoffset,
                   GLint width, GLint height, GLint depth,
                   const struct gl_pixelstore_attrib *pack, void *pixels)
{
   struct st_texture_image *stImage = st_texture_image(texImage);
   struct pipe_context *pipe = st->pipe;
   struct pipe_screen *screen = pipe->screen;
   struct pipe_resource *texture = stImage->pt;
   struct cso_context *cso = st->cso_context;
   const struct util_format_description *desc;
   struct st_pbo_addresses addr;
   struct pipe_framebuffer_state fb;
   enum pipe_texture_target pipe_target;
   GLenum gl_target = texImage->TexObject->Target;
   GLuint dims;
   bool success = false;

   if (texture->nr_samples > 1)
      return false;

   /* GetTexImage only returns a single face for cubemaps. */
   if (gl_target == GL_TEXTURE_CUBE_MAP) {
      gl_target = GL_TEXTURE_2D;
   }
   if (gl_target == GL_TEXTURE_CUBE_MAP_ARRAY) {
      gl_target = GL_TEXTURE_2D_ARRAY;
   }
   pipe_target = gl_target_to_pipe(gl_target);
   dims = _mesa_get_texture_dimensions(gl_target);

   /* From now on, we need the gallium representation of dimensions. */
   if (gl_target == GL_TEXTURE_1D_ARRAY) {
      depth = height;
      height = 1;
      zoffset = yoffset;
      yoffset = 0;
   }

   if (depth != 1 && !st->pbo.layers)
      return false;

   if (!screen->is_format_supported(screen, dst_format, PIPE_BUFFER, 0, 0,
                                    PIPE_BIND_SHADER_IMAGE) ||
       util_format_is_compressed(src_format) ||
       util_format_is_compressed(dst_format))
      return false;

   desc = util_format_description(dst_format);

   /* Compute PBO addresses */
   addr.bytes_per_pixel = desc->block.bits / 8;
   addr.xoffset = xoffset;
   addr.yoffset = yoffset;
   addr.width = width;
   addr.height = height;
   addr.depth = depth;
   if (!st_pbo_addresses_pixelstore(st, gl_target, dims == 3, pack, pixels, &addr))
      return false;

   cso_save_state(cso, (CSO_BIT_VERTEX_ELEMENTS |
                        CSO_BIT_FRAMEBUFFER |
                        CSO_BIT_VIEWPORT |
                        CSO_BIT_BLEND |
                        CSO_BIT_DEPTH_STENCIL_ALPHA |
                        CSO_BIT_RASTERIZER |
                        CSO_BIT_STREAM_OUTPUTS |
                        (st->active_queries ? CSO_BIT_PAUSE_QUERIES : 0) |
                        CSO_BIT_SAMPLE_MASK |
                        CSO_BIT_MIN_SAMPLES |
                        CSO_BIT_RENDER_CONDITION |
                        CSO_BITS_ALL_SHADERS));

   cso_set_sample_mask(cso, ~0);
   cso_set_min_samples(cso, 1);
   cso_set_render_condition(cso, NULL, FALSE, 0);

   /* Set up the sampler_view */
   {
      struct pipe_sampler_view templ;
      struct pipe_sampler_view *sampler_view;
      struct pipe_sampler_state sampler = {0};
      const struct pipe_sampler_state *samplers[1] = {&sampler};
      unsigned level = texImage->TexObject->Attrib.MinLevel + texImage->Level;
      unsigned max_layer = util_max_layer(texture, level);

      u_sampler_view_default_template(&templ, texture, src_format);

      templ.target = pipe_target;
      templ.u.tex.first_level = level;
      templ.u.tex.last_level = templ.u.tex.first_level;

      zoffset += texImage->Face + texImage->TexObject->Attrib.MinLayer;
      templ.u.tex.first_layer = MIN2(zoffset, max_layer);
      templ.u.tex.last_layer = MIN2(zoffset + depth - 1, max_layer);

      sampler_view = pipe->create_sampler_view(pipe, texture, &templ);
      if (sampler_view == NULL)
         goto fail;

      pipe->set_sampler_views(pipe, PIPE_SHADER_FRAGMENT, 0, 1, 0, true, &sampler_view);
      sampler_view = NULL;

      cso_set_samplers(cso, PIPE_SHADER_FRAGMENT, 1, samplers);
   }

   /* Set up destination image */
   {
      struct pipe_image_view image;

      memset(&image, 0, sizeof(image));
      image.resource = addr.buffer;
      image.format = dst_format;
      image.access = PIPE_IMAGE_ACCESS_WRITE;
      image.shader_access = PIPE_IMAGE_ACCESS_WRITE;
      image.u.buf.offset = addr.first_element * addr.bytes_per_pixel;
      image.u.buf.size = (addr.last_element - addr.first_element + 1) *
                         addr.bytes_per_pixel;

      pipe->set_shader_images(pipe, PIPE_SHADER_FRAGMENT, 0, 1, 0, &image);
   }

   /* Set up no-attachment framebuffer */
   memset(&fb, 0, sizeof(fb));
   fb.width = texture->width0;
   fb.height = texture->height0;
   fb.layers = 1;
   fb.samples = 1;
   cso_set_framebuffer(cso, &fb);

   /* Any blend state would do. Set this just to prevent drivers having
    * blend == NULL.
    */
   cso_set_blend(cso, &st->pbo.upload_blend);

   cso_set_viewport_dims(cso, fb.width, fb.height, FALSE);

   {
      struct pipe_depth_stencil_alpha_state dsa;
      memset(&dsa, 0, sizeof(dsa));
      cso_set_depth_stencil_alpha(cso, &dsa);
   }

   /* Set up the fragment shader */
   {
      void *fs = st_pbo_get_download_fs(st, pipe_target, src_format, dst_format, addr.depth != 1);
      if (!fs)
         goto fail;

      cso_set_fragment_shader_handle(cso, fs);
   }

   success = st_pbo_draw(st, &addr, fb.width, fb.height);

   /* Buffer written via shader images needs explicit synchronization. */
   pipe->memory_barrier(pipe, PIPE_BARRIER_IMAGE | PIPE_BARRIER_TEXTURE | PIPE_BARRIER_FRAMEBUFFER);

fail:
   /* Unbind all because st/mesa won't do it if the current shader doesn't
    * use them.
    */
   cso_restore_state(cso, CSO_UNBIND_FS_SAMPLERVIEWS | CSO_UNBIND_FS_IMAGE0);
   st->state.num_sampler_views[PIPE_SHADER_FRAGMENT] = 0;

   st->dirty |= ST_NEW_FS_CONSTANTS |
                ST_NEW_FS_IMAGES |
                ST_NEW_FS_SAMPLER_VIEWS |
                ST_NEW_VERTEX_ARRAYS;

   return success;
}


static void
st_TexSubImage(struct gl_context *ctx, GLuint dims,
               struct gl_texture_image *texImage,
               GLint xoffset, GLint yoffset, GLint zoffset,
               GLint width, GLint height, GLint depth,
               GLenum format, GLenum type, const void *pixels,
               const struct gl_pixelstore_attrib *unpack)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_image *stImage = st_texture_image(texImage);
   struct st_texture_object *stObj = st_texture_object(texImage->TexObject);
   struct pipe_context *pipe = st->pipe;
   struct pipe_screen *screen = st->screen;
   struct pipe_resource *dst = stImage->pt;
   struct pipe_resource *src = NULL;
   struct pipe_resource src_templ;
   struct pipe_transfer *transfer;
   struct pipe_blit_info blit;
   enum pipe_format src_format, dst_format;
   mesa_format mesa_src_format;
   GLenum gl_target = texImage->TexObject->Target;
   unsigned bind;
   GLubyte *map;
   unsigned dstz = texImage->Face + texImage->TexObject->Attrib.MinLayer;
   unsigned dst_level = 0;
   bool throttled = false;

   st_flush_bitmap_cache(st);
   st_invalidate_readpix_cache(st);

   if (stObj->pt == stImage->pt)
      dst_level = texImage->TexObject->Attrib.MinLevel + texImage->Level;

   assert(!_mesa_is_format_etc2(texImage->TexFormat) &&
          !_mesa_is_format_astc_2d(texImage->TexFormat) &&
          texImage->TexFormat != MESA_FORMAT_ETC1_RGB8);

   if (!dst)
      goto fallback;

   /* Try texture_subdata, which should be the fastest memcpy path. */
   if (pixels &&
       !unpack->BufferObj &&
       _mesa_texstore_can_use_memcpy(ctx, texImage->_BaseFormat,
                                     texImage->TexFormat, format, type,
                                     unpack)) {
      struct pipe_box box;
      unsigned stride, layer_stride;
      void *data;

      stride = _mesa_image_row_stride(unpack, width, format, type);
      layer_stride = _mesa_image_image_stride(unpack, width, height, format,
                                              type);
      data = _mesa_image_address(dims, unpack, pixels, width, height, format,
                                 type, 0, 0, 0);

      /* Convert to Gallium coordinates. */
      if (gl_target == GL_TEXTURE_1D_ARRAY) {
         zoffset = yoffset;
         yoffset = 0;
         depth = height;
         height = 1;
         layer_stride = stride;
      }

      util_throttle_memory_usage(pipe, &st->throttle,
                                 (uint64_t) width * height * depth *
                                 util_format_get_blocksize(dst->format));

      u_box_3d(xoffset, yoffset, zoffset + dstz, width, height, depth, &box);
      pipe->texture_subdata(pipe, dst, dst_level, 0,
                            &box, data, stride, layer_stride);
      return;
   }

   if (!st->prefer_blit_based_texture_transfer) {
      goto fallback;
   }

   /* XXX Fallback for depth-stencil formats due to an incomplete stencil
    * blit implementation in some drivers. */
   if (format == GL_DEPTH_STENCIL) {
      goto fallback;
   }

   /* If the base internal format and the texture format don't match,
    * we can't use blit-based TexSubImage. */
   if (texImage->_BaseFormat !=
       _mesa_get_format_base_format(texImage->TexFormat)) {
      goto fallback;
   }


   /* See if the destination format is supported. */
   if (format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL)
      bind = PIPE_BIND_DEPTH_STENCIL;
   else
      bind = PIPE_BIND_RENDER_TARGET;

   /* For luminance and intensity, only the red channel is stored
    * in the destination. */
   dst_format = util_format_linear(dst->format);
   dst_format = util_format_luminance_to_red(dst_format);
   dst_format = util_format_intensity_to_red(dst_format);

   if (!dst_format ||
       !screen->is_format_supported(screen, dst_format, dst->target,
                                    dst->nr_samples, dst->nr_storage_samples,
                                    bind)) {
      goto fallback;
   }

   if (unpack->BufferObj) {
      if (try_pbo_upload(ctx, dims, texImage, format, type, dst_format,
                         xoffset, yoffset, zoffset,
                         width, height, depth, pixels, unpack))
         return;
   }

   /* See if the texture format already matches the format and type,
    * in which case the memcpy-based fast path will likely be used and
    * we don't have to blit. */
   if (_mesa_format_matches_format_and_type(texImage->TexFormat, format,
                                            type, unpack->SwapBytes, NULL)) {
      goto fallback;
   }

   /* Choose the source format. */
   src_format = st_choose_matching_format(st, PIPE_BIND_SAMPLER_VIEW,
                                          format, type, unpack->SwapBytes);
   if (!src_format) {
      goto fallback;
   }

   mesa_src_format = st_pipe_format_to_mesa_format(src_format);

   /* There is no reason to do this if we cannot use memcpy for the temporary
    * source texture at least. This also takes transfer ops into account,
    * etc. */
   if (!_mesa_texstore_can_use_memcpy(ctx,
                             _mesa_get_format_base_format(mesa_src_format),
                             mesa_src_format, format, type, unpack)) {
      goto fallback;
   }

   /* TexSubImage only sets a single cubemap face. */
   if (gl_target == GL_TEXTURE_CUBE_MAP) {
      gl_target = GL_TEXTURE_2D;
   }
   /* TexSubImage can specify subsets of cube map array faces
    * so we need to upload via 2D array instead */
   if (gl_target == GL_TEXTURE_CUBE_MAP_ARRAY) {
      gl_target = GL_TEXTURE_2D_ARRAY;
   }

   /* Initialize the source texture description. */
   memset(&src_templ, 0, sizeof(src_templ));
   src_templ.target = gl_target_to_pipe(gl_target);
   src_templ.format = src_format;
   src_templ.bind = PIPE_BIND_SAMPLER_VIEW;
   src_templ.usage = PIPE_USAGE_STAGING;

   st_gl_texture_dims_to_pipe_dims(gl_target, width, height, depth,
                                   &src_templ.width0, &src_templ.height0,
                                   &src_templ.depth0, &src_templ.array_size);

   /* Check for NPOT texture support. */
   if (!screen->get_param(screen, PIPE_CAP_NPOT_TEXTURES) &&
       (!util_is_power_of_two_or_zero(src_templ.width0) ||
        !util_is_power_of_two_or_zero(src_templ.height0) ||
        !util_is_power_of_two_or_zero(src_templ.depth0))) {
      goto fallback;
   }

   util_throttle_memory_usage(pipe, &st->throttle,
                              (uint64_t) width * height * depth *
                              util_format_get_blocksize(src_templ.format));
   throttled = true;

   /* Create the source texture. */
   src = screen->resource_create(screen, &src_templ);
   if (!src) {
      goto fallback;
   }

   /* Map source pixels. */
   pixels = _mesa_validate_pbo_teximage(ctx, dims, width, height, depth,
                                        format, type, pixels, unpack,
                                        "glTexSubImage");
   if (!pixels) {
      /* This is a GL error. */
      pipe_resource_reference(&src, NULL);
      return;
   }

   /* From now on, we need the gallium representation of dimensions. */
   if (gl_target == GL_TEXTURE_1D_ARRAY) {
      zoffset = yoffset;
      yoffset = 0;
      depth = height;
      height = 1;
   }

   map = pipe_texture_map_3d(pipe, src, 0, PIPE_MAP_WRITE, 0, 0, 0,
                              width, height, depth, &transfer);
   if (!map) {
      _mesa_unmap_teximage_pbo(ctx, unpack);
      pipe_resource_reference(&src, NULL);
      goto fallback;
   }

   /* Upload pixels (just memcpy). */
   {
      const uint bytesPerRow = width * util_format_get_blocksize(src_format);
      GLuint row, slice;

      for (slice = 0; slice < (unsigned) depth; slice++) {
         if (gl_target == GL_TEXTURE_1D_ARRAY) {
            /* 1D array textures.
             * We need to convert gallium coords to GL coords.
             */
            void *src = _mesa_image_address2d(unpack, pixels,
                                                width, depth, format,
                                                type, slice, 0);
            memcpy(map, src, bytesPerRow);
         }
         else {
            ubyte *slice_map = map;

            for (row = 0; row < (unsigned) height; row++) {
               void *src = _mesa_image_address(dims, unpack, pixels,
                                                 width, height, format,
                                                 type, slice, row, 0);
               memcpy(slice_map, src, bytesPerRow);
               slice_map += transfer->stride;
            }
         }
         map += transfer->layer_stride;
      }
   }

   pipe_texture_unmap(pipe, transfer);
   _mesa_unmap_teximage_pbo(ctx, unpack);

   /* Blit. */
   memset(&blit, 0, sizeof(blit));
   blit.src.resource = src;
   blit.src.level = 0;
   blit.src.format = src_format;
   blit.dst.resource = dst;
   blit.dst.level = dst_level;
   blit.dst.format = dst_format;
   blit.src.box.x = blit.src.box.y = blit.src.box.z = 0;
   blit.dst.box.x = xoffset;
   blit.dst.box.y = yoffset;
   blit.dst.box.z = zoffset + dstz;
   blit.src.box.width = blit.dst.box.width = width;
   blit.src.box.height = blit.dst.box.height = height;
   blit.src.box.depth = blit.dst.box.depth = depth;
   blit.mask = st_get_blit_mask(format, texImage->_BaseFormat);
   blit.filter = PIPE_TEX_FILTER_NEAREST;
   blit.scissor_enable = FALSE;

   st->pipe->blit(st->pipe, &blit);

   pipe_resource_reference(&src, NULL);
   return;

fallback:
   if (!throttled) {
      util_throttle_memory_usage(pipe, &st->throttle,
                                 (uint64_t) width * height * depth *
                                 _mesa_get_format_bytes(texImage->TexFormat));
   }
   _mesa_store_texsubimage(ctx, dims, texImage, xoffset, yoffset, zoffset,
                           width, height, depth, format, type, pixels,
                           unpack);
}


static void
st_TexImage(struct gl_context * ctx, GLuint dims,
            struct gl_texture_image *texImage,
            GLenum format, GLenum type, const void *pixels,
            const struct gl_pixelstore_attrib *unpack)
{
   assert(dims == 1 || dims == 2 || dims == 3);

   prep_teximage(ctx, texImage, format, type);

   if (texImage->Width == 0 || texImage->Height == 0 || texImage->Depth == 0)
      return;

   /* allocate storage for texture data */
   if (!ctx->Driver.AllocTextureImageBuffer(ctx, texImage)) {
      _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage%uD", dims);
      return;
   }

   st_TexSubImage(ctx, dims, texImage, 0, 0, 0,
                  texImage->Width, texImage->Height, texImage->Depth,
                  format, type, pixels, unpack);
}


static void
st_CompressedTexSubImage(struct gl_context *ctx, GLuint dims,
                         struct gl_texture_image *texImage,
                         GLint x, GLint y, GLint z,
                         GLsizei w, GLsizei h, GLsizei d,
                         GLenum format, GLsizei imageSize, const void *data)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_image *stImage = st_texture_image(texImage);
   struct st_texture_object *stObj = st_texture_object(texImage->TexObject);
   struct pipe_resource *texture = stImage->pt;
   struct pipe_context *pipe = st->pipe;
   struct pipe_screen *screen = st->screen;
   struct pipe_resource *dst = stImage->pt;
   struct pipe_surface *surface = NULL;
   struct compressed_pixelstore store;
   struct st_pbo_addresses addr;
   enum pipe_format copy_format;
   unsigned bw, bh;
   intptr_t buf_offset;
   bool success = false;

   /* Check basic pre-conditions for PBO upload */
   if (!st->prefer_blit_based_texture_transfer) {
      goto fallback;
   }

   if (!ctx->Unpack.BufferObj)
      goto fallback;

   if (st_compressed_format_fallback(st, texImage->TexFormat))
      goto fallback;

   if (!dst) {
      goto fallback;
   }

   if (!st->pbo.upload_enabled ||
       !screen->get_param(screen, PIPE_CAP_SURFACE_REINTERPRET_BLOCKS)) {
      goto fallback;
   }

   /* Choose the pipe format for the upload. */
   addr.bytes_per_pixel = util_format_get_blocksize(dst->format);
   bw = util_format_get_blockwidth(dst->format);
   bh = util_format_get_blockheight(dst->format);

   switch (addr.bytes_per_pixel) {
   case 8:
      copy_format = PIPE_FORMAT_R16G16B16A16_UINT;
      break;
   case 16:
      copy_format = PIPE_FORMAT_R32G32B32A32_UINT;
      break;
   default:
      goto fallback;
   }

   if (!screen->is_format_supported(screen, copy_format, PIPE_BUFFER, 0, 0,
                                    PIPE_BIND_SAMPLER_VIEW)) {
      goto fallback;
   }

   if (!screen->is_format_supported(screen, copy_format, dst->target,
                                    dst->nr_samples, dst->nr_storage_samples,
                                    PIPE_BIND_RENDER_TARGET)) {
      goto fallback;
   }

   /* Interpret the pixelstore settings. */
   _mesa_compute_compressed_pixelstore(dims, texImage->TexFormat, w, h, d,
                                       &ctx->Unpack, &store);
   assert(store.CopyBytesPerRow % addr.bytes_per_pixel == 0);
   assert(store.SkipBytes % addr.bytes_per_pixel == 0);

   /* Compute the offset into the buffer */
   buf_offset = (intptr_t)data + store.SkipBytes;

   if (buf_offset % addr.bytes_per_pixel) {
      goto fallback;
   }

   buf_offset = buf_offset / addr.bytes_per_pixel;

   addr.xoffset = x / bw;
   addr.yoffset = y / bh;
   addr.width = store.CopyBytesPerRow / addr.bytes_per_pixel;
   addr.height = store.CopyRowsPerSlice;
   addr.depth = d;
   addr.pixels_per_row = store.TotalBytesPerRow / addr.bytes_per_pixel;
   addr.image_height = store.TotalRowsPerSlice;

   if (!st_pbo_addresses_setup(st,
                               st_buffer_object(ctx->Unpack.BufferObj)->buffer,
                               buf_offset, &addr))
      goto fallback;

   /* Set up the surface. */
   {
      unsigned level = stObj->pt != stImage->pt
         ? 0 : texImage->TexObject->Attrib.MinLevel + texImage->Level;
      unsigned max_layer = util_max_layer(texture, level);

      GLint layer = z + texImage->Face + texImage->TexObject->Attrib.MinLayer;

      struct pipe_surface templ;
      memset(&templ, 0, sizeof(templ));
      templ.format = copy_format;
      templ.u.tex.level = level;
      templ.u.tex.first_layer = MIN2(layer, max_layer);
      templ.u.tex.last_layer = MIN2(layer + d - 1, max_layer);

      surface = pipe->create_surface(pipe, texture, &templ);
      if (!surface)
         goto fallback;
   }

   success = try_pbo_upload_common(ctx, surface, &addr, copy_format);

   pipe_surface_reference(&surface, NULL);

   if (success)
      return;

fallback:
   _mesa_store_compressed_texsubimage(ctx, dims, texImage,
                                      x, y, z, w, h, d,
                                      format, imageSize, data);
}


static void
st_CompressedTexImage(struct gl_context *ctx, GLuint dims,
                      struct gl_texture_image *texImage,
                      GLsizei imageSize, const void *data)
{
   prep_teximage(ctx, texImage, GL_NONE, GL_NONE);

   /* only 2D and 3D compressed images are supported at this time */
   if (dims == 1) {
      _mesa_problem(ctx, "Unexpected glCompressedTexImage1D call");
      return;
   }

   /* This is pretty simple, because unlike the general texstore path we don't
    * have to worry about the usual image unpacking or image transfer
    * operations.
    */
   assert(texImage);
   assert(texImage->Width > 0);
   assert(texImage->Height > 0);
   assert(texImage->Depth > 0);

   /* allocate storage for texture data */
   if (!st_AllocTextureImageBuffer(ctx, texImage)) {
      _mesa_error(ctx, GL_OUT_OF_MEMORY, "glCompressedTexImage%uD", dims);
      return;
   }

   st_CompressedTexSubImage(ctx, dims, texImage,
                            0, 0, 0,
                            texImage->Width, texImage->Height, texImage->Depth,
                            texImage->TexFormat,
                            imageSize, data);
}


struct pbo_shader_data {
   nir_ssa_def *offset;
   nir_ssa_def *range;
   nir_ssa_def *invert;
   nir_ssa_def *blocksize;
   nir_ssa_def *alignment;
   nir_ssa_def *dst_bit_size;
   nir_ssa_def *channels;
   nir_ssa_def *normalized;
   nir_ssa_def *integer;
   nir_ssa_def *clamp_uint;
   nir_ssa_def *r11g11b10_or_sint;
   nir_ssa_def *r9g9b9e5;
   nir_ssa_def *bits1;
   nir_ssa_def *bits2;
   nir_ssa_def *bits3;
   nir_ssa_def *bits4;
   nir_ssa_def *swap;
   nir_ssa_def *bits; //vec4
};


/* must be under 16bytes / sizeof(vec4) / 128 bits) */
struct pbo_data {
   uint16_t x, y; //32

   uint16_t width, height, depth; //48

   /* 80 */
   uint8_t invert : 1;
   uint8_t blocksize : 7;

   /* 88 */
   uint8_t clamp_uint : 1;
   uint8_t r11g11b10_or_sint : 1; //
   uint8_t r9g9b9e5 : 1;
   uint8_t swap : 1;
   uint16_t alignment : 2;
   uint8_t dst_bit_size : 2; //8, 16, 32, 64

   /* 96 */
   uint8_t channels : 2;
   uint8_t bits1 : 6;

   /* 104 */
   uint8_t normalized : 1;
   uint8_t integer : 1;
   uint8_t bits2 : 6;

   /* 112 */
   uint8_t bits3 : 6;
   uint8_t pad1 : 2;

   /* 120 */
   uint8_t bits4 : 6;
   uint8_t pad2 : 2;
};


#define STRUCT_OFFSET(name) (offsetof(struct pbo_data, name) * 8)

#define STRUCT_BLOCK(offset, ...) \
   do { \
      assert(offset % 8 == 0); \
      nir_ssa_def *block##offset = nir_u2u32(b, nir_extract_bits(b, &ubo_load, 1, (offset), 1, 8)); \
      __VA_ARGS__ \
   } while (0)
#define STRUCT_MEMBER(blockoffset, name, offset, size, op, clamp) \
   do { \
      assert(offset + size <= 8); \
      nir_ssa_def *val = nir_iand_imm(b, block##blockoffset, u_bit_consecutive(offset, size)); \
      if (offset) \
         val = nir_ushr_imm(b, val, offset); \
      sd->name = op; \
      if (clamp) \
         sd->name = nir_umin(b, sd->name, nir_imm_int(b, clamp)); \
   } while (0)
#define STRUCT_MEMBER_SHIFTED_2BIT(blockoffset, name, offset, shift, clamp) \
   STRUCT_MEMBER(blockoffset, name, offset, 2, nir_ishl(b, nir_imm_int(b, shift), val), clamp)

#define STRUCT_MEMBER_BOOL(blockoffset, name, offset) \
   STRUCT_MEMBER(blockoffset, name, offset, 1, nir_ieq_imm(b, val, 1), 0)

static void
init_pbo_shader_data(nir_builder *b, struct pbo_shader_data *sd)
{
   nir_variable *ubo = nir_variable_create(b->shader, nir_var_uniform, glsl_uvec4_type(), "offset");
   nir_ssa_def *ubo_load = nir_load_var(b, ubo);

   sd->offset = nir_umin(b, nir_u2u32(b, nir_extract_bits(b, &ubo_load, 1, STRUCT_OFFSET(x), 2, 16)), nir_imm_int(b, 65535));
   sd->range = nir_umin(b, nir_u2u32(b, nir_extract_bits(b, &ubo_load, 1, STRUCT_OFFSET(width), 3, 16)), nir_imm_int(b, 65535));

   STRUCT_BLOCK(80,
      STRUCT_MEMBER_BOOL(80, invert, 0);
      STRUCT_MEMBER(80, blocksize, 1, 7, nir_iadd_imm(b, val, 1), 128);
   );

   STRUCT_BLOCK(88,
      STRUCT_MEMBER_BOOL(88, clamp_uint, 0);
      STRUCT_MEMBER_BOOL(88, r11g11b10_or_sint, 1);
      STRUCT_MEMBER_BOOL(88, r9g9b9e5, 2);
      STRUCT_MEMBER_BOOL(88, swap, 3);
      STRUCT_MEMBER_SHIFTED_2BIT(88, alignment, 4, 1, 8);
      STRUCT_MEMBER_SHIFTED_2BIT(88, dst_bit_size, 6, 8, 64);
   );

   STRUCT_BLOCK(96,
      STRUCT_MEMBER(96, channels, 0, 2, nir_iadd_imm(b, val, 1), 4);
      STRUCT_MEMBER(96, bits1, 2, 6, val, 32);
   );

   STRUCT_BLOCK(104,
      STRUCT_MEMBER_BOOL(104, normalized, 0);
      STRUCT_MEMBER_BOOL(104, integer, 1);
      STRUCT_MEMBER(104, bits2, 2, 6, val, 32);
   );


   STRUCT_BLOCK(112,
      STRUCT_MEMBER(112, bits3, 0, 6, val, 32);
   );

   STRUCT_BLOCK(120,
      STRUCT_MEMBER(120, bits4, 0, 6, val, 32);
   );
   sd->bits = nir_vec4(b, sd->bits1, sd->bits2, sd->bits3, sd->bits4);

   /* clamp swap in the shader to enable better optimizing */
   /* TODO?
   sd->swap = nir_bcsel(b, nir_ior(b,
                                   nir_ieq_imm(b, sd->blocksize, 8),
                                   nir_bcsel(b,
                                             nir_ieq_imm(b, sd->bits1, 8),
                                             nir_bcsel(b,
                                                       nir_uge(b, sd->channels, nir_imm_int(b, 2)),
                                                       nir_bcsel(b,
                                                                 nir_uge(b, sd->channels, nir_imm_int(b, 3)),
                                                                 nir_bcsel(b,
                                                                           nir_ieq(b, sd->channels, nir_imm_int(b, 4)),
                                                                           nir_ball(b, nir_ieq(b, sd->bits, nir_imm_ivec4(b, 8, 8, 8, 8))),
                                                                           nir_ball(b, nir_ieq(b, nir_channels(b, sd->bits, 7), nir_imm_ivec3(b, 8, 8, 8)))),
                                                                 nir_ball(b, nir_ieq(b, nir_channels(b, sd->bits, 3), nir_imm_ivec2(b, 8, 8)))),
                                                       nir_imm_bool(b, 0)),
                                             nir_imm_bool(b, 0))),
                           nir_imm_bool(b, 0),
                           sd->swap);
     */
}

static unsigned
fill_pbo_data(struct pbo_data *pd, enum pipe_format src_format, enum pipe_format dst_format, bool swap)
{
   unsigned bits[4] = {0};
   bool weird_packed = false;
   const struct util_format_description *dst_desc = util_format_description(dst_format);
   bool is_8bit = true;

   for (unsigned c = 0; c < 4; c++) {
      bits[c] = dst_desc->channel[c].size;
      if (c < dst_desc->nr_channels) {
         weird_packed |= bits[c] != bits[0] || bits[c] % 8 != 0;
         if (bits[c] != 8)
            is_8bit = false;
      }
   }

   if (is_8bit || dst_desc->block.bits == 8)
      swap = false;

   unsigned dst_bit_size = 0;
   if (weird_packed) {
      dst_bit_size = dst_desc->block.bits;
   } else {
      dst_bit_size = dst_desc->block.bits / dst_desc->nr_channels;
   }
   assert(dst_bit_size);
   assert(dst_bit_size <= 64);

   pd->dst_bit_size = dst_bit_size >> 4;
   pd->channels = dst_desc->nr_channels - 1;
   pd->normalized = dst_desc->is_unorm || dst_desc->is_snorm;
   pd->clamp_uint = dst_desc->is_unorm ||
                    (util_format_is_pure_sint(dst_format) &&
                     !util_format_is_pure_sint(src_format) &&
                     !util_format_is_snorm(src_format)) ||
                    util_format_is_pure_uint(dst_format);
   pd->integer = util_format_is_pure_uint(dst_format) || util_format_is_pure_sint(dst_format);
   pd->r11g11b10_or_sint = dst_format == PIPE_FORMAT_R11G11B10_FLOAT || util_format_is_pure_sint(dst_format);
   pd->r9g9b9e5 = dst_format == PIPE_FORMAT_R9G9B9E5_FLOAT;
   pd->bits1 = bits[0];
   pd->bits2 = bits[1];
   pd->bits3 = bits[2];
   pd->bits4 = bits[3];
   pd->swap = swap;

   return weird_packed ? 1 : dst_desc->nr_channels;
}

static nir_ssa_def *
get_buffer_offset(nir_builder *b, nir_ssa_def *coord, struct pbo_shader_data *sd)
{
/* from _mesa_image_offset():
      offset = topOfImage
               + (skippixels + column) * bytes_per_pixel
               + (skiprows + row) * bytes_per_row
               + (skipimages + img) * bytes_per_image;
 */
   nir_ssa_def *bytes_per_row = nir_imul(b, nir_channel(b, sd->range, 0), sd->blocksize);
   bytes_per_row = nir_bcsel(b, nir_ult(b, sd->alignment, nir_imm_int(b, 2)),
                             bytes_per_row,
                             nir_iand(b,
                                      nir_isub(b, nir_iadd(b, bytes_per_row, sd->alignment), nir_imm_int(b, 1)),
                                      nir_inot(b, nir_isub(b, sd->alignment, nir_imm_int(b, 1)))));
   nir_ssa_def *bytes_per_image = nir_imul(b, bytes_per_row, nir_channel(b, sd->range, 1));
   bytes_per_row = nir_bcsel(b, sd->invert,
                             nir_isub(b, nir_imm_int(b, 0), bytes_per_row),
                             bytes_per_row);
   return nir_iadd(b,
                   nir_imul(b, nir_channel(b, coord, 0), sd->blocksize),
                   nir_iadd(b,
                            nir_imul(b, nir_channel(b, coord, 1), bytes_per_row),
                            nir_imul(b, nir_channel(b, coord, 2), bytes_per_image)));
}

static inline void
write_ssbo(nir_builder *b, nir_ssa_def *pixel, nir_ssa_def *buffer_offset)
{
   nir_store_ssbo(b, pixel, nir_imm_zero(b, 1, 32), buffer_offset,
                  .align_mul = pixel->bit_size / 8,
                  .write_mask = (1 << pixel->num_components) - 1);
}

static void
write_conversion(nir_builder *b, nir_ssa_def *pixel, nir_ssa_def *buffer_offset, struct pbo_shader_data *sd)
{
   nir_push_if(b, nir_ilt(b, sd->dst_bit_size, nir_imm_int(b, 32)));
      nir_push_if(b, nir_ieq_imm(b, sd->dst_bit_size, 16));
         write_ssbo(b, nir_u2u16(b, pixel), buffer_offset);
      nir_push_else(b, NULL);
         write_ssbo(b, nir_u2u8(b, pixel), buffer_offset);
      nir_pop_if(b, NULL);
   nir_push_else(b, NULL);
      write_ssbo(b, pixel, buffer_offset);
   nir_pop_if(b, NULL);
}

static nir_ssa_def *
swap2(nir_builder *b, nir_ssa_def *src)
{
   /* dst[i] = (src[i] >> 8) | ((src[i] << 8) & 0xff00); */
   return nir_ior(b,
                  nir_ushr_imm(b, src, 8),
                  nir_iand_imm(b, nir_ishl(b, src, nir_imm_int(b, 8)), 0xff00));
}

static nir_ssa_def *
swap4(nir_builder *b, nir_ssa_def *src)
{
   /* a = (b >> 24) | ((b >> 8) & 0xff00) | ((b << 8) & 0xff0000) | ((b << 24) & 0xff000000); */
   return nir_ior(b,
                  /* (b >> 24) */
                  nir_ushr_imm(b, src, 24),
                  nir_ior(b,
                          /* ((b >> 8) & 0xff00) */
                          nir_iand(b, nir_ushr_imm(b, src, 8), nir_imm_int(b, 0xff00)),
                          nir_ior(b,
                                  /* ((b << 8) & 0xff0000) */
                                  nir_iand(b, nir_ishl(b, src, nir_imm_int(b, 8)), nir_imm_int(b, 0xff0000)),
                                  /* ((b << 24) & 0xff000000) */
                                  nir_iand(b, nir_ishl(b, src, nir_imm_int(b, 24)), nir_imm_int(b, 0xff000000)))));
}

/* explode the cf to handle channel counts in the shader */
static void
grab_components(nir_builder *b, nir_ssa_def *pixel, nir_ssa_def *buffer_offset, struct pbo_shader_data *sd, bool weird_packed)
{
   if (weird_packed) {
      nir_push_if(b, nir_ieq_imm(b, sd->bits1, 32));
         write_conversion(b, nir_channels(b, pixel, 3), buffer_offset, sd);
      nir_push_else(b, NULL);
         write_conversion(b, nir_channel(b, pixel, 0), buffer_offset, sd);
      nir_pop_if(b, NULL);
   } else {
      nir_push_if(b, nir_ieq_imm(b, sd->channels, 1));
         write_conversion(b, nir_channel(b, pixel, 0), buffer_offset, sd);
      nir_push_else(b, NULL);
         nir_push_if(b, nir_ieq_imm(b, sd->channels, 2));
            write_conversion(b, nir_channels(b, pixel, (1 << 2) - 1), buffer_offset, sd);
         nir_push_else(b, NULL);
            nir_push_if(b, nir_ieq_imm(b, sd->channels, 3));
               write_conversion(b, nir_channels(b, pixel, (1 << 3) - 1), buffer_offset, sd);
            nir_push_else(b, NULL);
               write_conversion(b, nir_channels(b, pixel, (1 << 4) - 1), buffer_offset, sd);
            nir_pop_if(b, NULL);
         nir_pop_if(b, NULL);
      nir_pop_if(b, NULL);
   }
}

/* if byteswap is enabled, handle that and then write the components */
static void
handle_swap(nir_builder *b, nir_ssa_def *pixel, nir_ssa_def *buffer_offset,
            struct pbo_shader_data *sd, unsigned num_components, bool weird_packed)
{
   nir_push_if(b, sd->swap); {
      nir_push_if(b, nir_ieq_imm(b, nir_udiv_imm(b, sd->blocksize, num_components), 2)); {
         /* this is a single high/low swap per component */
         nir_ssa_def *components[4];
         for (unsigned i = 0; i < 4; i++)
            components[i] = swap2(b, nir_channel(b, pixel, i));
         nir_ssa_def *v = nir_vec(b, components, 4);
         grab_components(b, v, buffer_offset, sd, weird_packed);
      } nir_push_else(b, NULL); {
         /* this is a pair of high/low swaps for each half of the component */
         nir_ssa_def *components[4];
         for (unsigned i = 0; i < 4; i++)
            components[i] = swap4(b, nir_channel(b, pixel, i));
         nir_ssa_def *v = nir_vec(b, components, 4);
         grab_components(b, v, buffer_offset, sd, weird_packed);
      } nir_pop_if(b, NULL);
   } nir_push_else(b, NULL); {
      /* swap disabled */
      grab_components(b, pixel, buffer_offset, sd, weird_packed);
   } nir_pop_if(b, NULL);
}

static nir_ssa_def *
check_for_weird_packing(nir_builder *b, struct pbo_shader_data *sd, unsigned component)
{
   nir_ssa_def *c = nir_channel(b, sd->bits, component - 1);

   return nir_bcsel(b,
                    nir_ige(b, sd->channels, nir_imm_int(b, component)),
                    nir_ior(b,
                            nir_ine(b, c, sd->bits1),
                            nir_ine(b, nir_imod(b, c, nir_imm_int(b, 8)), nir_imm_int(b, 0))),
                    nir_imm_bool(b, 0));
}

/* convenience function for clamping signed integers */
static inline nir_ssa_def *
nir_imin_imax(nir_builder *build, nir_ssa_def *src, nir_ssa_def *clamp_to_min, nir_ssa_def *clamp_to_max)
{
   return nir_imax(build, nir_imin(build, src, clamp_to_min), clamp_to_max);
}

static inline nir_ssa_def *
nir_format_float_to_unorm_with_factor(nir_builder *b, nir_ssa_def *f, nir_ssa_def *factor)
{
   /* Clamp to the range [0, 1] */
   f = nir_fsat(b, f);

   return nir_f2u32(b, nir_fround_even(b, nir_fmul(b, f, factor)));
}

static inline nir_ssa_def *
nir_format_float_to_snorm_with_factor(nir_builder *b, nir_ssa_def *f, nir_ssa_def *factor)
{
   /* Clamp to the range [-1, 1] */
   f = nir_fmin(b, nir_fmax(b, f, nir_imm_float(b, -1)), nir_imm_float(b, 1));

   return nir_f2i32(b, nir_fround_even(b, nir_fmul(b, f, factor)));
}

static nir_ssa_def *
clamp_and_mask(nir_builder *b, nir_ssa_def *src, nir_ssa_def *channels)
{
   nir_ssa_def *one = nir_imm_ivec4(b, 1, 0, 0, 0);
   nir_ssa_def *two = nir_imm_ivec4(b, 1, 1, 0, 0);
   nir_ssa_def *three = nir_imm_ivec4(b, 1, 1, 1, 0);
   nir_ssa_def *four = nir_imm_ivec4(b, 1, 1, 1, 1);
   /* avoid underflow by clamping to channel count */
   src = nir_bcsel(b,
                   nir_ieq(b, channels, one),
                   nir_isub(b, src, one),
                   nir_bcsel(b,
                             nir_ieq_imm(b, channels, 2),
                             nir_isub(b, src, two),
                             nir_bcsel(b,
                                       nir_ieq_imm(b, channels, 3),
                                       nir_isub(b, src, three),
                                       nir_isub(b, src, four))));

   return nir_mask(b, src, 32);
}

static void
convert_swap_write(nir_builder *b, nir_ssa_def *pixel, nir_ssa_def *buffer_offset,
                   unsigned num_components,
                   struct pbo_shader_data *sd)
{

   nir_ssa_def *weird_packed = nir_ior(b,
                                       nir_ior(b,
                                               check_for_weird_packing(b, sd, 4),
                                               check_for_weird_packing(b, sd, 3)),
                                       check_for_weird_packing(b, sd, 2));
   if (num_components == 1) {
      nir_push_if(b, weird_packed);
         nir_push_if(b, sd->r11g11b10_or_sint);
            handle_swap(b, nir_pad_vec4(b, nir_format_pack_11f11f10f(b, pixel)), buffer_offset, sd, 1, true);
         nir_push_else(b, NULL);
            nir_push_if(b, sd->r9g9b9e5);
               handle_swap(b, nir_pad_vec4(b, nir_format_pack_r9g9b9e5(b, pixel)), buffer_offset, sd, 1, true);
            nir_push_else(b, NULL);
               nir_push_if(b, nir_ieq_imm(b, sd->bits1, 32)); { //PIPE_FORMAT_Z32_FLOAT_S8X24_UINT
                  nir_ssa_def *pack[2];
                  pack[0] = nir_format_pack_uint_unmasked_ssa(b, nir_channel(b, pixel, 0), nir_channel(b, sd->bits, 0));
                  pack[1] = nir_format_pack_uint_unmasked_ssa(b, nir_channels(b, pixel, 6), nir_channels(b, sd->bits, 6));
                  handle_swap(b, nir_pad_vec4(b, nir_vec2(b, pack[0], pack[1])), buffer_offset, sd, 2, true);
               } nir_push_else(b, NULL);
                  handle_swap(b, nir_pad_vec4(b, nir_format_pack_uint_unmasked_ssa(b, pixel, sd->bits)), buffer_offset, sd, 1, true);
               nir_pop_if(b, NULL);
            nir_pop_if(b, NULL);
         nir_pop_if(b, NULL);
      nir_push_else(b, NULL);
         handle_swap(b, pixel, buffer_offset, sd, num_components, false);
      nir_pop_if(b, NULL);
   } else {
      nir_push_if(b, weird_packed);
         handle_swap(b, pixel, buffer_offset, sd, num_components, true);
      nir_push_else(b, NULL);
         handle_swap(b, pixel, buffer_offset, sd, num_components, false);
      nir_pop_if(b, NULL);
   }
}

static void
do_shader_conversion(nir_builder *b, nir_ssa_def *pixel,
                     unsigned num_components,
                     nir_ssa_def *coord, struct pbo_shader_data *sd)
{
   nir_ssa_def *buffer_offset = get_buffer_offset(b, coord, sd);

   nir_ssa_def *signed_bit_mask = clamp_and_mask(b, sd->bits, sd->channels);

#define CONVERT_SWAP_WRITE(PIXEL) \
   convert_swap_write(b, PIXEL, buffer_offset, num_components, sd);
   nir_push_if(b, sd->normalized);
      nir_push_if(b, sd->clamp_uint); //unorm
         CONVERT_SWAP_WRITE(nir_format_float_to_unorm_with_factor(b, pixel, nir_u2f32(b, nir_mask(b, sd->bits, 32))));
      nir_push_else(b, NULL);
         CONVERT_SWAP_WRITE(nir_format_float_to_snorm_with_factor(b, pixel, nir_u2f32(b, signed_bit_mask)));
      nir_pop_if(b, NULL);
   nir_push_else(b, NULL);
      nir_push_if(b, sd->integer);
         nir_push_if(b, sd->r11g11b10_or_sint); //sint
            nir_push_if(b, sd->clamp_uint); //uint -> sint
               CONVERT_SWAP_WRITE(nir_umin(b, pixel, signed_bit_mask));
            nir_push_else(b, NULL);
               CONVERT_SWAP_WRITE(nir_imin_imax(b, pixel, signed_bit_mask, nir_isub(b, nir_ineg(b, signed_bit_mask), nir_imm_int(b, 1))));
            nir_pop_if(b, NULL);
         nir_push_else(b, NULL);
            nir_push_if(b, sd->clamp_uint); //uint
               /* nir_format_clamp_uint */
               CONVERT_SWAP_WRITE(nir_umin(b, pixel, nir_mask(b, sd->bits, 32)));
            nir_pop_if(b, NULL);
         nir_pop_if(b, NULL);
      nir_push_else(b, NULL);
         nir_push_if(b, nir_ieq_imm(b, sd->bits1, 16)); //half
            CONVERT_SWAP_WRITE(nir_format_float_to_half(b, pixel));
         nir_push_else(b, NULL);
            CONVERT_SWAP_WRITE(pixel);
         nir_pop_if(b, NULL);
   nir_pop_if(b, NULL);
}

/* TODO: unify with st_pbo.c */
static const struct glsl_type *
sampler_type_for_target(enum pipe_texture_target target)
{
   bool is_array = target >= PIPE_TEXTURE_1D_ARRAY;
   static const enum glsl_sampler_dim dim[] = {
      [PIPE_BUFFER]             = GLSL_SAMPLER_DIM_BUF,
      [PIPE_TEXTURE_1D]         = GLSL_SAMPLER_DIM_1D,
      [PIPE_TEXTURE_2D]         = GLSL_SAMPLER_DIM_2D,
      [PIPE_TEXTURE_3D]         = GLSL_SAMPLER_DIM_3D,
      [PIPE_TEXTURE_CUBE]       = GLSL_SAMPLER_DIM_CUBE,
      [PIPE_TEXTURE_RECT]       = GLSL_SAMPLER_DIM_RECT,
      [PIPE_TEXTURE_1D_ARRAY]   = GLSL_SAMPLER_DIM_1D,
      [PIPE_TEXTURE_2D_ARRAY]   = GLSL_SAMPLER_DIM_2D,
      [PIPE_TEXTURE_CUBE_ARRAY] = GLSL_SAMPLER_DIM_CUBE,
   };

   return glsl_sampler_type(dim[target], false, is_array, GLSL_TYPE_FLOAT);
}

static void *
create_conversion_shader(struct st_context *st, enum pipe_texture_target target, unsigned num_components)
{
   const nir_shader_compiler_options *options = st_get_nir_compiler_options(st, MESA_SHADER_COMPUTE);
   nir_builder b = nir_builder_init_simple_shader(MESA_SHADER_COMPUTE, options, "%s", "convert");
   b.shader->info.workgroup_size[0] = target != PIPE_TEXTURE_1D ? 8 : 64;
   b.shader->info.workgroup_size[1] = target != PIPE_TEXTURE_1D ? 8 : 1;

   b.shader->info.workgroup_size[2] = 1;
   b.shader->info.textures_used[0] = 1;
   b.shader->info.num_ssbos = 1;
   b.shader->num_uniforms = 2;
   nir_variable *ssbo = nir_variable_create(b.shader, nir_var_mem_ssbo, glsl_array_type(glsl_float_type(), 0, 4), "ssbo");
   nir_variable *sampler = nir_variable_create(b.shader, nir_var_uniform, sampler_type_for_target(target), "sampler");
   unsigned coord_components = glsl_get_sampler_coordinate_components(sampler->type);
   sampler->data.explicit_binding = 1;

   struct pbo_shader_data sd;
   init_pbo_shader_data(&b, &sd);

   nir_ssa_def *bsize = nir_imm_ivec4(&b,
                                      b.shader->info.workgroup_size[0],
                                      b.shader->info.workgroup_size[1],
                                      b.shader->info.workgroup_size[2],
                                      0);
   nir_ssa_def *wid = nir_load_workgroup_id(&b, 32);
   nir_ssa_def *iid = nir_load_local_invocation_id(&b);
   nir_ssa_def *tile = nir_imul(&b, wid, bsize);
   nir_ssa_def *global_id = nir_iadd(&b, tile, iid);
   nir_ssa_def *start = nir_iadd(&b, global_id, sd.offset);

   nir_ssa_def *coord = nir_channels(&b, start, (1<<coord_components)-1);
   nir_ssa_def *max = nir_iadd(&b, sd.offset, sd.range);
   nir_push_if(&b, nir_ball(&b, nir_ilt(&b, coord, nir_channels(&b, max, (1<<coord_components)-1))));
   nir_tex_instr *txf = nir_tex_instr_create(b.shader, 3);
   txf->is_array = glsl_sampler_type_is_array(sampler->type);
   txf->op = nir_texop_txf;
   txf->sampler_dim = glsl_get_sampler_dim(sampler->type);
   txf->dest_type = nir_type_float32;
   txf->coord_components = coord_components;
   txf->texture_index = 0;
   txf->sampler_index = 0;
   txf->src[0].src_type = nir_tex_src_coord;
   txf->src[0].src = nir_src_for_ssa(coord);
   txf->src[1].src_type = nir_tex_src_lod;
   txf->src[1].src = nir_src_for_ssa(nir_imm_int(&b, 0));
   txf->src[2].src_type = nir_tex_src_texture_deref;
   nir_deref_instr *sampler_deref = nir_build_deref_var(&b, sampler);
   txf->src[2].src = nir_src_for_ssa(&sampler_deref->dest.ssa);

   nir_ssa_dest_init(&txf->instr, &txf->dest, 4, 32, NULL);
   nir_builder_instr_insert(&b, &txf->instr);

   /* pass the grid offset as the coord to get the zero-indexed buffer offset */
   do_shader_conversion(&b, &txf->dest.ssa, num_components, global_id, &sd);

   nir_pop_if(&b, NULL);


   nir_validate_shader(b.shader, NULL);
   st_nir_opts(b.shader);
   return st_nir_finish_builtin_shader(st, b.shader);
}

static void
invert_swizzle(uint8_t *out, const uint8_t *in)
{
        /* First, default to all zeroes to prevent uninitialized junk */

        for (unsigned c = 0; c < 4; ++c)
                out[c] = PIPE_SWIZZLE_0;

        /* Now "do" what the swizzle says */

        for (unsigned c = 0; c < 4; ++c) {
                unsigned char i = in[c];

                /* Who cares? */
                assert(PIPE_SWIZZLE_X == 0);
                if (i > PIPE_SWIZZLE_W)
                        continue;
                /* Invert */
                unsigned idx = i - PIPE_SWIZZLE_X;
                out[idx] = PIPE_SWIZZLE_X + c;
        }
}

static uint32_t
compute_shader_key(enum pipe_texture_target target, unsigned num_components)
{
   uint8_t key_target[] = {
      [PIPE_BUFFER] = UINT8_MAX,
      [PIPE_TEXTURE_1D] = 1,
      [PIPE_TEXTURE_2D] = 2,
      [PIPE_TEXTURE_3D] = 3,
      [PIPE_TEXTURE_CUBE] = 4,
      [PIPE_TEXTURE_RECT] = UINT8_MAX,
      [PIPE_TEXTURE_1D_ARRAY] = 5,
      [PIPE_TEXTURE_2D_ARRAY] = 6,
      [PIPE_TEXTURE_CUBE_ARRAY] = UINT8_MAX,
   };
   assert(target < ARRAY_SIZE(key_target));
   assert(key_target[target] != UINT8_MAX);
   return key_target[target] | (num_components << 3);
}

static unsigned
get_dim_from_target(enum pipe_texture_target target)
{
   switch (target) {
   case PIPE_TEXTURE_1D:
      return 1;
   case PIPE_TEXTURE_2D_ARRAY:
   case PIPE_TEXTURE_3D:
      return 3;
   default:
      return 2;
   }
}

static enum pipe_texture_target
get_target_from_texture(struct pipe_resource *src)
{
   enum pipe_texture_target view_target;
   switch (src->target) {
   case PIPE_TEXTURE_RECT:
      view_target = PIPE_TEXTURE_2D;
      break;
   case PIPE_TEXTURE_CUBE:
   case PIPE_TEXTURE_CUBE_ARRAY:
      view_target = PIPE_TEXTURE_2D_ARRAY;
      break;
   default:
      view_target = src->target;
      break;
   }
   return view_target;
}

/* force swizzling behavior for sampling */
enum swizzle_clamp {
   /* force component selection for named format */
   SWIZZLE_CLAMP_LUMINANCE = 1,
   SWIZZLE_CLAMP_ALPHA = 2,
   SWIZZLE_CLAMP_LUMINANCE_ALPHA = 3,
   SWIZZLE_CLAMP_INTENSITY = 4,
   SWIZZLE_CLAMP_RGBX = 5,

   /* select only 1 component */
   SWIZZLE_CLAMP_GREEN = 8,
   SWIZZLE_CLAMP_BLUE = 16,

   /* reverse ordering for format emulation */
   SWIZZLE_CLAMP_BGRA = 32,
};

static struct pipe_resource *
download_texture_compute(struct st_context *st,
                         const struct gl_pixelstore_attrib *pack,
                         GLint xoffset, GLint yoffset, GLint zoffset,
                         GLsizei width, GLsizei height, GLint depth,
                         unsigned level, unsigned layer,
                         GLenum format, GLenum type,
                         enum pipe_format src_format,
                         enum pipe_texture_target view_target,
                         struct pipe_resource *src,
                         enum pipe_format dst_format,
                         enum swizzle_clamp swizzle_clamp)
{
   struct pipe_context *pipe = st->pipe;
   struct pipe_screen *screen = st->screen;
   struct pipe_resource *dst = NULL;
   unsigned dim = get_dim_from_target(view_target);

   /* clamp 3d offsets based on slice */
   if (view_target == PIPE_TEXTURE_3D)
      zoffset += layer;

   unsigned num_components = 0;
   /* Upload constants */
   {
      struct pipe_constant_buffer cb;
      struct pbo_data pd = {
         .x = xoffset,
         .y = view_target == PIPE_TEXTURE_1D_ARRAY ? 0 : yoffset,
         .width = width, .height = height, .depth = depth,
         .invert = pack->Invert,
         .blocksize = util_format_get_blocksize(dst_format) - 1,
         .alignment = ffs(MAX2(pack->Alignment, 1)) - 1,
      };
      num_components = fill_pbo_data(&pd, src_format, dst_format, pack->SwapBytes == 1);

      if (st->pbo.constants_map)
         memcpy(st->pbo.constants_map, &pd, sizeof(pd));
      else
         pipe_buffer_write(st->pipe, st->pbo.constants, 0, sizeof(pd), &pd);
      cb.buffer = st->pbo.constants;
      cb.user_buffer = NULL;
      cb.buffer_offset = 0;
      cb.buffer_size = sizeof(pd);

      pipe->set_constant_buffer(pipe, PIPE_SHADER_COMPUTE, 0, false, &cb);
   }

   uint32_t hash_key = compute_shader_key(view_target, num_components);
   assert(hash_key != 0);

   struct hash_entry *he = _mesa_hash_table_search(st->pbo.shaders, (void*)(uintptr_t)hash_key);
   void *cs;
   if (!he) {
      cs = create_conversion_shader(st, view_target, num_components);
      he = _mesa_hash_table_insert(st->pbo.shaders, (void*)(uintptr_t)hash_key, cs);
   }
   cs = he->data;
   assert(cs);
   struct cso_context *cso = st->cso_context;

   cso_save_compute_state(cso, CSO_BIT_COMPUTE_SHADER | CSO_BIT_COMPUTE_SAMPLERS);
   cso_set_compute_shader_handle(cso, cs);

   /* Set up the sampler_view */
   {
      struct pipe_sampler_view templ;
      struct pipe_sampler_view *sampler_view;
      struct pipe_sampler_state sampler = {0};
      const struct pipe_sampler_state *samplers[1] = {&sampler};
      const struct util_format_description *desc = util_format_description(dst_format);

      u_sampler_view_default_template(&templ, src, src_format);
      if (util_format_is_depth_or_stencil(dst_format)) {
         templ.swizzle_r = PIPE_SWIZZLE_X;
         templ.swizzle_g = PIPE_SWIZZLE_X;
         templ.swizzle_b = PIPE_SWIZZLE_X;
         templ.swizzle_a = PIPE_SWIZZLE_X;
      } else {
         uint8_t invswizzle[4];
         const uint8_t *swizzle;

         /* these swizzle output bits require explicit component selection/ordering */
         if (swizzle_clamp & SWIZZLE_CLAMP_GREEN) {
            for (unsigned i = 0; i < 4; i++)
               invswizzle[i] = PIPE_SWIZZLE_Y;
         } else if (swizzle_clamp & SWIZZLE_CLAMP_BLUE) {
            for (unsigned i = 0; i < 4; i++)
               invswizzle[i] = PIPE_SWIZZLE_Z;
         } else {
            if (swizzle_clamp & SWIZZLE_CLAMP_BGRA) {
               if (util_format_get_nr_components(dst_format) == 3)
                  swizzle = util_format_description(PIPE_FORMAT_B8G8R8_UNORM)->swizzle;
               else
                  swizzle = util_format_description(PIPE_FORMAT_B8G8R8A8_UNORM)->swizzle;
            } else
               swizzle = desc->swizzle;
            invert_swizzle(invswizzle, swizzle);
         }
         swizzle_clamp &= ~(SWIZZLE_CLAMP_BGRA | SWIZZLE_CLAMP_GREEN | SWIZZLE_CLAMP_BLUE);

         /* these swizzle input modes clamp unused components to 0 and (sometimes) alpha to 1 */
         switch (swizzle_clamp) {
         case SWIZZLE_CLAMP_LUMINANCE:
            if (util_format_is_luminance(dst_format))
               break;
            for (unsigned i = 0; i < 4; i++) {
               if (invswizzle[i] != PIPE_SWIZZLE_X)
                  invswizzle[i] = invswizzle[i] == PIPE_SWIZZLE_W ? PIPE_SWIZZLE_1 : PIPE_SWIZZLE_0;
            }
            break;
         case SWIZZLE_CLAMP_ALPHA:
            for (unsigned i = 0; i < 4; i++) {
               if (invswizzle[i] != PIPE_SWIZZLE_W)
                  invswizzle[i] = PIPE_SWIZZLE_0;
            }
            break;
         case SWIZZLE_CLAMP_LUMINANCE_ALPHA:
            if (util_format_is_luminance_alpha(dst_format))
               break;
            for (unsigned i = 0; i < 4; i++) {
               if (invswizzle[i] != PIPE_SWIZZLE_X && invswizzle[i] != PIPE_SWIZZLE_W)
                  invswizzle[i] = PIPE_SWIZZLE_0;
            }
            break;
         case SWIZZLE_CLAMP_INTENSITY:
            for (unsigned i = 0; i < 4; i++) {
               if (invswizzle[i] == PIPE_SWIZZLE_W)
                  invswizzle[i] = PIPE_SWIZZLE_1;
               else if (invswizzle[i] != PIPE_SWIZZLE_X)
                  invswizzle[i] = PIPE_SWIZZLE_0;
            }
            break;
         case SWIZZLE_CLAMP_RGBX:
            for (unsigned i = 0; i < 4; i++) {
               if (invswizzle[i] == PIPE_SWIZZLE_W)
                  invswizzle[i] = PIPE_SWIZZLE_1;
            }
            break;
         default: break;
         }
         templ.swizzle_r = invswizzle[0];
         templ.swizzle_g = invswizzle[1];
         templ.swizzle_b = invswizzle[2];
         templ.swizzle_a = invswizzle[3];
      }
      templ.target = view_target;
      templ.u.tex.first_level = level;
      templ.u.tex.last_level = level;

      /* array textures expect to have array index provided */
      if (view_target != PIPE_TEXTURE_3D && src->array_size) {
         templ.u.tex.first_layer = layer;
         if (view_target == PIPE_TEXTURE_1D_ARRAY) {
            templ.u.tex.first_layer += yoffset;
            templ.u.tex.last_layer = templ.u.tex.first_layer + height - 1;
         } else {
            templ.u.tex.first_layer += zoffset;
            templ.u.tex.last_layer = templ.u.tex.first_layer + depth - 1;
         }
      }

      sampler_view = pipe->create_sampler_view(pipe, src, &templ);
      if (sampler_view == NULL)
         goto fail;

      pipe->set_sampler_views(pipe, PIPE_SHADER_COMPUTE, 0, 1, 0, false,
                              &sampler_view);
      st->state.num_sampler_views[PIPE_SHADER_COMPUTE] =
         MAX2(st->state.num_sampler_views[PIPE_SHADER_COMPUTE], 1);

      pipe_sampler_view_reference(&sampler_view, NULL);

      cso_set_samplers(cso, PIPE_SHADER_COMPUTE, 1, samplers);
   }

   /* Set up destination buffer */
   unsigned img_stride = _mesa_image_image_stride(pack, width, height, format, type);
   unsigned buffer_size = (depth + (dim == 3 ? pack->SkipImages : 0)) * img_stride;
   {
      dst = pipe_buffer_create(screen, PIPE_BIND_SHADER_BUFFER, PIPE_USAGE_STAGING, buffer_size);
      if (!dst)
         goto fail;

      struct pipe_shader_buffer buffer;
      memset(&buffer, 0, sizeof(buffer));
      buffer.buffer = dst;
      buffer.buffer_size = buffer_size;

      pipe->set_shader_buffers(pipe, PIPE_SHADER_COMPUTE, 0, 1, &buffer, 0x1);
   }

   struct pipe_grid_info info = { 0 };
   info.block[0] = src->target != PIPE_TEXTURE_1D ? 8 : 64;
   info.block[1] = src->target != PIPE_TEXTURE_1D ? 8 : 1;
   info.last_block[0] = width % info.block[0];
   info.last_block[1] = height % info.block[1];
   info.block[2] = 1;
   info.grid[0] = DIV_ROUND_UP(width, info.block[0]);
   info.grid[1] = DIV_ROUND_UP(height, info.block[1]);
   info.grid[2] = depth;

   pipe->launch_grid(pipe, &info);

fail:
   cso_restore_compute_state(cso);

   /* Unbind all because st/mesa won't do it if the current shader doesn't
    * use them.
    */
   pipe->set_sampler_views(pipe, PIPE_SHADER_COMPUTE, 0, 0, false,
                           st->state.num_sampler_views[PIPE_SHADER_COMPUTE],
                           NULL);
   st->state.num_sampler_views[PIPE_SHADER_COMPUTE] = 0;
   pipe->set_shader_buffers(pipe, PIPE_SHADER_COMPUTE, 0, 1, NULL, 0);

   st->dirty |= ST_NEW_CS_CONSTANTS |
                ST_NEW_CS_SSBOS |
                ST_NEW_CS_SAMPLER_VIEWS;



   return dst;
}

static void
copy_compute_buffer(struct gl_context * ctx,
                    struct gl_pixelstore_attrib *pack,
                    enum pipe_texture_target view_target,
                    struct pipe_resource *dst, enum pipe_format dst_format,
                    GLint xoffset, GLint yoffset, GLint zoffset,
                    GLsizei width, GLsizei height, GLint depth,
                    GLenum format, GLenum type, void *pixels)
{
   struct pipe_transfer *xfer;
   struct st_context *st = st_context(ctx);
   unsigned dim = get_dim_from_target(view_target);
   uint8_t *map = pipe_buffer_map(st->pipe, dst, PIPE_MAP_READ | PIPE_MAP_ONCE, &xfer);
   if (!map)
      return;

   pixels = _mesa_map_pbo_dest(ctx, pack, pixels);
   /* compute shader doesn't handle these to cut down on uniform size */
   if (pack->RowLength ||
       pack->SkipPixels ||
       pack->SkipRows ||
       pack->ImageHeight ||
       pack->SkipImages) {

      if (view_target == PIPE_TEXTURE_1D_ARRAY) {
         depth = height;
         height = 1;
         zoffset = yoffset;
         yoffset = 0;
      }
      struct gl_pixelstore_attrib packing = *pack;
      memset(&packing.RowLength, 0, offsetof(struct gl_pixelstore_attrib, SwapBytes) - offsetof(struct gl_pixelstore_attrib, RowLength));
      for (unsigned z = 0; z < depth; z++) {
         for (unsigned y = 0; y < height; y++) {
            GLubyte *dst = _mesa_image_address(dim, pack, pixels,
                                       width, height, format, type,
                                       z, y, 0);
            GLubyte *srcpx = _mesa_image_address(dim, &packing, map,
                                                 width, height, format, type,
                                                 z, y, 0);
            memcpy(dst, srcpx, util_format_get_stride(dst_format, width));
         }
      }
   } else {
      /* direct copy for all other cases */
      memcpy(pixels, map, dst->width0);
   }

   _mesa_unmap_pbo_dest(ctx, pack);
   pipe_buffer_unmap(st->pipe, xfer);
}

static void
st_GetTexSubImage_shader(struct gl_context * ctx,
                         GLint xoffset, GLint yoffset, GLint zoffset,
                         GLsizei width, GLsizei height, GLint depth,
                         GLenum format, GLenum type, void * pixels,
                         struct gl_texture_image *texImage)
{
   struct st_context *st = st_context(ctx);
   struct pipe_screen *screen = st->screen;
   struct st_texture_object *stObj = st_texture_object(texImage->TexObject);
   struct pipe_resource *src = stObj->pt;
   struct pipe_resource *dst = NULL;
   enum pipe_format dst_format, src_format;
   unsigned level = texImage->Level + texImage->TexObject->Attrib.MinLevel;
   unsigned layer = texImage->Face + texImage->TexObject->Attrib.MinLayer;
   enum pipe_texture_target view_target;

   assert(!_mesa_is_format_etc2(texImage->TexFormat) &&
          !_mesa_is_format_astc_2d(texImage->TexFormat) &&
          texImage->TexFormat != MESA_FORMAT_ETC1_RGB8);

   /* small cs copies probably incur too much overhead to be better than memcpy */
   if (width * height * depth < 64 * 64)
      goto fallback;

   /* See if the texture format already matches the format and type,
    * in which case the memcpy-based fast path will be used. */
   if (_mesa_format_matches_format_and_type(texImage->TexFormat, format,
                                            type, ctx->Pack.SwapBytes, NULL)) {
      goto fallback;
   }
   enum swizzle_clamp swizzle_clamp = 0;
   src_format = get_src_format(screen, stObj->surface_based ? stObj->surface_format : src->format, src);
   if (src_format == PIPE_FORMAT_NONE)
      goto fallback;

   if (texImage->_BaseFormat != _mesa_get_format_base_format(texImage->TexFormat)) {
      /* special handling for drivers that don't support these formats natively */
      if (texImage->_BaseFormat == GL_LUMINANCE)
         swizzle_clamp = SWIZZLE_CLAMP_LUMINANCE;
      else if (texImage->_BaseFormat == GL_LUMINANCE_ALPHA)
         swizzle_clamp = SWIZZLE_CLAMP_LUMINANCE_ALPHA;
      else if (texImage->_BaseFormat == GL_ALPHA)
         swizzle_clamp = SWIZZLE_CLAMP_ALPHA;
      else if (texImage->_BaseFormat == GL_INTENSITY)
         swizzle_clamp = SWIZZLE_CLAMP_INTENSITY;
      else if (texImage->_BaseFormat == GL_RGB)
         swizzle_clamp = SWIZZLE_CLAMP_RGBX;
   }

   dst_format = get_dst_format(ctx, PIPE_BUFFER, src_format, false, format, type, 0);

   if (dst_format == PIPE_FORMAT_NONE) {
      bool need_bgra_swizzle = false;
      dst_format = get_hack_format(ctx, src_format, format, type, &need_bgra_swizzle);
      if (dst_format == PIPE_FORMAT_NONE)
         goto fallback;
      /* special swizzling for component selection */
      if (need_bgra_swizzle)
         swizzle_clamp |= SWIZZLE_CLAMP_BGRA;
      else if (format == GL_GREEN_INTEGER)
         swizzle_clamp |= SWIZZLE_CLAMP_GREEN;
      else if (format == GL_BLUE_INTEGER)
         swizzle_clamp |= SWIZZLE_CLAMP_BLUE;
   }

   view_target = get_target_from_texture(src);
   /* I don't know why this works
    * only for the texture rects
    * but that's how it is
    */
   if ((src->target != PIPE_TEXTURE_RECT &&
       /* this would need multiple samplerviews */
       ((util_format_is_depth_and_stencil(src_format) && util_format_is_depth_and_stencil(dst_format)) ||
       /* these format just doesn't work and science can't explain why */
       dst_format == PIPE_FORMAT_Z32_FLOAT)) ||
       /* L8 -> L32_FLOAT is another thinker */
       (!util_format_is_float(src_format) && dst_format == PIPE_FORMAT_L32_FLOAT))
      goto fallback;

   dst = download_texture_compute(st, &ctx->Pack, xoffset, yoffset, zoffset, width, height, depth,
                                  level, layer, format, type, src_format, view_target, src, dst_format,
                                  swizzle_clamp);

   copy_compute_buffer(ctx, &ctx->Pack, view_target, dst, dst_format, xoffset, yoffset, zoffset,
                       width, height, depth, format, type, pixels);

   pipe_resource_reference(&dst, NULL);

   return;

fallback:

   _mesa_GetTexSubImage_sw(ctx, xoffset, yoffset, zoffset,
                           width, height, depth,
                           format, type, pixels, texImage);
}


/**
 * Called via ctx->Driver.GetTexSubImage()
 *
 * This uses a blit to copy the texture to a texture format which matches
 * the format and type combo and then a fast read-back is done using memcpy.
 * We can do arbitrary X/Y/Z/W/0/1 swizzling here as long as there is
 * a format which matches the swizzling.
 *
 * If such a format isn't available, it falls back to _mesa_GetTexImage_sw.
 *
 * NOTE: Drivers usually do a blit to convert between tiled and linear
 *       texture layouts during texture uploads/downloads, so the blit
 *       we do here should be free in such cases.
 */
static void
st_GetTexSubImage(struct gl_context * ctx,
                  GLint xoffset, GLint yoffset, GLint zoffset,
                  GLsizei width, GLsizei height, GLint depth,
                  GLenum format, GLenum type, void * pixels,
                  struct gl_texture_image *texImage)
{
   struct st_context *st = st_context(ctx);
   struct pipe_screen *screen = st->screen;
   struct st_texture_image *stImage = st_texture_image(texImage);
   struct st_texture_object *stObj = st_texture_object(texImage->TexObject);
   struct pipe_resource *src = stObj->pt;
   struct pipe_resource *dst = NULL;
   enum pipe_format dst_format, src_format;
   GLenum gl_target = texImage->TexObject->Target;
   enum pipe_texture_target pipe_target;
   struct pipe_blit_info blit;
   unsigned bind;
   boolean done = FALSE;

   assert(!_mesa_is_format_etc2(texImage->TexFormat) &&
          !_mesa_is_format_astc_2d(texImage->TexFormat) &&
          texImage->TexFormat != MESA_FORMAT_ETC1_RGB8);

   st_flush_bitmap_cache(st);
   if (getenv("MESA_COMPUTE_PBO"))
      goto fallback;

   /* GetTexImage only returns a single face for cubemaps. */
   if (gl_target == GL_TEXTURE_CUBE_MAP) {
      gl_target = GL_TEXTURE_2D;
   }
   pipe_target = gl_target_to_pipe(gl_target);

   if (!st->prefer_blit_based_texture_transfer &&
       !_mesa_is_format_compressed(texImage->TexFormat)) {
      /* Try to avoid the fallback if we're doing texture decompression here */
      goto fallback;
   }

   /* Handle non-finalized textures. */
   if (!stImage->pt || stImage->pt != stObj->pt || !src) {
      goto real_fallback;
   }

   /* XXX Fallback to _mesa_GetTexImage_sw for depth-stencil formats
    * due to an incomplete stencil blit implementation in some drivers. */
   if (format == GL_DEPTH_STENCIL || format == GL_STENCIL_INDEX) {
      goto fallback;
   }

   /* If the base internal format and the texture format don't match, we have
    * to fall back to _mesa_GetTexImage_sw. */
   if (texImage->_BaseFormat !=
       _mesa_get_format_base_format(texImage->TexFormat)) {
      goto fallback;
   }

   src_format = get_src_format(screen, stObj->surface_based ? stObj->surface_format : src->format, src);
   if (src_format == PIPE_FORMAT_NONE)
      goto fallback;

   if (format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL)
      bind = PIPE_BIND_DEPTH_STENCIL;
   else
      bind = PIPE_BIND_RENDER_TARGET;

   dst_format = get_dst_format(ctx, pipe_target, src_format, util_format_is_compressed(src->format),
                               format, type, bind);
   if (dst_format == PIPE_FORMAT_NONE)
      goto fallback;

   if (st->pbo.download_enabled && ctx->Pack.BufferObj) {
      if (try_pbo_download(st, texImage,
                           src_format, dst_format,
                           xoffset, yoffset, zoffset,
                           width, height, depth,
                           &ctx->Pack, pixels))
         return;
   }

   /* See if the texture format already matches the format and type,
    * in which case the memcpy-based fast path will be used. */
   if (_mesa_format_matches_format_and_type(texImage->TexFormat, format,
                                            type, ctx->Pack.SwapBytes, NULL))
      goto fallback;

   dst = create_dst_texture(ctx, dst_format, pipe_target, width, height, depth, gl_target, bind);
   if (!dst)
      goto fallback;

   /* From now on, we need the gallium representation of dimensions. */
   if (gl_target == GL_TEXTURE_1D_ARRAY) {
      zoffset = yoffset;
      yoffset = 0;
      depth = height;
      height = 1;
   }

   assert(texImage->Face == 0 ||
          texImage->TexObject->Attrib.MinLayer == 0 ||
          zoffset == 0);

   memset(&blit, 0, sizeof(blit));
   blit.src.resource = src;
   blit.src.level = texImage->Level + texImage->TexObject->Attrib.MinLevel;
   blit.src.format = src_format;
   blit.dst.resource = dst;
   blit.dst.level = 0;
   blit.dst.format = dst->format;
   blit.src.box.x = xoffset;
   blit.dst.box.x = 0;
   blit.src.box.y = yoffset;
   blit.dst.box.y = 0;
   blit.src.box.z = texImage->Face + texImage->TexObject->Attrib.MinLayer + zoffset;
   blit.dst.box.z = 0;
   blit.src.box.width = blit.dst.box.width = width;
   blit.src.box.height = blit.dst.box.height = height;
   blit.src.box.depth = blit.dst.box.depth = depth;
   blit.mask = st_get_blit_mask(texImage->_BaseFormat, format);
   blit.filter = PIPE_TEX_FILTER_NEAREST;
   blit.scissor_enable = FALSE;

   /* blit/render/decompress */
   st->pipe->blit(st->pipe, &blit);

   done = copy_to_staging_dest(ctx, dst, xoffset, yoffset, zoffset, width, height,
                           depth, format, type, pixels, texImage);
   pipe_resource_reference(&dst, NULL);

fallback:
   if (!done) {
      if (st->allow_compute_based_texture_transfer)
         st_GetTexSubImage_shader(ctx, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels, texImage);
      else
real_fallback:
         _mesa_GetTexSubImage_sw(ctx, xoffset, yoffset, zoffset,
                                 width, height, depth,
                                 format, type, pixels, texImage);
   }
}


/**
 * Do a CopyTexSubImage operation using a read transfer from the source,
 * a write transfer to the destination and get_tile()/put_tile() to access
 * the pixels/texels.
 *
 * Note: srcY=0=TOP of renderbuffer
 */
static void
fallback_copy_texsubimage(struct gl_context *ctx,
                          struct st_renderbuffer *strb,
                          struct st_texture_image *stImage,
                          GLenum baseFormat,
                          GLint destX, GLint destY, GLint slice,
                          GLint srcX, GLint srcY,
                          GLsizei width, GLsizei height)
{
   struct st_context *st = st_context(ctx);
   struct pipe_context *pipe = st->pipe;
   struct pipe_transfer *src_trans;
   GLubyte *texDest;
   enum pipe_map_flags transfer_usage;
   void *map;
   unsigned dst_width = width;
   unsigned dst_height = height;
   unsigned dst_depth = 1;
   struct pipe_transfer *transfer;

   if (ST_DEBUG & DEBUG_FALLBACK)
      debug_printf("%s: fallback processing\n", __func__);

   if (st_fb_orientation(ctx->ReadBuffer) == Y_0_TOP) {
      srcY = strb->Base.Height - srcY - height;
   }

   map = pipe_texture_map(pipe,
                           strb->texture,
                           strb->surface->u.tex.level,
                           strb->surface->u.tex.first_layer,
                           PIPE_MAP_READ,
                           srcX, srcY,
                           width, height, &src_trans);
   if (!map) {
      _mesa_error(ctx, GL_OUT_OF_MEMORY, "glCopyTexSubImage()");
      return;
   }

   if ((baseFormat == GL_DEPTH_COMPONENT ||
        baseFormat == GL_DEPTH_STENCIL) &&
       util_format_is_depth_and_stencil(stImage->pt->format))
      transfer_usage = PIPE_MAP_READ_WRITE;
   else
      transfer_usage = PIPE_MAP_WRITE;

   texDest = st_texture_image_map(st, stImage, transfer_usage,
                                  destX, destY, slice,
                                  dst_width, dst_height, dst_depth,
                                  &transfer);
   if (!texDest) {
      _mesa_error(ctx, GL_OUT_OF_MEMORY, "glCopyTexSubImage()");
      goto err;
   }

   if (baseFormat == GL_DEPTH_COMPONENT ||
       baseFormat == GL_DEPTH_STENCIL) {
      const GLboolean scaleOrBias = (ctx->Pixel.DepthScale != 1.0F ||
                                     ctx->Pixel.DepthBias != 0.0F);
      GLint row, yStep;
      uint *data;

      /* determine bottom-to-top vs. top-to-bottom order for src buffer */
      if (st_fb_orientation(ctx->ReadBuffer) == Y_0_TOP) {
         srcY = height - 1;
         yStep = -1;
      }
      else {
         srcY = 0;
         yStep = 1;
      }

      data = malloc(width * sizeof(uint));

      if (data) {
         unsigned dst_stride = (stImage->pt->target == PIPE_TEXTURE_1D_ARRAY ?
                                transfer->layer_stride : transfer->stride);
         /* To avoid a large temp memory allocation, do copy row by row */
         for (row = 0; row < height; row++, srcY += yStep) {
            util_format_unpack_z_32unorm(strb->texture->format,
                                         data, (uint8_t *)map + src_trans->stride * srcY,
                                         width);
            if (scaleOrBias) {
               _mesa_scale_and_bias_depth_uint(ctx, width, data);
            }

            util_format_pack_z_32unorm(stImage->pt->format,
                                       texDest + row * dst_stride, data, width);
         }
      }
      else {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glCopyTexSubImage()");
      }

      free(data);
   }
   else {
      /* RGBA format */
      GLfloat *tempSrc =
         malloc(width * height * 4 * sizeof(GLfloat));

      if (tempSrc) {
         const GLint dims = 2;
         GLint dstRowStride;
         struct gl_texture_image *texImage = &stImage->base;
         struct gl_pixelstore_attrib unpack = ctx->DefaultPacking;

         if (st_fb_orientation(ctx->ReadBuffer) == Y_0_TOP) {
            unpack.Invert = GL_TRUE;
         }

         if (stImage->pt->target == PIPE_TEXTURE_1D_ARRAY) {
            dstRowStride = transfer->layer_stride;
         }
         else {
            dstRowStride = transfer->stride;
         }

         /* get float/RGBA image from framebuffer */
         /* XXX this usually involves a lot of int/float conversion.
          * try to avoid that someday.
          */
         pipe_get_tile_rgba(src_trans, map, 0, 0, width, height,
                            util_format_linear(strb->texture->format),
                            tempSrc);

         /* Store into texture memory.
          * Note that this does some special things such as pixel transfer
          * ops and format conversion.  In particular, if the dest tex format
          * is actually RGBA but the user created the texture as GL_RGB we
          * need to fill-in/override the alpha channel with 1.0.
          */
         _mesa_texstore(ctx, dims,
                        texImage->_BaseFormat,
                        texImage->TexFormat,
                        dstRowStride,
                        &texDest,
                        width, height, 1,
                        GL_RGBA, GL_FLOAT, tempSrc, /* src */
                        &unpack);
      }
      else {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexSubImage");
      }

      free(tempSrc);
   }

   st_texture_image_unmap(st, stImage, slice);
err:
   pipe->texture_unmap(pipe, src_trans);
}


static bool
st_can_copyteximage_using_blit(const struct gl_texture_image *texImage,
                               const struct gl_renderbuffer *rb)
{
   GLenum tex_baseformat = _mesa_get_format_base_format(texImage->TexFormat);

   /* We don't blit to a teximage where the GL base format doesn't match the
    * texture's chosen format, except in the case of a GL_RGB texture
    * represented with GL_RGBA (where the alpha channel is just being
    * dropped).
    */
   if (texImage->_BaseFormat != tex_baseformat &&
       ((texImage->_BaseFormat != GL_RGB || tex_baseformat != GL_RGBA))) {
      return false;
   }

   /* We can't blit from a RB where the GL base format doesn't match the RB's
    * chosen format (for example, GL RGB or ALPHA with rb->Format of an RGBA
    * type, because the other channels will be undefined).
    */
   if (rb->_BaseFormat != _mesa_get_format_base_format(rb->Format))
      return false;

   return true;
}


/**
 * Do a CopyTex[Sub]Image1/2/3D() using a hardware (blit) path if possible.
 * Note that the region to copy has already been clipped so we know we
 * won't read from outside the source renderbuffer's bounds.
 *
 * Note: srcY=0=Bottom of renderbuffer (GL convention)
 */
static void
st_CopyTexSubImage(struct gl_context *ctx, GLuint dims,
                   struct gl_texture_image *texImage,
                   GLint destX, GLint destY, GLint slice,
                   struct gl_renderbuffer *rb,
                   GLint srcX, GLint srcY, GLsizei width, GLsizei height)
{
   struct st_texture_image *stImage = st_texture_image(texImage);
   struct st_texture_object *stObj = st_texture_object(texImage->TexObject);
   struct st_renderbuffer *strb = st_renderbuffer(rb);
   struct st_context *st = st_context(ctx);
   struct pipe_context *pipe = st->pipe;
   struct pipe_screen *screen = st->screen;
   struct pipe_blit_info blit;
   enum pipe_format dst_format;
   GLboolean do_flip = (st_fb_orientation(ctx->ReadBuffer) == Y_0_TOP);
   unsigned bind;
   GLint srcY0, srcY1;

   st_flush_bitmap_cache(st);
   st_invalidate_readpix_cache(st);

   assert(!_mesa_is_format_etc2(texImage->TexFormat) &&
          !_mesa_is_format_astc_2d(texImage->TexFormat) &&
          texImage->TexFormat != MESA_FORMAT_ETC1_RGB8);

   if (!strb || !strb->surface || !stImage->pt) {
      debug_printf("%s: null strb or stImage\n", __func__);
      return;
   }

   if (_mesa_texstore_needs_transfer_ops(ctx, texImage->_BaseFormat,
                                         texImage->TexFormat)) {
      goto fallback;
   }

   if (!st_can_copyteximage_using_blit(texImage, rb)) {
      goto fallback;
   }

   /* Choose the destination format to match the TexImage behavior. */
   dst_format = util_format_linear(stImage->pt->format);
   dst_format = util_format_luminance_to_red(dst_format);
   dst_format = util_format_intensity_to_red(dst_format);

   /* See if the destination format is supported. */
   if (texImage->_BaseFormat == GL_DEPTH_STENCIL ||
       texImage->_BaseFormat == GL_DEPTH_COMPONENT) {
      bind = PIPE_BIND_DEPTH_STENCIL;
   }
   else {
      bind = PIPE_BIND_RENDER_TARGET;
   }

   if (!dst_format ||
       !screen->is_format_supported(screen, dst_format, stImage->pt->target,
                                    stImage->pt->nr_samples,
                                    stImage->pt->nr_storage_samples, bind)) {
      goto fallback;
   }

   /* Y flipping for the main framebuffer. */
   if (do_flip) {
      srcY1 = strb->Base.Height - srcY - height;
      srcY0 = srcY1 + height;
   }
   else {
      srcY0 = srcY;
      srcY1 = srcY0 + height;
   }

   /* Blit the texture.
    * This supports flipping, format conversions, and downsampling.
    */
   memset(&blit, 0, sizeof(blit));
   blit.src.resource = strb->texture;
   blit.src.format = util_format_linear(strb->surface->format);
   blit.src.level = strb->surface->u.tex.level;
   blit.src.box.x = srcX;
   blit.src.box.y = srcY0;
   blit.src.box.z = strb->surface->u.tex.first_layer;
   blit.src.box.width = width;
   blit.src.box.height = srcY1 - srcY0;
   blit.src.box.depth = 1;
   blit.dst.resource = stImage->pt;
   blit.dst.format = dst_format;
   blit.dst.level = stObj->pt != stImage->pt
      ? 0 : texImage->Level + texImage->TexObject->Attrib.MinLevel;
   blit.dst.box.x = destX;
   blit.dst.box.y = destY;
   blit.dst.box.z = stImage->base.Face + slice +
                    texImage->TexObject->Attrib.MinLayer;
   blit.dst.box.width = width;
   blit.dst.box.height = height;
   blit.dst.box.depth = 1;
   blit.mask = st_get_blit_mask(rb->_BaseFormat, texImage->_BaseFormat);
   blit.filter = PIPE_TEX_FILTER_NEAREST;
   pipe->blit(pipe, &blit);
   return;

fallback:
   /* software fallback */
   fallback_copy_texsubimage(ctx,
                             strb, stImage, texImage->_BaseFormat,
                             destX, destY, slice,
                             srcX, srcY, width, height);
}


/**
 * Copy image data from stImage into the texture object 'stObj' at level
 * 'dstLevel'.
 */
static void
copy_image_data_to_texture(struct st_context *st,
                           struct st_texture_object *stObj,
                           GLuint dstLevel,
                           struct st_texture_image *stImage)
{
   /* debug checks */
   {
      ASSERTED const struct gl_texture_image *dstImage =
         stObj->base.Image[stImage->base.Face][dstLevel];
      assert(dstImage);
      assert(dstImage->Width == stImage->base.Width);
      assert(dstImage->Height == stImage->base.Height);
      assert(dstImage->Depth == stImage->base.Depth);
   }

   if (stImage->pt) {
      /* Copy potentially with the blitter:
       */
      GLuint src_level;
      if (stImage->pt->last_level == 0)
         src_level = 0;
      else
         src_level = stImage->base.Level;

      assert(src_level <= stImage->pt->last_level);
      assert(u_minify(stImage->pt->width0, src_level) == stImage->base.Width);
      assert(stImage->pt->target == PIPE_TEXTURE_1D_ARRAY ||
             u_minify(stImage->pt->height0, src_level) == stImage->base.Height);
      assert(stImage->pt->target == PIPE_TEXTURE_2D_ARRAY ||
             stImage->pt->target == PIPE_TEXTURE_CUBE_ARRAY ||
             u_minify(stImage->pt->depth0, src_level) == stImage->base.Depth);

      st_texture_image_copy(st->pipe,
                            stObj->pt, dstLevel,  /* dest texture, level */
                            stImage->pt, src_level, /* src texture, level */
                            stImage->base.Face);

      pipe_resource_reference(&stImage->pt, NULL);
   }
   pipe_resource_reference(&stImage->pt, stObj->pt);
}


/**
 * Called during state validation.  When this function is finished,
 * the texture object should be ready for rendering.
 * \return GL_TRUE for success, GL_FALSE for failure (out of mem)
 */
GLboolean
st_finalize_texture(struct gl_context *ctx,
                    struct pipe_context *pipe,
                    struct gl_texture_object *tObj,
                    GLuint cubeMapFace)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_object *stObj = st_texture_object(tObj);
   const GLuint nr_faces = _mesa_num_tex_faces(stObj->base.Target);
   GLuint face;
   const struct st_texture_image *firstImage;
   enum pipe_format firstImageFormat;
   unsigned ptWidth;
   uint16_t ptHeight, ptDepth, ptLayers, ptNumSamples;

   if (tObj->Immutable)
      return GL_TRUE;

   if (tObj->_MipmapComplete)
      stObj->lastLevel = stObj->base._MaxLevel;
   else if (tObj->_BaseComplete)
      stObj->lastLevel = stObj->base.Attrib.BaseLevel;

   /* Skip the loop over images in the common case of no images having
    * changed.  But if the GL_BASE_LEVEL or GL_MAX_LEVEL change to something we
    * haven't looked at, then we do need to look at those new images.
    */
   if (!stObj->needs_validation &&
       stObj->base.Attrib.BaseLevel >= stObj->validated_first_level &&
       stObj->lastLevel <= stObj->validated_last_level) {
      return GL_TRUE;
   }

   /* If this texture comes from a window system, there is nothing else to do. */
   if (stObj->surface_based) {
      return GL_TRUE;
   }

   firstImage = st_texture_image_const(stObj->base.Image[cubeMapFace]
                                       [stObj->base.Attrib.BaseLevel]);
   if (!firstImage)
      return false;

   /* If both firstImage and stObj point to a texture which can contain
    * all active images, favour firstImage.  Note that because of the
    * completeness requirement, we know that the image dimensions
    * will match.
    */
   if (firstImage->pt &&
       firstImage->pt != stObj->pt &&
       (!stObj->pt || firstImage->pt->last_level >= stObj->pt->last_level)) {
      pipe_resource_reference(&stObj->pt, firstImage->pt);
      st_texture_release_all_sampler_views(st, stObj);
   }

   /* Find gallium format for the Mesa texture */
   firstImageFormat =
      st_mesa_format_to_pipe_format(st, firstImage->base.TexFormat);

   /* Find size of level=0 Gallium mipmap image, plus number of texture layers */
   {
      unsigned width;
      uint16_t height, depth;

      st_gl_texture_dims_to_pipe_dims(stObj->base.Target,
                                      firstImage->base.Width2,
                                      firstImage->base.Height2,
                                      firstImage->base.Depth2,
                                      &width, &height, &depth, &ptLayers);

      /* If we previously allocated a pipe texture and its sizes are
       * compatible, use them.
       */
      if (stObj->pt &&
          u_minify(stObj->pt->width0, firstImage->base.Level) == width &&
          u_minify(stObj->pt->height0, firstImage->base.Level) == height &&
          u_minify(stObj->pt->depth0, firstImage->base.Level) == depth) {
         ptWidth = stObj->pt->width0;
         ptHeight = stObj->pt->height0;
         ptDepth = stObj->pt->depth0;
      } else {
         /* Otherwise, compute a new level=0 size that is compatible with the
          * base level image.
          */
         ptWidth = width > 1 ? width << firstImage->base.Level : 1;
         ptHeight = height > 1 ? height << firstImage->base.Level : 1;
         ptDepth = depth > 1 ? depth << firstImage->base.Level : 1;

         /* If the base level image is 1x1x1, we still need to ensure that the
          * resulting pipe texture ends up with the required number of levels
          * in total.
          */
         if (ptWidth == 1 && ptHeight == 1 && ptDepth == 1) {
            ptWidth <<= firstImage->base.Level;

            if (stObj->base.Target == GL_TEXTURE_CUBE_MAP ||
                stObj->base.Target == GL_TEXTURE_CUBE_MAP_ARRAY)
               ptHeight = ptWidth;
         }

         /* At this point, the texture may be incomplete (mismatched cube
          * face sizes, for example).  If that's the case, give up, but
          * don't return GL_FALSE as that would raise an incorrect
          * GL_OUT_OF_MEMORY error.  See Piglit fbo-incomplete-texture-03 test.
          */
         if (!stObj->base._BaseComplete) {
            _mesa_test_texobj_completeness(ctx, &stObj->base);
            if (!stObj->base._BaseComplete) {
               return TRUE;
            }
         }
      }

      ptNumSamples = firstImage->base.NumSamples;
   }

   /* If we already have a gallium texture, check that it matches the texture
    * object's format, target, size, num_levels, etc.
    */
   if (stObj->pt) {
      if (stObj->pt->target != gl_target_to_pipe(stObj->base.Target) ||
          stObj->pt->format != firstImageFormat ||
          stObj->pt->last_level < stObj->lastLevel ||
          stObj->pt->width0 != ptWidth ||
          stObj->pt->height0 != ptHeight ||
          stObj->pt->depth0 != ptDepth ||
          stObj->pt->nr_samples != ptNumSamples ||
          stObj->pt->array_size != ptLayers)
      {
         /* The gallium texture does not match the Mesa texture so delete the
          * gallium texture now.  We'll make a new one below.
          */
         pipe_resource_reference(&stObj->pt, NULL);
         st_texture_release_all_sampler_views(st, stObj);
         st->dirty |= ST_NEW_FRAMEBUFFER;
      }
   }

   /* May need to create a new gallium texture:
    */
   if (!stObj->pt) {
      GLuint bindings = default_bindings(st, firstImageFormat);

      stObj->pt = st_texture_create(st,
                                    gl_target_to_pipe(stObj->base.Target),
                                    firstImageFormat,
                                    stObj->lastLevel,
                                    ptWidth,
                                    ptHeight,
                                    ptDepth,
                                    ptLayers, ptNumSamples,
                                    bindings);

      if (!stObj->pt) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage");
         return GL_FALSE;
      }
   }

   /* Pull in any images not in the object's texture:
    */
   for (face = 0; face < nr_faces; face++) {
      GLuint level;
      for (level = stObj->base.Attrib.BaseLevel; level <= stObj->lastLevel; level++) {
         struct st_texture_image *stImage =
            st_texture_image(stObj->base.Image[face][level]);

         /* Need to import images in main memory or held in other textures.
          */
         if (stImage && stObj->pt != stImage->pt) {
            GLuint height;
            GLuint depth;

            if (stObj->base.Target != GL_TEXTURE_1D_ARRAY)
               height = u_minify(ptHeight, level);
            else
               height = ptLayers;

            if (stObj->base.Target == GL_TEXTURE_3D)
               depth = u_minify(ptDepth, level);
            else if (stObj->base.Target == GL_TEXTURE_CUBE_MAP)
               depth = 1;
            else
               depth = ptLayers;

            if (level == 0 ||
                (stImage->base.Width == u_minify(ptWidth, level) &&
                 stImage->base.Height == height &&
                 stImage->base.Depth == depth)) {
               /* src image fits expected dest mipmap level size */
               copy_image_data_to_texture(st, stObj, level, stImage);
            }
         }
      }
   }

   stObj->validated_first_level = stObj->base.Attrib.BaseLevel;
   stObj->validated_last_level = stObj->lastLevel;
   stObj->needs_validation = false;

   return GL_TRUE;
}


/**
 * Allocate a new pipe_resource object
 * width0, height0, depth0 are the dimensions of the level 0 image
 * (the highest resolution).  last_level indicates how many mipmap levels
 * to allocate storage for.  For non-mipmapped textures, this will be zero.
 */
static struct pipe_resource *
st_texture_create_from_memory(struct st_context *st,
                              struct st_memory_object *memObj,
                              GLuint64 offset,
                              enum pipe_texture_target target,
                              enum pipe_format format,
                              GLuint last_level,
                              GLuint width0,
                              GLuint height0,
                              GLuint depth0,
                              GLuint layers,
                              GLuint nr_samples,
                              GLuint bind)
{
   struct pipe_resource pt, *newtex;
   struct pipe_screen *screen = st->screen;

   assert(target < PIPE_MAX_TEXTURE_TYPES);
   assert(width0 > 0);
   assert(height0 > 0);
   assert(depth0 > 0);
   if (target == PIPE_TEXTURE_CUBE)
      assert(layers == 6);

   DBG("%s target %d format %s last_level %d\n", __func__,
       (int) target, util_format_name(format), last_level);

   assert(format);
   assert(screen->is_format_supported(screen, format, target, 0, 0,
                                      PIPE_BIND_SAMPLER_VIEW));

   memset(&pt, 0, sizeof(pt));
   pt.target = target;
   pt.format = format;
   pt.last_level = last_level;
   pt.width0 = width0;
   pt.height0 = height0;
   pt.depth0 = depth0;
   pt.array_size = layers;
   pt.usage = PIPE_USAGE_DEFAULT;
   pt.bind = bind;
   /* only set this for OpenGL textures, not renderbuffers */
   pt.flags = PIPE_RESOURCE_FLAG_TEXTURING_MORE_LIKELY;
   if (memObj->TextureTiling == GL_LINEAR_TILING_EXT)
      pt.bind |= PIPE_BIND_LINEAR;

   pt.nr_samples = nr_samples;
   pt.nr_storage_samples = nr_samples;

   newtex = screen->resource_from_memobj(screen, &pt, memObj->memory, offset);

   assert(!newtex || pipe_is_referenced(&newtex->reference));

   return newtex;
}


/**
 * Allocate texture memory for a whole mipmap stack.
 * Note: for multisample textures if the requested sample count is not
 * supported, we search for the next higher supported sample count.
 */
static GLboolean
st_texture_storage(struct gl_context *ctx,
                   struct gl_texture_object *texObj,
                   GLsizei levels, GLsizei width,
                   GLsizei height, GLsizei depth,
                   struct gl_memory_object *memObj,
                   GLuint64 offset)
{
   const GLuint numFaces = _mesa_num_tex_faces(texObj->Target);
   struct gl_texture_image *texImage = texObj->Image[0][0];
   struct st_context *st = st_context(ctx);
   struct st_texture_object *stObj = st_texture_object(texObj);
   struct st_memory_object *smObj = st_memory_object(memObj);
   struct pipe_screen *screen = st->screen;
   unsigned ptWidth, bindings;
   uint16_t ptHeight, ptDepth, ptLayers;
   enum pipe_format fmt;
   GLint level;
   GLuint num_samples = texImage->NumSamples;

   assert(levels > 0);

   stObj->lastLevel = levels - 1;

   fmt = st_mesa_format_to_pipe_format(st, texImage->TexFormat);

   bindings = default_bindings(st, fmt);

   if (smObj) {
      smObj->TextureTiling = texObj->TextureTiling;
      bindings |= PIPE_BIND_SHARED;
   }

   if (num_samples > 0) {
      /* Find msaa sample count which is actually supported.  For example,
       * if the user requests 1x but only 4x or 8x msaa is supported, we'll
       * choose 4x here.
       */
      enum pipe_texture_target ptarget = gl_target_to_pipe(texObj->Target);
      boolean found = FALSE;

      if (ctx->Const.MaxSamples > 1 && num_samples == 1) {
         /* don't try num_samples = 1 with drivers that support real msaa */
         num_samples = 2;
      }

      for (; num_samples <= ctx->Const.MaxSamples; num_samples++) {
         if (screen->is_format_supported(screen, fmt, ptarget,
                                         num_samples, num_samples,
                                         PIPE_BIND_SAMPLER_VIEW)) {
            /* Update the sample count in gl_texture_image as well. */
            texImage->NumSamples = num_samples;
            found = TRUE;
            break;
         }
      }

      if (!found) {
         return GL_FALSE;
      }
   }

   st_gl_texture_dims_to_pipe_dims(texObj->Target,
                                   width, height, depth,
                                   &ptWidth, &ptHeight, &ptDepth, &ptLayers);

   pipe_resource_reference(&stObj->pt, NULL);

   if (smObj) {
      stObj->pt = st_texture_create_from_memory(st,
                                                smObj,
                                                offset,
                                                gl_target_to_pipe(texObj->Target),
                                                fmt,
                                                levels - 1,
                                                ptWidth,
                                                ptHeight,
                                                ptDepth,
                                                ptLayers, num_samples,
                                                bindings);
   }
   else {
      stObj->pt = st_texture_create(st,
                                    gl_target_to_pipe(texObj->Target),
                                    fmt,
                                    levels - 1,
                                    ptWidth,
                                    ptHeight,
                                    ptDepth,
                                    ptLayers, num_samples,
                                    bindings);
   }

   if (!stObj->pt)
      return GL_FALSE;

   /* Set image resource pointers */
   for (level = 0; level < levels; level++) {
      GLuint face;
      for (face = 0; face < numFaces; face++) {
         struct st_texture_image *stImage =
            st_texture_image(texObj->Image[face][level]);
         pipe_resource_reference(&stImage->pt, stObj->pt);

         compressed_tex_fallback_allocate(st, stImage);
      }
   }

   /* The texture is in a validated state, so no need to check later. */
   stObj->needs_validation = false;
   stObj->validated_first_level = 0;
   stObj->validated_last_level = levels - 1;

   return GL_TRUE;
}

/**
 * Called via ctx->Driver.AllocTextureStorage() to allocate texture memory
 * for a whole mipmap stack.
 */
static GLboolean
st_AllocTextureStorage(struct gl_context *ctx,
                       struct gl_texture_object *texObj,
                       GLsizei levels, GLsizei width,
                       GLsizei height, GLsizei depth)
{
   return st_texture_storage(ctx, texObj, levels,
                             width, height, depth,
                             NULL, 0);
}


static GLboolean
st_TestProxyTexImage(struct gl_context *ctx, GLenum target,
                     GLuint numLevels, GLint level,
                     mesa_format format, GLuint numSamples,
                     GLint width, GLint height, GLint depth)
{
   struct st_context *st = st_context(ctx);

   if (width == 0 || height == 0 || depth == 0) {
      /* zero-sized images are legal, and always fit! */
      return GL_TRUE;
   }

   if (st->screen->can_create_resource) {
      /* Ask the gallium driver if the texture is too large */
      struct gl_texture_object *texObj =
         _mesa_get_current_tex_object(ctx, target);
      struct pipe_resource pt;

      /* Setup the pipe_resource object
       */
      memset(&pt, 0, sizeof(pt));

      pt.target = gl_target_to_pipe(target);
      pt.format = st_mesa_format_to_pipe_format(st, format);
      pt.nr_samples = numSamples;
      pt.nr_storage_samples = numSamples;

      st_gl_texture_dims_to_pipe_dims(target,
                                      width, height, depth,
                                      &pt.width0, &pt.height0,
                                      &pt.depth0, &pt.array_size);

      if (numLevels > 0) {
         /* For immutable textures we know the final number of mip levels */
         pt.last_level = numLevels - 1;
      }
      else if (level == 0 && (texObj->Sampler.Attrib.MinFilter == GL_LINEAR ||
                              texObj->Sampler.Attrib.MinFilter == GL_NEAREST)) {
         /* assume just one mipmap level */
         pt.last_level = 0;
      }
      else {
         /* assume a full set of mipmaps */
         pt.last_level = util_logbase2(MAX4(width, height, depth, 0));
      }

      return st->screen->can_create_resource(st->screen, &pt);
   }
   else {
      /* Use core Mesa fallback */
      return _mesa_test_proxy_teximage(ctx, target, numLevels, level, format,
                                       numSamples, width, height, depth);
   }
}

static GLboolean
st_TextureView(struct gl_context *ctx,
               struct gl_texture_object *texObj,
               struct gl_texture_object *origTexObj)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_object *orig = st_texture_object(origTexObj);
   struct st_texture_object *tex = st_texture_object(texObj);
   struct gl_texture_image *image = texObj->Image[0][0];

   const int numFaces = _mesa_num_tex_faces(texObj->Target);
   const int numLevels = texObj->Attrib.NumLevels;

   int face;
   int level;

   pipe_resource_reference(&tex->pt, orig->pt);

   /* Set image resource pointers */
   for (level = 0; level < numLevels; level++) {
      for (face = 0; face < numFaces; face++) {
         struct st_texture_image *stImage =
            st_texture_image(texObj->Image[face][level]);
         struct st_texture_image *origImage =
            st_texture_image(origTexObj->Image[face][level]);
         pipe_resource_reference(&stImage->pt, tex->pt);
         if (origImage &&
             origImage->compressed_data) {
            pipe_reference(NULL,
                           &origImage->compressed_data->reference);
            stImage->compressed_data = origImage->compressed_data;
         }
      }
   }

   tex->surface_based = GL_TRUE;
   tex->surface_format =
      st_mesa_format_to_pipe_format(st_context(ctx), image->TexFormat);

   tex->lastLevel = numLevels - 1;

   /* free texture sampler views.  They need to be recreated when we
    * change the texture view parameters.
    */
   st_texture_release_all_sampler_views(st, tex);

   /* The texture is in a validated state, so no need to check later. */
   tex->needs_validation = false;
   tex->validated_first_level = 0;
   tex->validated_last_level = numLevels - 1;

   return GL_TRUE;
}


/**
 * Find the mipmap level in 'pt' which matches the level described by
 * 'texImage'.
 */
static unsigned
find_mipmap_level(const struct gl_texture_image *texImage,
                  const struct pipe_resource *pt)
{
   const GLenum target = texImage->TexObject->Target;
   GLint texWidth = texImage->Width;
   GLint texHeight = texImage->Height;
   GLint texDepth = texImage->Depth;
   unsigned level, w;
   uint16_t h, d, layers;

   st_gl_texture_dims_to_pipe_dims(target, texWidth, texHeight, texDepth,
                                   &w, &h, &d, &layers);

   for (level = 0; level <= pt->last_level; level++) {
      if (u_minify(pt->width0, level) == w &&
          u_minify(pt->height0, level) == h &&
          u_minify(pt->depth0, level) == d) {
         return level;
      }
   }

   /* If we get here, there must be some sort of inconsistency between
    * the Mesa texture object/images and the gallium resource.
    */
   debug_printf("Inconsistent textures in find_mipmap_level()\n");

   return texImage->Level;
}


static void
st_ClearTexSubImage(struct gl_context *ctx,
                    struct gl_texture_image *texImage,
                    GLint xoffset, GLint yoffset, GLint zoffset,
                    GLsizei width, GLsizei height, GLsizei depth,
                    const void *clearValue)
{
   static const char zeros[16] = {0};
   struct gl_texture_object *texObj = texImage->TexObject;
   struct st_texture_image *stImage = st_texture_image(texImage);
   struct pipe_resource *pt = stImage->pt;
   struct st_context *st = st_context(ctx);
   struct pipe_context *pipe = st->pipe;
   unsigned level;
   struct pipe_box box;

   if (!pt)
      return;

   st_flush_bitmap_cache(st);
   st_invalidate_readpix_cache(st);

   u_box_3d(xoffset, yoffset, zoffset + texImage->Face,
            width, height, depth, &box);

   if (pt->target == PIPE_TEXTURE_1D_ARRAY) {
      box.z = box.y;
      box.depth = box.height;
      box.y = 0;
      box.height = 1;
   }

   if (texObj->Immutable) {
      /* The texture object has to be consistent (no "loose", per-image
       * gallium resources).  If this texture is a view into another
       * texture, we have to apply the MinLevel/Layer offsets.  If this is
       * not a texture view, the offsets will be zero.
       */
      assert(stImage->pt == st_texture_object(texObj)->pt);
      level = texImage->Level + texObj->Attrib.MinLevel;
      box.z += texObj->Attrib.MinLayer;
   }
   else {
      /* Texture level sizes may be inconsistent.  We my have "loose",
       * per-image gallium resources.  The texImage->Level may not match
       * the gallium resource texture level.
       */
      level = find_mipmap_level(texImage, pt);
   }

   assert(level <= pt->last_level);

   pipe->clear_texture(pipe, pt, level, &box, clearValue ? clearValue : zeros);
}


/**
 * Called via the glTexParam*() function, but only when some texture object
 * state has actually changed.
 */
static void
st_TexParameter(struct gl_context *ctx,
                struct gl_texture_object *texObj, GLenum pname)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_object *stObj = st_texture_object(texObj);

   switch (pname) {
   case GL_ALL_ATTRIB_BITS: /* meaning is all pnames, internal */
   case GL_TEXTURE_BASE_LEVEL:
   case GL_TEXTURE_MAX_LEVEL:
   case GL_DEPTH_TEXTURE_MODE:
   case GL_DEPTH_STENCIL_TEXTURE_MODE:
   case GL_TEXTURE_SRGB_DECODE_EXT:
   case GL_TEXTURE_SWIZZLE_R:
   case GL_TEXTURE_SWIZZLE_G:
   case GL_TEXTURE_SWIZZLE_B:
   case GL_TEXTURE_SWIZZLE_A:
   case GL_TEXTURE_SWIZZLE_RGBA:
   case GL_TEXTURE_BUFFER_SIZE:
   case GL_TEXTURE_BUFFER_OFFSET:
      /* changing any of these texture parameters means we must create
       * new sampler views.
       */
      st_texture_release_all_sampler_views(st, stObj);
      break;
   default:
      ; /* nothing */
   }
}

static GLboolean
st_SetTextureStorageForMemoryObject(struct gl_context *ctx,
                                    struct gl_texture_object *texObj,
                                    struct gl_memory_object *memObj,
                                    GLsizei levels, GLsizei width,
                                    GLsizei height, GLsizei depth,
                                    GLuint64 offset)
{
   return st_texture_storage(ctx, texObj, levels,
                             width, height, depth,
                             memObj, offset);
}

static GLuint64
st_NewTextureHandle(struct gl_context *ctx, struct gl_texture_object *texObj,
                    struct gl_sampler_object *sampObj)
{
   struct st_context *st = st_context(ctx);
   struct st_texture_object *stObj = st_texture_object(texObj);
   struct pipe_context *pipe = st->pipe;
   struct pipe_sampler_view *view;
   struct pipe_sampler_state sampler = {0};

   if (texObj->Target != GL_TEXTURE_BUFFER) {
      if (!st_finalize_texture(ctx, pipe, texObj, 0))
         return 0;

      st_convert_sampler(st, texObj, sampObj, 0, &sampler, false);

      /* TODO: Clarify the interaction of ARB_bindless_texture and EXT_texture_sRGB_decode */
      view = st_get_texture_sampler_view_from_stobj(st, stObj, sampObj, 0,
                                                    true, false);
   } else {
      view = st_get_buffer_sampler_view_from_stobj(st, stObj, false);
   }

   return pipe->create_texture_handle(pipe, view, &sampler);
}


static void
st_DeleteTextureHandle(struct gl_context *ctx, GLuint64 handle)
{
   struct st_context *st = st_context(ctx);
   struct pipe_context *pipe = st->pipe;

   pipe->delete_texture_handle(pipe, handle);
}


static void
st_MakeTextureHandleResident(struct gl_context *ctx, GLuint64 handle,
                             bool resident)
{
   struct st_context *st = st_context(ctx);
   struct pipe_context *pipe = st->pipe;

   pipe->make_texture_handle_resident(pipe, handle, resident);
}


static GLuint64
st_NewImageHandle(struct gl_context *ctx, struct gl_image_unit *imgObj)
{
   struct st_context *st = st_context(ctx);
   struct pipe_context *pipe = st->pipe;
   struct pipe_image_view image;

   st_convert_image(st, imgObj, &image, GL_READ_WRITE);

   return pipe->create_image_handle(pipe, &image);
}


static void
st_DeleteImageHandle(struct gl_context *ctx, GLuint64 handle)
{
   struct st_context *st = st_context(ctx);
   struct pipe_context *pipe = st->pipe;

   pipe->delete_image_handle(pipe, handle);
}


static void
st_MakeImageHandleResident(struct gl_context *ctx, GLuint64 handle,
                           GLenum access, bool resident)
{
   struct st_context *st = st_context(ctx);
   struct pipe_context *pipe = st->pipe;

   pipe->make_image_handle_resident(pipe, handle, access, resident);
}


void
st_init_texture_functions(struct dd_function_table *functions)
{
   functions->ChooseTextureFormat = st_ChooseTextureFormat;
   functions->QueryInternalFormat = st_QueryInternalFormat;
   functions->TexImage = st_TexImage;
   functions->TexSubImage = st_TexSubImage;
   functions->CompressedTexSubImage = st_CompressedTexSubImage;
   functions->CopyTexSubImage = st_CopyTexSubImage;
   functions->GenerateMipmap = st_generate_mipmap;

   functions->GetTexSubImage = st_GetTexSubImage;

   /* compressed texture functions */
   functions->CompressedTexImage = st_CompressedTexImage;

   functions->NewTextureObject = st_NewTextureObject;
   functions->NewTextureImage = st_NewTextureImage;
   functions->DeleteTextureImage = st_DeleteTextureImage;
   functions->DeleteTexture = st_DeleteTextureObject;
   functions->TextureRemovedFromShared = st_TextureReleaseAllSamplerViews;
   functions->AllocTextureImageBuffer = st_AllocTextureImageBuffer;
   functions->FreeTextureImageBuffer = st_FreeTextureImageBuffer;
   functions->MapTextureImage = st_MapTextureImage;
   functions->UnmapTextureImage = st_UnmapTextureImage;

   /* XXX Temporary until we can query pipe's texture sizes */
   functions->TestProxyTexImage = st_TestProxyTexImage;

   functions->AllocTextureStorage = st_AllocTextureStorage;
   functions->TextureView = st_TextureView;
   functions->ClearTexSubImage = st_ClearTexSubImage;

   functions->TexParameter = st_TexParameter;

   /* bindless functions */
   functions->NewTextureHandle = st_NewTextureHandle;
   functions->DeleteTextureHandle = st_DeleteTextureHandle;
   functions->MakeTextureHandleResident = st_MakeTextureHandleResident;
   functions->NewImageHandle = st_NewImageHandle;
   functions->DeleteImageHandle = st_DeleteImageHandle;
   functions->MakeImageHandleResident = st_MakeImageHandleResident;

   /* external object functions */
   functions->SetTextureStorageForMemoryObject = st_SetTextureStorageForMemoryObject;
}
