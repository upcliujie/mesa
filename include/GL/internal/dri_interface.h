/*
 * Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2007-2008 Red Hat, Inc.
 * (C) Copyright IBM Corporation 2004
 * All Rights Reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * \file dri_interface.h
 *
 * This file contains all the types and functions that define the interface
 * between a DRI driver and driver loader.  Currently, the most common driver
 * loader is the XFree86 libGL.so.  However, other loaders do exist, and in
 * the future the server-side libglx.a will also be a loader.
 *
 * This interface is frozen, and somewhat deprecated. All that remains in this
 * header is enough to make modern versions of xserver build, the remainder
 * has moved to include/dri_internal.h and is no longer installed. The xserver
 * loader is planned to migrate to using EGL directly for loading, at which
 * point the rest of this file will go away.
 * 
 * \author Kevin E. Martin <kevin@precisioninsight.com>
 * \author Ian Romanick <idr@us.ibm.com>
 * \author Kristian Høgsberg <krh@redhat.com>
 */

#ifndef DRI_INTERFACE_H
#define DRI_INTERFACE_H

#include <stdint.h>

/**
 * \name DRI interface structures
 *
 * The following structures define the interface between the GLX client
 * side library and the DRI (direct rendering infrastructure).
 */
/*@{*/
typedef struct __DRIdisplayRec		__DRIdisplay;
typedef struct __DRIscreenRec		__DRIscreen;
typedef struct __DRIcontextRec		__DRIcontext;
typedef struct __DRIdrawableRec		__DRIdrawable;
typedef struct __DRIconfigRec		__DRIconfig;

typedef struct __DRIcoreExtensionRec		__DRIcoreExtension;
typedef struct __DRIextensionRec		__DRIextension;
typedef struct __DRIcopySubBufferExtensionRec	__DRIcopySubBufferExtension;
typedef struct __DRIswapControlExtensionRec	__DRIswapControlExtension;
typedef struct __DRItexBufferExtensionRec	__DRItexBufferExtension;
typedef struct __DRIswrastExtensionRec		__DRIswrastExtension;
typedef struct __DRIbufferRec			__DRIbuffer;
typedef struct __DRIdri2ExtensionRec		__DRIdri2Extension;
typedef struct __DRIdri2LoaderExtensionRec	__DRIdri2LoaderExtension;
typedef struct __DRI2flushExtensionRec	__DRI2flushExtension;

/*@}*/

/**
 * Extension struct.  Drivers 'inherit' from this struct by embedding
 * it as the first element in the extension struct.
 *
 * We never break API in for a DRI extension.  If we need to change
 * the way things work in a non-backwards compatible manner, we
 * introduce a new extension.  During a transition period, we can
 * leave both the old and the new extension in the driver, which
 * allows us to move to the new interface without having to update the
 * loader(s) in lock step.
 *
 * However, we can add entry points to an extension over time as long
 * as we don't break the old ones.  As we add entry points to an
 * extension, we increase the version number.  The corresponding
 * #define can be used to guard code that accesses the new entry
 * points at compile time and the version field in the extension
 * struct can be used at run-time to determine how to use the
 * extension.
 */
struct __DRIextensionRec {
    const char *name;
    int version;
};

/**
 * The first set of extension are the screen extensions, returned by
 * __DRIcore::getExtensions().  This entry point will return a list of
 * extensions and the loader can use the ones it knows about by
 * casting them to more specific extensions and advertising any GLX
 * extensions the DRI extensions enables.
 */

/**
 * Used by drivers that implement the GLX_MESA_copy_sub_buffer extension.
 */
#define __DRI_COPY_SUB_BUFFER "DRI_CopySubBuffer"
#define __DRI_COPY_SUB_BUFFER_VERSION 1
struct __DRIcopySubBufferExtensionRec {
    __DRIextension base;
    void (*copySubBuffer)(__DRIdrawable *drawable, int x, int y, int w, int h);
};

/* Valid values for format in the setTexBuffer2 function below.  These
 * values match the GLX tokens for compatibility reasons, but we
 * define them here since the DRI interface can't depend on GLX. */
