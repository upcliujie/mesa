/**
 * \file dd.h
 * Device driver interfaces.
 */

/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2006  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


#ifndef DD_INCLUDED
#define DD_INCLUDED

#include "glheader.h"
#include "formats.h"
#include "menums.h"
#include "compiler/shader_enums.h"

/* Windows winnt.h defines MemoryBarrier as a macro on some platforms,
 * including as a function-like macro in some cases. That either causes
 * the table entry below to have a weird name, or fail to compile.
 */
#ifdef MemoryBarrier
#undef MemoryBarrier
#endif

struct gl_bitmap_atlas;
struct gl_buffer_object;
struct gl_context;
struct gl_display_list;
struct gl_framebuffer;
struct gl_image_unit;
struct gl_pixelstore_attrib;
struct gl_program;
struct gl_renderbuffer;
struct gl_renderbuffer_attachment;
struct gl_shader;
struct gl_shader_program;
struct gl_texture_image;
struct gl_texture_object;
struct gl_memory_info;
struct gl_memory_object;
struct gl_query_object;
struct gl_sampler_object;
struct gl_transform_feedback_object;
struct gl_vertex_array_object;
struct ati_fragment_shader;
struct util_queue_monitoring;
struct _mesa_prim;
struct _mesa_index_buffer;
struct pipe_draw_info;
struct pipe_draw_start_count_bias;
struct pipe_vertex_state;
struct pipe_draw_vertex_state_info;
struct pipe_vertex_buffer;
struct pipe_vertex_element;

/* GL_ARB_vertex_buffer_object */
/* Modifies GL_MAP_UNSYNCHRONIZED_BIT to allow driver to fail (return
 * NULL) if buffer is unavailable for immediate mapping.
 *
 * Does GL_MAP_INVALIDATE_RANGE_BIT do this?  It seems so, but it
 * would require more book-keeping in the driver than seems necessary
 * at this point.
 *
 * Does GL_MAP_INVALDIATE_BUFFER_BIT do this?  Not really -- we don't
 * want to provoke the driver to throw away the old storage, we will
 * respect the contents of already referenced data.
 */
#define MESA_MAP_NOWAIT_BIT       0x4000

/* Mapping a buffer is allowed from any thread. */
#define MESA_MAP_THREAD_SAFE_BIT  0x8000

/* This buffer will only be mapped/unmapped once */
#define MESA_MAP_ONCE            0x10000

/* This BufferStorage flag indicates that the buffer will be used
 * by pipe_vertex_state, which doesn't track buffer busyness and doesn't
 * support invalidations.
 */
#define MESA_GALLIUM_VERTEX_STATE_STORAGE 0x20000


/**
 * Device driver function table.
 * Core Mesa uses these function pointers to call into device drivers.
 * Most of these functions directly correspond to OpenGL state commands.
 * Core Mesa will call these functions after error checking has been done
 * so that the drivers don't have to worry about error testing.
 *
 * Vertex transformation/clipping/lighting is patched into the T&L module.
 * Rasterization functions are patched into the swrast module.
 *
 * Note: when new functions are added here, the drivers/common/driverfuncs.c
 * file should be updated too!!!
 */
struct dd_function_table {
   /**
    * Notify the driver after Mesa has made some internal state changes.  
    *
    * This is in addition to any state change callbacks Mesa may already have
    * made.
    */
   void (*UpdateState)(struct gl_context *ctx);

   /**
    * This is called whenever glFinish() is called.
    */
   void (*Finish)( struct gl_context *ctx );

   /**
    * This is called whenever glFlush() is called.
    */
   void (*Flush)(struct gl_context *ctx, unsigned gallium_flush_flags);

   /**
    * Clear the color/depth/stencil/accum buffer(s).
    * \param buffers  a bitmask of BUFFER_BIT_* flags indicating which
    *                 renderbuffers need to be cleared.
    */
   void (*Clear)( struct gl_context *ctx, GLbitfield buffers );

   /**
    * Execute glRasterPos, updating the ctx->Current.Raster fields
    */
   void (*RasterPos)( struct gl_context *ctx, const GLfloat v[4] );

   /**
    * \name Image-related functions
    */
   /*@{*/

   /**
    * Called by glDrawPixels().
    * \p unpack describes how to unpack the source image data.
    */
   void (*DrawPixels)( struct gl_context *ctx,
		       GLint x, GLint y, GLsizei width, GLsizei height,
		       GLenum format, GLenum type,
		       const struct gl_pixelstore_attrib *unpack,
		       const GLvoid *pixels );

   /**
    * Called by glReadPixels().
    */
   void (*ReadPixels)( struct gl_context *ctx,
		       GLint x, GLint y, GLsizei width, GLsizei height,
		       GLenum format, GLenum type,
		       const struct gl_pixelstore_attrib *unpack,
		       GLvoid *dest );

   /**
    * Called by glCopyPixels().  
    */
   void (*CopyPixels)( struct gl_context *ctx, GLint srcx, GLint srcy,
                       GLsizei width, GLsizei height,
                       GLint dstx, GLint dsty, GLenum type );