#define __DRI_TEXTURE_FORMAT_NONE        0x20D8
#define __DRI_TEXTURE_FORMAT_RGB         0x20D9
#define __DRI_TEXTURE_FORMAT_RGBA        0x20DA

#define __DRI_TEX_BUFFER "DRI_TexBuffer"
#define __DRI_TEX_BUFFER_VERSION 3
struct __DRItexBufferExtensionRec {
    __DRIextension base;

    /**
     * Method to override base texture image with the contents of a
     * __DRIdrawable. 
     *
     * For GLX_EXT_texture_from_pixmap with AIGLX.  Deprecated in favor of
     * setTexBuffer2 in version 2 of this interface
     */
    void (*setTexBuffer)(__DRIcontext *pDRICtx,
			 int target,
			 __DRIdrawable *pDraw);

    /**
     * Method to override base texture image with the contents of a
     * __DRIdrawable, including the required texture format attribute.
     *
     * For GLX_EXT_texture_from_pixmap with AIGLX.
     *
     * \since 2
     */
    void (*setTexBuffer2)(__DRIcontext *pDRICtx,
			  int target,
			  int format,
			  __DRIdrawable *pDraw);
    /**
     * Method to release texture buffer in case some special platform
     * need this.
     *
     * For GLX_EXT_texture_from_pixmap with AIGLX.
     *
     * \since 3
     */
    void (*releaseTexBuffer)(__DRIcontext *pDRICtx,
			int target,
			__DRIdrawable *pDraw);
};

/**
 * Used by drivers that implement DRI2
 */
#define __DRI2_FLUSH "DRI2_Flush"
#define __DRI2_FLUSH_VERSION 4

#define __DRI2_FLUSH_DRAWABLE (1 << 0) /* the drawable should be flushed. */
#define __DRI2_FLUSH_CONTEXT  (1 << 1) /* glFlush should be called */
#define __DRI2_FLUSH_INVALIDATE_ANCILLARY (1 << 2)

enum __DRI2throttleReason {
   __DRI2_THROTTLE_SWAPBUFFER,
   __DRI2_THROTTLE_COPYSUBBUFFER,
   __DRI2_THROTTLE_FLUSHFRONT
};

struct __DRI2flushExtensionRec {
    __DRIextension base;
    void (*flush)(__DRIdrawable *drawable);

    /**
     * Ask the driver to call getBuffers/getBuffersWithFormat before
     * it starts rendering again.
     *
     * \param drawable the drawable to invalidate
     *
     * \since 3
     */
    void (*invalidate)(__DRIdrawable *drawable);

    /**
     * This function reduces the number of flushes in the driver by combining
     * several operations into one call.
     *
     * It can:
     * - throttle
     * - flush a drawable
     * - flush a context
     *
     * \param context           the context
     * \param drawable          the drawable to flush
     * \param flags             a combination of _DRI2_FLUSH_xxx flags
     * \param throttle_reason   the reason for throttling, 0 = no throttling
     *
     * \since 4
     */
    void (*flush_with_flags)(__DRIcontext *ctx,
                             __DRIdrawable *drawable,
                             unsigned flags,
                             enum __DRI2throttleReason throttle_reason);
};

/**
 * The following extensions describe loader features that the DRI driver can
 * make use of.  Some of these are mandatory, such as the DRI Loader extensions
 * for DRI2, while others are optional, and if present allow the driver to
 * expose certain features.  The loader pass in a NULL terminated array of
 * these extensions to the driver in the createNewScreen constructor.
 */

typedef struct __DRIswrastLoaderExtensionRec __DRIswrastLoaderExtension;

#define __DRI_SWRAST_IMAGE_OP_DRAW	1
#define __DRI_SWRAST_IMAGE_OP_CLEAR	2
#define __DRI_SWRAST_IMAGE_OP_SWAP	3

/**
 * SWRast Loader extension.
 */
#define __DRI_SWRAST_LOADER "DRI_SWRastLoader"
#define __DRI_SWRAST_LOADER_VERSION 6
struct __DRIswrastLoaderExtensionRec {
    __DRIextension base;