   /**
    * Called by glBitmap().  
    */
   void (*Bitmap)( struct gl_context *ctx,
		   GLint x, GLint y, GLsizei width, GLsizei height,
		   const struct gl_pixelstore_attrib *unpack,
		   const GLubyte *bitmap );

   /**
    * Called by display list code for optimized glCallLists/glBitmap rendering
    * The driver must support texture rectangles of width 1024 or more.
    */
   void (*DrawAtlasBitmaps)(struct gl_context *ctx,
                            const struct gl_bitmap_atlas *atlas,
                            GLuint count, const GLubyte *ids);
   /*@}*/

   
   /**
    * \name Texture image functions
    */
   /*@{*/

   /**
    * Choose actual hardware texture format given the texture target, the
    * user-provided source image format and type and the desired internal
    * format.  In some cases, srcFormat and srcType can be GL_NONE.
    * Note:  target may be GL_TEXTURE_CUBE_MAP, but never
    * GL_TEXTURE_CUBE_MAP_[POSITIVE/NEGATIVE]_[XYZ].
    * Called by glTexImage(), etc.
    */
   mesa_format (*ChooseTextureFormat)(struct gl_context *ctx,
                                      GLenum target, GLint internalFormat,
                                      GLenum srcFormat, GLenum srcType );

   /**
    * Queries different driver parameters for a particular target and format.
    * Since ARB_internalformat_query2 introduced several new query parameters
    * over ARB_internalformat_query, having one driver hook for each parameter
    * is no longer feasible. So this is the generic entry-point for calls
    * to glGetInternalFormativ and glGetInternalFormati64v, after Mesa has
    * checked errors and default values.
    *
    * \param ctx            GL context
    * \param target         GL target enum
    * \param internalFormat GL format enum
    * \param pname          GL enum that specifies the info to query.
    * \param params         Buffer to hold the result of the query.
    */
   void (*QueryInternalFormat)(struct gl_context *ctx,
                               GLenum target,
                               GLenum internalFormat,
                               GLenum pname,
                               GLint *params);

   /**
    * Called by glTexImage[123]D() and glCopyTexImage[12]D()
    * Allocate texture memory and copy the user's image to the buffer.
    * The gl_texture_image fields, etc. will be fully initialized.
    * The parameters are the same as glTexImage3D(), plus:
    * \param dims  1, 2, or 3 indicating glTexImage1/2/3D()
    * \param packing describes how to unpack the source data.
    * \param texImage is the destination texture image.
    */
   void (*TexImage)(struct gl_context *ctx, GLuint dims,
                    struct gl_texture_image *texImage,
                    GLenum format, GLenum type, const GLvoid *pixels,
                    const struct gl_pixelstore_attrib *packing);

   /**
    * Called by glTexSubImage[123]D().
    * Replace a subset of the target texture with new texel data.
    */
   void (*TexSubImage)(struct gl_context *ctx, GLuint dims,
                       struct gl_texture_image *texImage,
                       GLint xoffset, GLint yoffset, GLint zoffset,
                       GLsizei width, GLsizei height, GLint depth,
                       GLenum format, GLenum type,
                       const GLvoid *pixels,
                       const struct gl_pixelstore_attrib *packing);


   /**
    * Called by glGetTexImage(), glGetTextureSubImage().
    */
   void (*GetTexSubImage)(struct gl_context *ctx,
                          GLint xoffset, GLint yoffset, GLint zoffset,
                          GLsizei width, GLsizei height, GLsizei depth,
                          GLenum format, GLenum type, GLvoid *pixels,
                          struct gl_texture_image *texImage);

   /**
    * Called by glClearTex[Sub]Image
    *
    * Clears a rectangular region of the image to a given value. The
    * clearValue argument is either NULL or points to a single texel to use as
    * the clear value in the same internal format as the texture image. If it
    * is NULL then the texture should be cleared to zeroes.
    */
   void (*ClearTexSubImage)(struct gl_context *ctx,
                            struct gl_texture_image *texImage,
                            GLint xoffset, GLint yoffset, GLint zoffset,
                            GLsizei width, GLsizei height, GLsizei depth,
                            const GLvoid *clearValue);

   /**
    * Called by glCopyTex[Sub]Image[123]D().
    *
    * This function should copy a rectangular region in the rb to a single
    * destination slice, specified by @slice.  In the case of 1D array
    * textures (where one GL call can potentially affect multiple destination
    * slices), core mesa takes care of calling this function multiple times,
    * once for each scanline to be copied.
    */
   void (*CopyTexSubImage)(struct gl_context *ctx, GLuint dims,
                           struct gl_texture_image *texImage,
                           GLint xoffset, GLint yoffset, GLint slice,
                           struct gl_renderbuffer *rb,
                           GLint x, GLint y,
                           GLsizei width, GLsizei height);
   /**
    * Called by glCopyImageSubData().
    *
    * This function should copy one 2-D slice from src_teximage or
    * src_renderbuffer to dst_teximage or dst_renderbuffer.  Either the
    * teximage or renderbuffer pointer will be non-null to indicate which
    * is the real src/dst.
    *
    * If one of the textures is 3-D or is a 1-D or 2-D array
    * texture, this function will be called multiple times: once for each
    * slice.  If one of the textures is a cube map, this function will be
    * called once for each face to be copied.
    */
   void (*CopyImageSubData)(struct gl_context *ctx,
                            struct gl_texture_image *src_teximage,
                            struct gl_renderbuffer *src_renderbuffer,
                            int src_x, int src_y, int src_z,
                            struct gl_texture_image *dst_teximage,
                            struct gl_renderbuffer *dst_renderbuffer,
                            int dst_x, int dst_y, int dst_z,
                            int src_width, int src_height);