    /*
     * Drawable position and size
     */
    void (*getDrawableInfo)(__DRIdrawable *drawable,
			    int *x, int *y, int *width, int *height,
			    void *loaderPrivate);

    /**
     * Put image to drawable
     */
    void (*putImage)(__DRIdrawable *drawable, int op,
		     int x, int y, int width, int height,
		     char *data, void *loaderPrivate);

    /**
     * Get image from readable
     */
    void (*getImage)(__DRIdrawable *readable,
		     int x, int y, int width, int height,
		     char *data, void *loaderPrivate);

    /**
     * Put image to drawable
     *
     * \since 2
     */
    void (*putImage2)(__DRIdrawable *drawable, int op,
                      int x, int y, int width, int height, int stride,
                      char *data, void *loaderPrivate);

   /**
     * Put image to drawable
     *
     * \since 3
     */
   void (*getImage2)(__DRIdrawable *readable,
		     int x, int y, int width, int height, int stride,
		     char *data, void *loaderPrivate);

    /**
     * Put shm image to drawable
     *
     * \since 4
     */
    void (*putImageShm)(__DRIdrawable *drawable, int op,
                        int x, int y, int width, int height, int stride,
                        int shmid, char *shmaddr, unsigned offset,
                        void *loaderPrivate);
    /**
     * Get shm image from readable
     *
     * \since 4
     */
    void (*getImageShm)(__DRIdrawable *readable,
                        int x, int y, int width, int height,
                        int shmid, void *loaderPrivate);

   /**
     * Put shm image to drawable (v2)
     *
     * The original version fixes srcx/y to 0, and expected
     * the offset to be adjusted. This version allows src x,y
     * to not be included in the offset. This is needed to
     * avoid certain overflow checks in the X server, that
     * result in lost rendering.
     *
     * \since 5
     */
    void (*putImageShm2)(__DRIdrawable *drawable, int op,
                         int x, int y,
                         int width, int height, int stride,
                         int shmid, char *shmaddr, unsigned offset,
                         void *loaderPrivate);

    /**
     * get shm image to drawable (v2)
     *
     * There are some cases where GLX can't use SHM, but DRI
     * still tries, we need to get a return type for when to
     * fallback to the non-shm path.
     *
     * \since 6
     */
    unsigned char (*getImageShm2)(__DRIdrawable *readable,
                                  int x, int y, int width, int height,
                                  int shmid, void *loaderPrivate);
};

/**
 * Invalidate loader extension.  The presence of this extension
 * indicates to the DRI driver that the loader will call invalidate in
 * the __DRI2_FLUSH extension, whenever the needs to query for new
 * buffers.  This means that the DRI driver can drop the polling in
 * glViewport().
 *
 * The extension doesn't provide any functionality, it's only use to
 * indicate to the driver that it can use the new semantics.  A DRI
 * driver can use this to switch between the different semantics or
 * just refuse to initialize if this extension isn't present.
 */
#define __DRI_USE_INVALIDATE "DRI_UseInvalidate"
#define __DRI_USE_INVALIDATE_VERSION 1

typedef struct __DRIuseInvalidateExtensionRec __DRIuseInvalidateExtension;
struct __DRIuseInvalidateExtensionRec {
   __DRIextension base;
};

/**
 * The remaining extensions describe driver extensions, immediately
 * available interfaces provided by the driver.  To start using the
 * driver, dlsym() for the __DRI_DRIVER_EXTENSIONS symbol and look for
 * the extension you need in the array.
 */
#define __DRI_DRIVER_EXTENSIONS "__driDriverExtensions"

/**
 * This symbol replaces the __DRI_DRIVER_EXTENSIONS symbol, and will be
 * suffixed by "_drivername", allowing multiple drivers to be built into one
 * library, and also giving the driver the chance to return a variable driver
 * extensions struct depending on the driver name being loaded or any other
 * system state.
 *
 * The function prototype is:
 *
 * const __DRIextension **__driDriverGetExtensions_drivername(void);
 */
#define __DRI_DRIVER_GET_EXTENSIONS "__driDriverGetExtensions"