   /**
    * Called by glGenerateMipmap() or when GL_GENERATE_MIPMAP_SGIS is enabled.
    * Note that if the texture is a cube map, the <target> parameter will
    * indicate which cube face to generate (GL_POSITIVE/NEGATIVE_X/Y/Z).
    * texObj->BaseLevel is the level from which to generate the remaining
    * mipmap levels.
    */
   void (*GenerateMipmap)(struct gl_context *ctx, GLenum target,
                          struct gl_texture_object *texObj);

   /**
    * Called by glTexImage, glCompressedTexImage, glCopyTexImage
    * and glTexStorage to check if the dimensions of the texture image
    * are too large.
    * \param target  any GL_PROXY_TEXTURE_x target
    * \return GL_TRUE if the image is OK, GL_FALSE if too large
    */
   GLboolean (*TestProxyTexImage)(struct gl_context *ctx, GLenum target,
                                  GLuint numLevels, GLint level,
                                  mesa_format format, GLuint numSamples,
                                  GLint width, GLint height,
                                  GLint depth);
   /*@}*/

   
   /**
    * \name Compressed texture functions
    */
   /*@{*/

   /**
    * Called by glCompressedTexImage[123]D().
    */
   void (*CompressedTexImage)(struct gl_context *ctx, GLuint dims,
                              struct gl_texture_image *texImage,
                              GLsizei imageSize, const GLvoid *data);

   /**
    * Called by glCompressedTexSubImage[123]D().
    */
   void (*CompressedTexSubImage)(struct gl_context *ctx, GLuint dims,
                                 struct gl_texture_image *texImage,
                                 GLint xoffset, GLint yoffset, GLint zoffset,
                                 GLsizei width, GLsizei height, GLsizei depth,
                                 GLenum format,
                                 GLsizei imageSize, const GLvoid *data);
   /*@}*/

   /**
    * \name Texture object / image functions
    */
   /*@{*/

   /**
    * Called to allocate a new texture object.  Drivers will usually
    * allocate/return a subclass of gl_texture_object.
    */
   struct gl_texture_object * (*NewTextureObject)(struct gl_context *ctx,
                                                  GLuint name, GLenum target);
   /**
    * Called to delete/free a texture object.  Drivers should free the
    * object and any image data it contains.
    */
   void (*DeleteTexture)(struct gl_context *ctx,
                         struct gl_texture_object *texObj);

   /**
    * Called to notify that texture is removed from ctx->Shared->TexObjects
    */
   void (*TextureRemovedFromShared)(struct gl_context *ctx,
                                   struct gl_texture_object *texObj);

   /** Called to allocate a new texture image object. */
   struct gl_texture_image * (*NewTextureImage)(struct gl_context *ctx);

   /** Called to free a texture image object returned by NewTextureImage() */
   void (*DeleteTextureImage)(struct gl_context *ctx,
                              struct gl_texture_image *);

   /** Called to allocate memory for a single texture image */
   GLboolean (*AllocTextureImageBuffer)(struct gl_context *ctx,
                                        struct gl_texture_image *texImage);

   /** Free the memory for a single texture image */
   void (*FreeTextureImageBuffer)(struct gl_context *ctx,
                                  struct gl_texture_image *texImage);

   /** Map a slice of a texture image into user space.
    * Note: for GL_TEXTURE_1D_ARRAY, height must be 1, y must be 0 and slice
    * indicates the 1D array index.
    * \param texImage  the texture image
    * \param slice  the 3D image slice or array texture slice
    * \param x, y, w, h  region of interest
    * \param mode  bitmask of GL_MAP_READ_BIT, GL_MAP_WRITE_BIT and
    *              GL_MAP_INVALIDATE_RANGE_BIT (if writing)
    * \param mapOut  returns start of mapping of region of interest
    * \param rowStrideOut returns row stride (in bytes).  In the case of a
    * compressed texture, this is the byte stride between one row of blocks
    * and another.
    */
   void (*MapTextureImage)(struct gl_context *ctx,
			   struct gl_texture_image *texImage,
			   GLuint slice,
			   GLuint x, GLuint y, GLuint w, GLuint h,
			   GLbitfield mode,
			   GLubyte **mapOut, GLint *rowStrideOut);

   void (*UnmapTextureImage)(struct gl_context *ctx,
			     struct gl_texture_image *texImage,
			     GLuint slice);

   /** For GL_ARB_texture_storage.  Allocate memory for whole mipmap stack.
    * All the gl_texture_images in the texture object will have their
    * dimensions, format, etc. initialized already.
    */
   GLboolean (*AllocTextureStorage)(struct gl_context *ctx,
                                    struct gl_texture_object *texObj,
                                    GLsizei levels, GLsizei width,
                                    GLsizei height, GLsizei depth);