/**
 * Tokens for __DRIconfig attribs.  A number of attributes defined by
 * GLX or EGL standards are not in the table, as they must be provided
 * by the loader.  For example, FBConfig ID or visual ID, drawable type.
 */

#define __DRI_ATTRIB_BUFFER_SIZE		 1
#define __DRI_ATTRIB_LEVEL			 2
#define __DRI_ATTRIB_RED_SIZE			 3
#define __DRI_ATTRIB_GREEN_SIZE			 4
#define __DRI_ATTRIB_BLUE_SIZE			 5
#define __DRI_ATTRIB_LUMINANCE_SIZE		 6
#define __DRI_ATTRIB_ALPHA_SIZE			 7
#define __DRI_ATTRIB_ALPHA_MASK_SIZE		 8
#define __DRI_ATTRIB_DEPTH_SIZE			 9
#define __DRI_ATTRIB_STENCIL_SIZE		10
#define __DRI_ATTRIB_ACCUM_RED_SIZE		11
#define __DRI_ATTRIB_ACCUM_GREEN_SIZE		12
#define __DRI_ATTRIB_ACCUM_BLUE_SIZE		13
#define __DRI_ATTRIB_ACCUM_ALPHA_SIZE		14
#define __DRI_ATTRIB_SAMPLE_BUFFERS		15
#define __DRI_ATTRIB_SAMPLES			16
#define __DRI_ATTRIB_RENDER_TYPE		17
#define __DRI_ATTRIB_CONFIG_CAVEAT		18
#define __DRI_ATTRIB_CONFORMANT			19
#define __DRI_ATTRIB_DOUBLE_BUFFER		20
#define __DRI_ATTRIB_STEREO			21
#define __DRI_ATTRIB_AUX_BUFFERS		22
#define __DRI_ATTRIB_TRANSPARENT_TYPE		23
#define __DRI_ATTRIB_TRANSPARENT_INDEX_VALUE	24
#define __DRI_ATTRIB_TRANSPARENT_RED_VALUE	25
#define __DRI_ATTRIB_TRANSPARENT_GREEN_VALUE	26
#define __DRI_ATTRIB_TRANSPARENT_BLUE_VALUE	27
#define __DRI_ATTRIB_TRANSPARENT_ALPHA_VALUE	28
#define __DRI_ATTRIB_FLOAT_MODE			29
#define __DRI_ATTRIB_RED_MASK			30
#define __DRI_ATTRIB_GREEN_MASK			31
#define __DRI_ATTRIB_BLUE_MASK			32
#define __DRI_ATTRIB_ALPHA_MASK			33
#define __DRI_ATTRIB_MAX_PBUFFER_WIDTH		34
#define __DRI_ATTRIB_MAX_PBUFFER_HEIGHT		35
#define __DRI_ATTRIB_MAX_PBUFFER_PIXELS		36
#define __DRI_ATTRIB_OPTIMAL_PBUFFER_WIDTH	37
#define __DRI_ATTRIB_OPTIMAL_PBUFFER_HEIGHT	38
#define __DRI_ATTRIB_VISUAL_SELECT_GROUP	39
#define __DRI_ATTRIB_SWAP_METHOD		40
#define __DRI_ATTRIB_MAX_SWAP_INTERVAL		41
#define __DRI_ATTRIB_MIN_SWAP_INTERVAL		42
#define __DRI_ATTRIB_BIND_TO_TEXTURE_RGB	43
#define __DRI_ATTRIB_BIND_TO_TEXTURE_RGBA	44
#define __DRI_ATTRIB_BIND_TO_MIPMAP_TEXTURE	45
#define __DRI_ATTRIB_BIND_TO_TEXTURE_TARGETS	46
#define __DRI_ATTRIB_YINVERTED			47
#define __DRI_ATTRIB_FRAMEBUFFER_SRGB_CAPABLE	48
#define __DRI_ATTRIB_MUTABLE_RENDER_BUFFER	49 /* EGL_MUTABLE_RENDER_BUFFER_BIT_KHR */
#define __DRI_ATTRIB_RED_SHIFT			50
#define __DRI_ATTRIB_GREEN_SHIFT		51
#define __DRI_ATTRIB_BLUE_SHIFT			52
#define __DRI_ATTRIB_ALPHA_SHIFT		53
#define __DRI_ATTRIB_MAX			54