   /** Called as part of glTextureView to add views to origTexObj */
   GLboolean (*TextureView)(struct gl_context *ctx,
                            struct gl_texture_object *texObj,
                            struct gl_texture_object *origTexObj);

   /**
    * Map a renderbuffer into user space.
    * \param mode  bitmask of GL_MAP_READ_BIT, GL_MAP_WRITE_BIT and
    *              GL_MAP_INVALIDATE_RANGE_BIT (if writing)
    */
   void (*MapRenderbuffer)(struct gl_context *ctx,
			   struct gl_renderbuffer *rb,
			   GLuint x, GLuint y, GLuint w, GLuint h,
			   GLbitfield mode,
			   GLubyte **mapOut, GLint *rowStrideOut,
			   bool flip_y);

   void (*UnmapRenderbuffer)(struct gl_context *ctx,
			     struct gl_renderbuffer *rb);

   /*@}*/


   /**
    * \name Vertex/fragment program functions
    */
   /*@{*/
   /** Allocate a new program */
   struct gl_program * (*NewProgram)(struct gl_context *ctx,
                                     gl_shader_stage stage,
                                     GLuint id, bool is_arb_asm);
   /** Delete a program */
   void (*DeleteProgram)(struct gl_context *ctx, struct gl_program *prog);   
   /**
    * Allocate a program to associate with the new ATI fragment shader (optional)
    */
   struct gl_program * (*NewATIfs)(struct gl_context *ctx,
                                   struct ati_fragment_shader *curProg);
   /**
    * Notify driver that a program string (and GPU code) has been specified
    * or modified.  Return GL_TRUE or GL_FALSE to indicate if the program is
    * supported by the driver.
    */
   GLboolean (*ProgramStringNotify)(struct gl_context *ctx, GLenum target, 
                                    struct gl_program *prog);

   /*@}*/

   /**
    * \name GLSL shader/program functions.
    */
   /*@{*/
   /**
    * Called when a shader program is linked.
    *
    * This gives drivers an opportunity to clone the IR and make their
    * own transformations on it for the purposes of code generation.
    */
   GLboolean (*LinkShader)(struct gl_context *ctx,
                           struct gl_shader_program *shader);
   /*@}*/


   /**
    * \name Draw functions.
    */
   /*@{*/
   /**
    * For indirect array drawing:
    *
    *    typedef struct {
    *       GLuint count;
    *       GLuint primCount;
    *       GLuint first;
    *       GLuint baseInstance; // in GL 4.2 and later, must be zero otherwise
    *    } DrawArraysIndirectCommand;
    *
    * For indirect indexed drawing:
    *
    *    typedef struct {
    *       GLuint count;
    *       GLuint primCount;
    *       GLuint firstIndex;
    *       GLint  baseVertex;
    *       GLuint baseInstance; // in GL 4.2 and later, must be zero otherwise
    *    } DrawElementsIndirectCommand;
    */

   /**
    * Draw a number of primitives.
    * \param prims  array [nr_prims] describing what to draw (prim type,
    *               vertex count, first index, instance count, etc).
    * \param ib  index buffer for indexed drawing, NULL for array drawing
    * \param index_bounds_valid  are min_index and max_index valid?
    * \param min_index  lowest vertex index used
    * \param max_index  highest vertex index used
    * \param num_instances  instance count from ARB_draw_instanced
    * \param base_instance  base instance from ARB_base_instance
    */
   void (*Draw)(struct gl_context *ctx,
                const struct _mesa_prim *prims, unsigned nr_prims,
                const struct _mesa_index_buffer *ib,
                bool index_bounds_valid,
                bool primitive_restart,
                unsigned restart_index,
                unsigned min_index, unsigned max_index,
                unsigned num_instances, unsigned base_instance);

   /**
    * Optimal Gallium version of Draw() that doesn't require translation
    * of draw info in the state tracker.
    *
    * The interface is identical to pipe_context::draw_vbo
    * with indirect == NULL.
    *
    * "info" is not const and the following fields can be changed by
    * the callee, so callers should be aware:
    * - info->index_bounds_valid (if false)
    * - info->min_index (if index_bounds_valid is false)
    * - info->max_index (if index_bounds_valid is false)
    * - info->drawid (if increment_draw_id is true)
    * - info->index.gl_bo (if index_size && !has_user_indices)
    */
   void (*DrawGallium)(struct gl_context *ctx,
                       struct pipe_draw_info *info,
                       unsigned drawid_offset,
                       const struct pipe_draw_start_count_bias *draws,
                       unsigned num_draws);

   /**
    * Same as DrawGallium, but mode can also change between draws.
    *
    * "info" is not const and the following fields can be changed by
    * the callee in addition to the fields listed by DrawGallium:
    * - info->mode
    *
    * This function exists to decrease complexity of DrawGallium.
    */
   void (*DrawGalliumMultiMode)(struct gl_context *ctx,
                                struct pipe_draw_info *info,
                                const struct pipe_draw_start_count_bias *draws,
                                const unsigned char *mode,
                                unsigned num_draws);

   /**
    * Draw a primitive, getting the vertex count, instance count, start
    * vertex, etc. from a buffer object.
    * \param mode  GL_POINTS, GL_LINES, GL_TRIANGLE_STRIP, etc.
    * \param indirect_data  buffer to get "DrawArrays/ElementsIndirectCommand"
    *                       data
    * \param indirect_offset  offset of first primitive in indrect_data buffer
    * \param draw_count  number of primitives to draw
    * \param stride  stride, in bytes, between
    *                "DrawArrays/ElementsIndirectCommand" objects
    * \param indirect_draw_count_buffer  if non-NULL specifies a buffer to get
    *                                    the real draw_count value.  Used for
    *                                    GL_ARB_indirect_parameters.
    * \param indirect_draw_count_offset  offset to the draw_count value in
    *                                    indirect_draw_count_buffer
    * \param ib  index buffer for indexed drawing, NULL otherwise.
    */
   void (*DrawIndirect)(struct gl_context *ctx, GLuint mode,
                        struct gl_buffer_object *indirect_data,
                        GLsizeiptr indirect_offset, unsigned draw_count,
                        unsigned stride,
                        struct gl_buffer_object *indirect_draw_count_buffer,
                        GLsizeiptr indirect_draw_count_offset,
                        const struct _mesa_index_buffer *ib,
                        bool primitive_restart,
                        unsigned restart_index);

   /**
    * Driver implementation of glDrawTransformFeedback.
    *
    * \param mode    Primitive type
    * \param num_instances  instance count from ARB_draw_instanced
    * \param stream  If called via DrawTransformFeedbackStream, specifies
    *                the vertex stream buffer from which to get the vertex
    *                count.
    * \param tfb_vertcount  if non-null, indicates which transform feedback
    *                       object has the vertex count.
    */
   void (*DrawTransformFeedback)(struct gl_context *ctx, GLenum mode,
                                 unsigned num_instances, unsigned stream,
                                 struct gl_transform_feedback_object *tfb_vertcount);

   void (*DrawGalliumVertexState)(struct gl_context *ctx,
                                  struct pipe_vertex_state *state,
                                  struct pipe_draw_vertex_state_info info,
                                  const struct pipe_draw_start_count_bias *draws,
                                  const uint8_t *mode,
                                  unsigned num_draws,
                                  bool per_vertex_edgeflags);
   /*@}*/

   struct pipe_vertex_state *
      (*CreateGalliumVertexState)(struct gl_context *ctx,
                                  const struct gl_vertex_array_object *vao,
                                  struct gl_buffer_object *indexbuf,
                                  uint32_t enabled_attribs);

   /**
    * \name State-changing functions.
    *
    * \note drawing functions are above.
    *
    * These functions are called by their corresponding OpenGL API functions.
    * They are \e also called by the gl_PopAttrib() function!!!
    * May add more functions like these to the device driver in the future.
    */
   /*@{*/
   /** Used to allocated any buffers with on-demand creation */
   void (*DrawBufferAllocate)(struct gl_context *ctx);
   /** Enable or disable server-side gl capabilities */
   void (*Enable)(struct gl_context *ctx, GLenum cap, GLboolean state);
   /* Specifies the current buffer for reading */
   void (*ReadBuffer)( struct gl_context *ctx, GLenum buffer );
   /** Set rasterization mode */
   void (*RenderMode)(struct gl_context *ctx, GLenum mode );
   /** Set texture parameter (callee gets param value from the texObj) */
   void (*TexParameter)(struct gl_context *ctx,
                        struct gl_texture_object *texObj, GLenum pname);
   /** Set the viewport */
   void (*Viewport)(struct gl_context *ctx);
   /*@}*/


   /**
    * \name Vertex/pixel buffer object functions
    */
   /*@{*/
   struct gl_buffer_object * (*NewBufferObject)(struct gl_context *ctx,
                                                GLuint buffer);
   
   void (*DeleteBuffer)( struct gl_context *ctx, struct gl_buffer_object *obj );

   GLboolean (*BufferData)(struct gl_context *ctx, GLenum target,
                           GLsizeiptrARB size, const GLvoid *data, GLenum usage,
                           GLenum storageFlags, struct gl_buffer_object *obj);

   void (*BufferSubData)( struct gl_context *ctx, GLintptrARB offset,
			  GLsizeiptrARB size, const GLvoid *data,
			  struct gl_buffer_object *obj );

   void (*GetBufferSubData)( struct gl_context *ctx,
			     GLintptrARB offset, GLsizeiptrARB size,
			     GLvoid *data, struct gl_buffer_object *obj );

   void (*ClearBufferSubData)( struct gl_context *ctx,
                               GLintptr offset, GLsizeiptr size,
                               const GLvoid *clearValue,
                               GLsizeiptr clearValueSize,
                               struct gl_buffer_object *obj );

   void (*CopyBufferSubData)( struct gl_context *ctx,
                              struct gl_buffer_object *src,
                              struct gl_buffer_object *dst,
                              GLintptr readOffset, GLintptr writeOffset,
                              GLsizeiptr size );