/* __DRI_ATTRIB_RENDER_TYPE */
#define __DRI_ATTRIB_RGBA_BIT			0x01	
#define __DRI_ATTRIB_COLOR_INDEX_BIT		0x02
#define __DRI_ATTRIB_LUMINANCE_BIT		0x04
#define __DRI_ATTRIB_FLOAT_BIT			0x08
#define __DRI_ATTRIB_UNSIGNED_FLOAT_BIT		0x10

/* __DRI_ATTRIB_CONFIG_CAVEAT */
#define __DRI_ATTRIB_SLOW_BIT			0x01
#define __DRI_ATTRIB_NON_CONFORMANT_CONFIG	0x02

/* __DRI_ATTRIB_TRANSPARENT_TYPE */
#define __DRI_ATTRIB_TRANSPARENT_RGB		0x00
#define __DRI_ATTRIB_TRANSPARENT_INDEX		0x01

/* __DRI_ATTRIB_BIND_TO_TEXTURE_TARGETS	 */
#define __DRI_ATTRIB_TEXTURE_1D_BIT		0x01
#define __DRI_ATTRIB_TEXTURE_2D_BIT		0x02
#define __DRI_ATTRIB_TEXTURE_RECTANGLE_BIT	0x04

/* __DRI_ATTRIB_SWAP_METHOD */
/* Note that with the exception of __DRI_ATTRIB_SWAP_NONE, we need to define
 * the same tokens as GLX. This is because old and current X servers will
 * transmit the driconf value grabbed from the AIGLX driver untranslated as
 * the GLX fbconfig value. __DRI_ATTRIB_SWAP_NONE is only used by dri drivers
 * to signal to the dri core that the driconfig is single-buffer.
 */
#define __DRI_ATTRIB_SWAP_NONE                  0x0000
#define __DRI_ATTRIB_SWAP_EXCHANGE              0x8061
#define __DRI_ATTRIB_SWAP_COPY                  0x8062
#define __DRI_ATTRIB_SWAP_UNDEFINED             0x8063

/**
 * This extension defines the core DRI functionality.
 *
 * Version >= 2 indicates that getConfigAttrib with __DRI_ATTRIB_SWAP_METHOD
 * returns a reliable value.
 */
#define __DRI_CORE "DRI_Core"
#define __DRI_CORE_VERSION 2

struct __DRIcoreExtensionRec {
    __DRIextension base;

    __DRIscreen *(*createNewScreen)(int screen, int fd,
				    unsigned int sarea_handle,
				    const __DRIextension **extensions,
				    const __DRIconfig ***driverConfigs,
				    void *loaderPrivate);

    void (*destroyScreen)(__DRIscreen *screen);

    const __DRIextension **(*getExtensions)(__DRIscreen *screen);

    int (*getConfigAttrib)(const __DRIconfig *config,
			   unsigned int attrib,
			   unsigned int *value);

    int (*indexConfigAttrib)(const __DRIconfig *config, int index,
			     unsigned int *attrib, unsigned int *value);

    __DRIdrawable *(*createNewDrawable)(__DRIscreen *screen,
					const __DRIconfig *config,
					unsigned int drawable_id,
					unsigned int head,
					void *loaderPrivate);

    void (*destroyDrawable)(__DRIdrawable *drawable);

    void (*swapBuffers)(__DRIdrawable *drawable);

    __DRIcontext *(*createNewContext)(__DRIscreen *screen,
				      const __DRIconfig *config,
				      __DRIcontext *shared,
				      void *loaderPrivate);

    int (*copyContext)(__DRIcontext *dest,
		       __DRIcontext *src,
		       unsigned long mask);

    void (*destroyContext)(__DRIcontext *context);

    int (*bindContext)(__DRIcontext *ctx,
		       __DRIdrawable *pdraw,
		       __DRIdrawable *pread);