   void (*InvalidateBufferSubData)( struct gl_context *ctx,
                                    struct gl_buffer_object *obj,
                                    GLintptr offset,
                                    GLsizeiptr length );

   /* Returns pointer to the start of the mapped range.
    * May return NULL if MESA_MAP_NOWAIT_BIT is set in access:
    */
   void * (*MapBufferRange)( struct gl_context *ctx, GLintptr offset,
                             GLsizeiptr length, GLbitfield access,
                             struct gl_buffer_object *obj,
                             gl_map_buffer_index index);

   void (*FlushMappedBufferRange)(struct gl_context *ctx,
                                  GLintptr offset, GLsizeiptr length,
                                  struct gl_buffer_object *obj,
                                  gl_map_buffer_index index);

   GLboolean (*UnmapBuffer)( struct gl_context *ctx,
			     struct gl_buffer_object *obj,
                             gl_map_buffer_index index);
   /*@}*/

   /**
    * \name Functions for GL_APPLE_object_purgeable
    */
   /*@{*/
   /* variations on ObjectPurgeable */
   GLenum (*BufferObjectPurgeable)(struct gl_context *ctx,
                                   struct gl_buffer_object *obj, GLenum option);
   GLenum (*RenderObjectPurgeable)(struct gl_context *ctx,
                                   struct gl_renderbuffer *obj, GLenum option);
   GLenum (*TextureObjectPurgeable)(struct gl_context *ctx,
                                    struct gl_texture_object *obj,
                                    GLenum option);

   /* variations on ObjectUnpurgeable */
   GLenum (*BufferObjectUnpurgeable)(struct gl_context *ctx,
                                     struct gl_buffer_object *obj,
                                     GLenum option);
   GLenum (*RenderObjectUnpurgeable)(struct gl_context *ctx,
                                     struct gl_renderbuffer *obj,
                                     GLenum option);
   GLenum (*TextureObjectUnpurgeable)(struct gl_context *ctx,
                                      struct gl_texture_object *obj,
                                      GLenum option);
   /*@}*/

   /**
    * \name Functions for GL_EXT_framebuffer_{object,blit,discard}.
    */
   /*@{*/
   struct gl_renderbuffer * (*NewRenderbuffer)(struct gl_context *ctx,
                                               GLuint name);
   void (*RenderTexture)(struct gl_context *ctx,
                         struct gl_framebuffer *fb,
                         struct gl_renderbuffer_attachment *att);
   void (*FinishRenderTexture)(struct gl_context *ctx,
                               struct gl_renderbuffer *rb);
   void (*ValidateFramebuffer)(struct gl_context *ctx,
                               struct gl_framebuffer *fb);
   /*@}*/
   void (*BlitFramebuffer)(struct gl_context *ctx,
                           struct gl_framebuffer *readFb,
                           struct gl_framebuffer *drawFb,
                           GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                           GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                           GLbitfield mask, GLenum filter);
   void (*DiscardFramebuffer)(struct gl_context *ctx, struct gl_framebuffer *fb,
                              struct gl_renderbuffer_attachment *att);

   /**
    * \name Functions for GL_ARB_sample_locations
    */
   void (*GetProgrammableSampleCaps)(struct gl_context *ctx,
                                     const struct gl_framebuffer *fb,
                                     GLuint *bits, GLuint *width, GLuint *height);
   void (*EvaluateDepthValues)(struct gl_context *ctx);

   /*@}*/

   /**
    * \name Performance monitors
    */
   /*@{*/
   void (*InitPerfMonitorGroups)(struct gl_context *ctx);
   struct gl_perf_monitor_object * (*NewPerfMonitor)(struct gl_context *ctx);
   void (*DeletePerfMonitor)(struct gl_context *ctx,
                             struct gl_perf_monitor_object *m);
   GLboolean (*BeginPerfMonitor)(struct gl_context *ctx,
                                 struct gl_perf_monitor_object *m);

   /** Stop an active performance monitor, discarding results. */
   void (*ResetPerfMonitor)(struct gl_context *ctx,
                            struct gl_perf_monitor_object *m);
   void (*EndPerfMonitor)(struct gl_context *ctx,
                          struct gl_perf_monitor_object *m);
   GLboolean (*IsPerfMonitorResultAvailable)(struct gl_context *ctx,
                                             struct gl_perf_monitor_object *m);
   void (*GetPerfMonitorResult)(struct gl_context *ctx,
                                struct gl_perf_monitor_object *m,
                                GLsizei dataSize,
                                GLuint *data,
                                GLint *bytesWritten);
   /*@}*/

   /**
    * \name Performance Query objects
    */
   /*@{*/
   unsigned (*InitPerfQueryInfo)(struct gl_context *ctx);
   void (*GetPerfQueryInfo)(struct gl_context *ctx,
                            unsigned queryIndex,
                            const char **name,
                            GLuint *dataSize,
                            GLuint *numCounters,
                            GLuint *numActive);
   void (*GetPerfCounterInfo)(struct gl_context *ctx,
                              unsigned queryIndex,
                              unsigned counterIndex,
                              const char **name,
                              const char **desc,
                              GLuint *offset,
                              GLuint *data_size,
                              GLuint *type_enum,
                              GLuint *data_type_enum,
                              GLuint64 *raw_max);
   struct gl_perf_query_object * (*NewPerfQueryObject)(struct gl_context *ctx,
                                                       unsigned queryIndex);
   void (*DeletePerfQuery)(struct gl_context *ctx,
                           struct gl_perf_query_object *obj);
   bool (*BeginPerfQuery)(struct gl_context *ctx,
                          struct gl_perf_query_object *obj);
   void (*EndPerfQuery)(struct gl_context *ctx,
                        struct gl_perf_query_object *obj);
   void (*WaitPerfQuery)(struct gl_context *ctx,
                         struct gl_perf_query_object *obj);
   bool (*IsPerfQueryReady)(struct gl_context *ctx,
                            struct gl_perf_query_object *obj);
   bool (*GetPerfQueryData)(struct gl_context *ctx,
                            struct gl_perf_query_object *obj,
                            GLsizei dataSize,
                            GLuint *data,
                            GLuint *bytesWritten);
   /*@}*/


   /**
    * \name GREMEDY debug/marker functions
    */
   /*@{*/
   void (*EmitStringMarker)(struct gl_context *ctx, const GLchar *string, GLsizei len);
   /*@}*/

   /**
    * \name Support for multiple T&L engines
    */
   /*@{*/

   /**
    * Set by the driver-supplied T&L engine.  
    *
    * Set to PRIM_OUTSIDE_BEGIN_END when outside glBegin()/glEnd().
    */
   GLuint CurrentExecPrimitive;

   /**
    * Current glBegin state of an in-progress compilation.  May be
    * GL_POINTS, GL_TRIANGLE_STRIP, etc. or PRIM_OUTSIDE_BEGIN_END
    * or PRIM_UNKNOWN.
    */
   GLuint CurrentSavePrimitive;


#define FLUSH_STORED_VERTICES 0x1
#define FLUSH_UPDATE_CURRENT  0x2
   /**
    * Set by the driver-supplied T&L engine whenever vertices are buffered
    * between glBegin()/glEnd() objects or __struct gl_contextRec::Current
    * is not updated.  A bitmask of the FLUSH_x values above.
    *
    * The dd_function_table::FlushVertices call below may be used to resolve
    * these conditions.
    */
   GLbitfield NeedFlush;

   /** Need to call vbo_save_SaveFlushVertices() upon state change? */
   GLboolean SaveNeedFlush;

   /**@}*/

   /**
    * \name GL_OES_draw_texture interface
    */
   /*@{*/
   void (*DrawTex)(struct gl_context *ctx, GLfloat x, GLfloat y, GLfloat z,
                   GLfloat width, GLfloat height);
   /*@}*/

   /**
    * \name GL_OES_EGL_image interface
    */
   void (*EGLImageTargetTexture2D)(struct gl_context *ctx, GLenum target,
				   struct gl_texture_object *texObj,
				   struct gl_texture_image *texImage,
				   GLeglImageOES image_handle);
   void (*EGLImageTargetRenderbufferStorage)(struct gl_context *ctx,
					     struct gl_renderbuffer *rb,
					     void *image_handle);

   /**
    * \name GL_EXT_EGL_image_storage interface
    */
   void (*EGLImageTargetTexStorage)(struct gl_context *ctx, GLenum target,
                                    struct gl_texture_object *texObj,
                                    struct gl_texture_image *texImage,
                                    GLeglImageOES image_handle);

   /**
    * \name GL_ARB_texture_multisample
    */
   void (*GetSamplePosition)(struct gl_context *ctx,
                             struct gl_framebuffer *fb,
                             GLuint index,
                             GLfloat *outValue);

   /**
    * \name NV_vdpau_interop interface
    */
   void (*VDPAUMapSurface)(struct gl_context *ctx, GLenum target,
                           GLenum access, GLboolean output,
                           struct gl_texture_object *texObj,
                           struct gl_texture_image *texImage,
                           const GLvoid *vdpSurface, GLuint index);
   void (*VDPAUUnmapSurface)(struct gl_context *ctx, GLenum target,
                             GLenum access, GLboolean output,
                             struct gl_texture_object *texObj,
                             struct gl_texture_image *texImage,
                             const GLvoid *vdpSurface, GLuint index);

   /**
    * Query reset status for GL_ARB_robustness
    *
    * Per \c glGetGraphicsResetStatusARB, this function should return a
    * non-zero value once after a reset.  If a reset is non-atomic, the
    * non-zero status should be returned for the duration of the reset.
    */
   GLenum (*GetGraphicsResetStatus)(struct gl_context *ctx);

   /**
    * \name GL_ARB_compute_shader interface
    */
   /*@{*/
   void (*DispatchCompute)(struct gl_context *ctx, const GLuint *num_groups);
   void (*DispatchComputeIndirect)(struct gl_context *ctx, GLintptr indirect);
   /*@}*/

   /**
    * \name GL_ARB_compute_variable_group_size interface
    */
   /*@{*/
   void (*DispatchComputeGroupSize)(struct gl_context *ctx,
                                    const GLuint *num_groups,
                                    const GLuint *group_size);
   /*@}*/