    int (*unbindContext)(__DRIcontext *ctx);
};

/**
 * This extension provides alternative screen, drawable and context
 * constructors for swrast DRI functionality.  This is used in
 * conjunction with the core extension.
 */
#define __DRI_SWRAST "DRI_SWRast"
#define __DRI_SWRAST_VERSION 4

struct __DRIswrastExtensionRec {
    __DRIextension base;

    __DRIscreen *(*createNewScreen)(int screen,
				    const __DRIextension **extensions,
				    const __DRIconfig ***driver_configs,
				    void *loaderPrivate);

    __DRIdrawable *(*createNewDrawable)(__DRIscreen *screen,
					const __DRIconfig *config,
					void *loaderPrivate);

   /* Since version 2 */
   __DRIcontext *(*createNewContextForAPI)(__DRIscreen *screen,
                                           int api,
                                           const __DRIconfig *config,
                                           __DRIcontext *shared,
                                           void *data);

   /**
    * Create a context for a particular API with a set of attributes
    *
    * \since version 3
    *
    * \sa __DRIdri2ExtensionRec::createContextAttribs
    */
   __DRIcontext *(*createContextAttribs)(__DRIscreen *screen,
					 int api,
					 const __DRIconfig *config,
					 __DRIcontext *shared,
					 unsigned num_attribs,
					 const uint32_t *attribs,
					 unsigned *error,
					 void *loaderPrivate);

   /**
    * createNewScreen() with the driver extensions passed in.
    *
    * \since version 4
    */
   __DRIscreen *(*createNewScreen2)(int screen,
                                    const __DRIextension **loader_extensions,
                                    const __DRIextension **driver_extensions,
                                    const __DRIconfig ***driver_configs,
                                    void *loaderPrivate);

};

/** Common DRI function definitions, shared among DRI2 and Image extensions
 */

typedef __DRIscreen *
(*__DRIcreateNewScreen2Func)(int screen, int fd,
                             const __DRIextension **extensions,
                             const __DRIextension **driver_extensions,
                             const __DRIconfig ***driver_configs,
                             void *loaderPrivate);

typedef __DRIdrawable *
(*__DRIcreateNewDrawableFunc)(__DRIscreen *screen,
                              const __DRIconfig *config,
                              void *loaderPrivate);

typedef __DRIcontext *
(*__DRIcreateContextAttribsFunc)(__DRIscreen *screen,
                                 int api,
                                 const __DRIconfig *config,
                                 __DRIcontext *shared,
                                 unsigned num_attribs,
                                 const uint32_t *attribs,
                                 unsigned *error,
                                 void *loaderPrivate);

typedef unsigned int
(*__DRIgetAPIMaskFunc)(__DRIscreen *screen);

/**
 * DRI2 Loader extension.
 */
#define __DRI_BUFFER_FRONT_LEFT		0
#define __DRI_BUFFER_BACK_LEFT		1
#define __DRI_BUFFER_FRONT_RIGHT	2
#define __DRI_BUFFER_BACK_RIGHT		3
#define __DRI_BUFFER_DEPTH		4
#define __DRI_BUFFER_STENCIL		5
#define __DRI_BUFFER_ACCUM		6
#define __DRI_BUFFER_FAKE_FRONT_LEFT	7
#define __DRI_BUFFER_FAKE_FRONT_RIGHT	8
#define __DRI_BUFFER_DEPTH_STENCIL	9  /**< Only available with DRI2 1.1 */
#define __DRI_BUFFER_HIZ		10

/* Inofficial and for internal use. Increase when adding a new buffer token. */
#define __DRI_BUFFER_COUNT		11

struct __DRIbufferRec {
    unsigned int attachment;
    unsigned int name;
    unsigned int pitch;
    unsigned int cpp;
    unsigned int flags;
};

#define __DRI_DRI2_LOADER "DRI_DRI2Loader"
#define __DRI_DRI2_LOADER_VERSION 5

enum dri_loader_cap {
   /* Whether the loader handles RGBA channel ordering correctly. If not,
    * only BGRA ordering can be exposed.
    */
   DRI_LOADER_CAP_RGBA_ORDERING,
   DRI_LOADER_CAP_FP16,
};

struct __DRIdri2LoaderExtensionRec {
    __DRIextension base;

    __DRIbuffer *(*getBuffers)(__DRIdrawable *driDrawable,
			       int *width, int *height,
			       unsigned int *attachments, int count,
			       int *out_count, void *loaderPrivate);

    /**
     * Flush pending front-buffer rendering
     *
     * Any rendering that has been performed to the
     * \c __DRI_BUFFER_FAKE_FRONT_LEFT will be flushed to the
     * \c __DRI_BUFFER_FRONT_LEFT.
     *
     * \param driDrawable    Drawable whose front-buffer is to be flushed
     * \param loaderPrivate  Loader's private data that was previously passed
     *                       into __DRIdri2ExtensionRec::createNewDrawable
     *
     * \since 2
     */
    void (*flushFrontBuffer)(__DRIdrawable *driDrawable, void *loaderPrivate);


    /**
     * Get list of buffers from the server
     *
     * Gets a list of buffer for the specified set of attachments.  Unlike
     * \c ::getBuffers, this function takes a list of attachments paired with
     * opaque \c unsigned \c int value describing the format of the buffer.
     * It is the responsibility of the caller to know what the service that
     * allocates the buffers will expect to receive for the format.
     *
     * \param driDrawable    Drawable whose buffers are being queried.
     * \param width          Output where the width of the buffers is stored.
     * \param height         Output where the height of the buffers is stored.
     * \param attachments    List of pairs of attachment ID and opaque format
     *                       requested for the drawable.
     * \param count          Number of attachment / format pairs stored in
     *                       \c attachments.
     * \param loaderPrivate  Loader's private data that was previously passed
     *                       into __DRIdri2ExtensionRec::createNewDrawable.
     *
     * \since 3
     */
    __DRIbuffer *(*getBuffersWithFormat)(__DRIdrawable *driDrawable,
					 int *width, int *height,
					 unsigned int *attachments, int count,
					 int *out_count, void *loaderPrivate);

    /**
     * Return a loader capability value. If the loader doesn't know the enum,
     * it will return 0.
     *
     * \param loaderPrivate The last parameter of createNewScreen or
     *                      createNewScreen2.
     * \param cap           See the enum.
     *
     * \since 4
     */
    unsigned (*getCapability)(void *loaderPrivate, enum dri_loader_cap cap);

    /**
     * Clean up any loader state associated with an image.
     *
     * \param loaderPrivate  Loader's private data that was previously passed
     *                       into a __DRIimageExtensionRec::createImage function
     * \since 5
     */
    void (*destroyLoaderImageState)(void *loaderPrivate);
};

/**
 * This extension provides alternative screen, drawable and context
 * constructors for DRI2.
 */
#define __DRI_DRI2 "DRI_DRI2"
#define __DRI_DRI2_VERSION 4

#define __DRI_API_OPENGL	0	/**< OpenGL compatibility profile */
#define __DRI_API_GLES		1	/**< OpenGL ES 1.x */
#define __DRI_API_GLES2		2	/**< OpenGL ES 2.x */
#define __DRI_API_OPENGL_CORE	3	/**< OpenGL 3.2+ core profile */
#define __DRI_API_GLES3		4	/**< OpenGL ES 3.x */

#define __DRI_CTX_ATTRIB_MAJOR_VERSION		0
#define __DRI_CTX_ATTRIB_MINOR_VERSION		1

/* These must alias the GLX/EGL values. */
#define __DRI_CTX_ATTRIB_FLAGS			2
#define __DRI_CTX_FLAG_DEBUG			0x00000001
#define __DRI_CTX_FLAG_FORWARD_COMPATIBLE	0x00000002
#define __DRI_CTX_FLAG_ROBUST_BUFFER_ACCESS	0x00000004
#define __DRI_CTX_FLAG_NO_ERROR			0x00000008 /* Deprecated, do not use */
/* Not yet implemented but placed here to reserve the alias with GLX */
#define __DRI_CTX_FLAG_RESET_ISOLATION          0x00000008