   /**
    * Query information about memory. Device memory is e.g. VRAM. Staging
    * memory is e.g. GART. All sizes are in kilobytes.
    */
   void (*QueryMemoryInfo)(struct gl_context *ctx,
                           struct gl_memory_info *info);

   /**
    * Indicate that this thread is being used by Mesa as a background drawing
    * thread for the given GL context.
    *
    * If this function is called more than once from any given thread, each
    * subsequent call overrides the context that was passed in the previous
    * call.  Mesa takes advantage of this to re-use a background thread to
    * perform drawing on behalf of multiple contexts.
    *
    * Mesa may sometimes call this function from a non-background thread
    * (i.e. a thread that has already been bound to a context using
    * __DriverAPIRec::MakeCurrent()); when this happens, ctx will be equal to
    * the context that is bound to this thread.
    *
    * Mesa will only call this function if GL multithreading is enabled.
    */
   void (*SetBackgroundContext)(struct gl_context *ctx,
                                struct util_queue_monitoring *queue_info);

   /**
    * \name GL_ARB_sparse_buffer interface
    */
   /*@{*/
   void (*BufferPageCommitment)(struct gl_context *ctx,
                                struct gl_buffer_object *bufferObj,
                                GLintptr offset, GLsizeiptr size,
                                GLboolean commit);
   /*@}*/

   /**
    * \name GL_ARB_bindless_texture interface
    */
   /*@{*/
   GLuint64 (*NewTextureHandle)(struct gl_context *ctx,
                                struct gl_texture_object *texObj,
                                struct gl_sampler_object *sampObj);
   void (*DeleteTextureHandle)(struct gl_context *ctx, GLuint64 handle);
   void (*MakeTextureHandleResident)(struct gl_context *ctx, GLuint64 handle,
                                     bool resident);
   GLuint64 (*NewImageHandle)(struct gl_context *ctx,
                              struct gl_image_unit *imgObj);
   void (*DeleteImageHandle)(struct gl_context *ctx, GLuint64 handle);
   void (*MakeImageHandleResident)(struct gl_context *ctx, GLuint64 handle,
                                   GLenum access, bool resident);
   /*@}*/


   /**
    * Set the given memory object as the texture's storage.
    */
   GLboolean (*SetTextureStorageForMemoryObject)(struct gl_context *ctx,
                                                 struct gl_texture_object *tex_obj,
                                                 struct gl_memory_object *mem_obj,
                                                 GLsizei levels, GLsizei width,
                                                 GLsizei height, GLsizei depth,
                                                 GLuint64 offset);

   /**
    * Use a memory object as the backing data for a buffer object
    */
   GLboolean (*BufferDataMem)(struct gl_context *ctx,
                              GLenum target,
                              GLsizeiptrARB size,
                              struct gl_memory_object *memObj,
                              GLuint64 offset,
                              GLenum usage,
                              struct gl_buffer_object *bufObj);

   /**
    * Fill uuid with an unique identifier for this driver
    *
    * uuid must point to GL_UUID_SIZE_EXT bytes of available memory
    */
   void (*GetDriverUuid)(struct gl_context *ctx, char *uuid);

   /**
    * Fill uuid with an unique identifier for the device associated
    * to this driver
    *
    * uuid must point to GL_UUID_SIZE_EXT bytes of available memory
    */
   void (*GetDeviceUuid)(struct gl_context *ctx, char *uuid);

   /*@}*/

   /**
    * \name GL_ARB_get_program_binary
    */
   /*@{*/
   /**
    * Calls to retrieve/store a binary serialized copy of the current program.
    */
   void (*GetProgramBinaryDriverSHA1)(struct gl_context *ctx, uint8_t *sha1);

   void (*ProgramBinarySerializeDriverBlob)(struct gl_context *ctx,
                                            struct gl_shader_program *shProg,
                                            struct gl_program *prog);

   void (*ProgramBinaryDeserializeDriverBlob)(struct gl_context *ctx,
                                              struct gl_shader_program *shProg,
                                              struct gl_program *prog);
   /*@}*/

   /**
    * \name Disk shader cache functions
    */
   /*@{*/
   /**
    * Called to initialize gl_program::driver_cache_blob (and size) with a
    * ralloc allocated buffer.
    *
    * This buffer will be saved and restored as part of the gl_program
    * serialization and deserialization.
    */
   void (*ShaderCacheSerializeDriverBlob)(struct gl_context *ctx,
                                          struct gl_program *prog);
   /*@}*/

   /**
    * \name Set the number of compiler threads for ARB_parallel_shader_compile
    */
   void (*SetMaxShaderCompilerThreads)(struct gl_context *ctx, unsigned count);
   bool (*GetShaderProgramCompletionStatus)(struct gl_context *ctx,
                                            struct gl_shader_program *shprog);

   void (*PinDriverToL3Cache)(struct gl_context *ctx, unsigned L3_cache);

   GLboolean (*ValidateEGLImage)(struct gl_context *ctx, GLeglImageOES image_handle);
};

#endif /* DD_INCLUDED */