#define __DRI_CTX_ATTRIB_RESET_STRATEGY		3
#define __DRI_CTX_RESET_NO_NOTIFICATION		0
#define __DRI_CTX_RESET_LOSE_CONTEXT		1

/**
 * \name Context priority levels.
 */
#define __DRI_CTX_ATTRIB_PRIORITY		4
#define __DRI_CTX_PRIORITY_LOW			0
#define __DRI_CTX_PRIORITY_MEDIUM		1
#define __DRI_CTX_PRIORITY_HIGH			2

#define __DRI_CTX_ATTRIB_RELEASE_BEHAVIOR	5
#define __DRI_CTX_RELEASE_BEHAVIOR_NONE         0
#define __DRI_CTX_RELEASE_BEHAVIOR_FLUSH        1

#define __DRI_CTX_ATTRIB_NO_ERROR               6

#define __DRI_CTX_NUM_ATTRIBS                   7

/**
 * \name Reasons that __DRIdri2Extension::createContextAttribs might fail
 */
/*@{*/
/** Success! */
#define __DRI_CTX_ERROR_SUCCESS			0

/** Memory allocation failure */
#define __DRI_CTX_ERROR_NO_MEMORY		1

/** Client requested an API (e.g., OpenGL ES 2.0) that the driver can't do. */
#define __DRI_CTX_ERROR_BAD_API			2

/** Client requested an API version that the driver can't do. */
#define __DRI_CTX_ERROR_BAD_VERSION		3

/** Client requested a flag or combination of flags the driver can't do. */
#define __DRI_CTX_ERROR_BAD_FLAG		4

/** Client requested an attribute the driver doesn't understand. */
#define __DRI_CTX_ERROR_UNKNOWN_ATTRIBUTE	5

/** Client requested a flag the driver doesn't understand. */
#define __DRI_CTX_ERROR_UNKNOWN_FLAG		6
/*@}*/

struct __DRIdri2ExtensionRec {
    __DRIextension base;

    __DRIscreen *(*createNewScreen)(int screen, int fd,
				    const __DRIextension **extensions,
				    const __DRIconfig ***driver_configs,
				    void *loaderPrivate);

   __DRIcreateNewDrawableFunc   createNewDrawable;
   __DRIcontext *(*createNewContext)(__DRIscreen *screen,
                                     const __DRIconfig *config,
                                     __DRIcontext *shared,
                                     void *loaderPrivate);

   /* Since version 2 */
   __DRIgetAPIMaskFunc          getAPIMask;

   __DRIcontext *(*createNewContextForAPI)(__DRIscreen *screen,
					   int api,
					   const __DRIconfig *config,
					   __DRIcontext *shared,
					   void *data);

   __DRIbuffer *(*allocateBuffer)(__DRIscreen *screen,
				  unsigned int attachment,
				  unsigned int format,
				  int width,
				  int height);
   void (*releaseBuffer)(__DRIscreen *screen,
			 __DRIbuffer *buffer);

   /**
    * Create a context for a particular API with a set of attributes
    *
    * \since version 3
    *
    * \sa __DRIswrastExtensionRec::createContextAttribs
    */
   __DRIcreateContextAttribsFunc        createContextAttribs;

   /**
    * createNewScreen with the driver's extension list passed in.
    *
    * \since version 4
    */
   __DRIcreateNewScreen2Func            createNewScreen2;
};

/**
 * Robust context driver extension.
 *
 * Existence of this extension means the driver can accept the
 * \c __DRI_CTX_FLAG_ROBUST_BUFFER_ACCESS flag and the
 * \c __DRI_CTX_ATTRIB_RESET_STRATEGY attribute in
 * \c __DRIdri2ExtensionRec::createContextAttribs.
 */
#define __DRI2_ROBUSTNESS "DRI_Robustness"
#define __DRI2_ROBUSTNESS_VERSION 1

typedef struct __DRIrobustnessExtensionRec __DRIrobustnessExtension;
struct __DRIrobustnessExtensionRec {
   __DRIextension base;
};

#endif
