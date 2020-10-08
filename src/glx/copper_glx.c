/*
 * Copyright 2020 Red Hat, Inc.
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

#if defined(GLX_DIRECT_RENDERING)

#include <xcb/xproto.h>
#include <xcb/shm.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include "glxclient.h"
#include <dlfcn.h>
#include "dri_common.h"
#include "copper_priv.h"
#include <assert.h>

static void *
copper_get_instance(void *_vkscr)
{
   struct copper_screen *vkscr = _vkscr; 
   return vkscr->instance;
}

static struct __DRIcopperLoaderExtensionRec copperLoaderExtension = {
    .base = { __DRI_COPPER_LOADER, 1 },
    .GetInstance = copper_get_instance,
};

static const __DRIextension *copper_loader[] = {
   &copperLoaderExtension.base,
   NULL
};

static void
copper_destroy_context(struct glx_context *context)
{
   struct copper_context *pcp = (struct copper_context *) context;
   struct copper_screen *psc = (struct copper_screen *) context->psc;

   driReleaseDrawables(&pcp->base);

   free((char *) context->extensions);

   (*psc->core->destroyContext) (pcp->driContext);

   free(pcp);
}

static int
copper_bind_context(struct glx_context *context, struct glx_context *old,
 		    GLXDrawable draw, GLXDrawable read)
{
   struct copper_context *pcp = (struct copper_context *) context;
   struct copper_screen *psc = (struct copper_screen *) pcp->base.psc;
   struct copper_drawable *pdraw, *pread;

   pdraw = (struct copper_drawable *) driFetchDrawable(context, draw);
   pread = (struct copper_drawable *) driFetchDrawable(context, read);

   driReleaseDrawables(&pcp->base);

   if ((*psc->core->bindContext) (pcp->driContext,
                                  pdraw ? pdraw->driDrawable : NULL,
                                  pread ? pread->driDrawable : NULL))
      return Success;

   return GLXBadContext;
}

static void
copper_unbind_context(struct glx_context *context, struct glx_context *new)
{
   struct copper_context *pcp = (struct copper_context *) context;
   struct copper_screen *psc = (struct copper_screen *) pcp->base.psc;

   (*psc->core->unbindContext) (pcp->driContext);
}

static const struct glx_context_vtable copper_context_vtable = {
   .destroy             = copper_destroy_context,
   .bind                = copper_bind_context,
   .unbind              = copper_unbind_context,
   .wait_gl             = NULL,
   .wait_x              = NULL,
   .use_x_font          = DRI_glXUseXFont,
   .bind_tex_image      = NULL,
   .release_tex_image   = NULL,
   .get_proc_address    = NULL,
};

static struct glx_context *
copper_create_context_attribs(struct glx_screen *base,
			     struct glx_config *config_base,
			     struct glx_context *shareList,
			     unsigned num_attribs,
			     const uint32_t *attribs,
			     unsigned *error)
{
   struct copper_context *pcp, *pcp_shared;
   __GLXDRIconfigPrivate *config = (__GLXDRIconfigPrivate *) config_base;
   struct copper_screen *psc = (struct copper_screen *) base;
   __DRIcontext *shared = NULL;

   uint32_t minor_ver;
   uint32_t major_ver;
   uint32_t renderType;
   uint32_t flags;
   unsigned api;
   int reset;
   int release;
   uint32_t ctx_attribs[2 * 5];
   unsigned num_ctx_attribs = 0;

   if (!psc->base.driScreen)
      return NULL;

   /* Remap the GLX tokens to DRI2 tokens.
    */
   if (!dri2_convert_glx_attribs(num_attribs, attribs,
                                 &major_ver, &minor_ver, &renderType, &flags,
                                 &api, &reset, &release, error))
      return NULL;

   if (!dri2_check_no_error(flags, shareList, major_ver, error))
      return NULL;

   /* Check the renderType value */
   if (!validate_renderType_against_config(config_base, renderType)) {
       return NULL;
   }

   if (reset != __DRI_CTX_RESET_NO_NOTIFICATION)
      return NULL;

   if (release != __DRI_CTX_RELEASE_BEHAVIOR_FLUSH &&
       release != __DRI_CTX_RELEASE_BEHAVIOR_NONE)
      return NULL;

   if (shareList) {
      pcp_shared = (struct copper_context *) shareList;
      shared = pcp_shared->driContext;
   }

   pcp = calloc(1, sizeof *pcp);
   if (pcp == NULL)
      return NULL;

   if (!glx_context_init(&pcp->base, &psc->base, config_base)) {
      free(pcp);
      return NULL;
   }

   ctx_attribs[num_ctx_attribs++] = __DRI_CTX_ATTRIB_MAJOR_VERSION;
   ctx_attribs[num_ctx_attribs++] = major_ver;
   ctx_attribs[num_ctx_attribs++] = __DRI_CTX_ATTRIB_MINOR_VERSION;
   ctx_attribs[num_ctx_attribs++] = minor_ver;
   if (release != __DRI_CTX_RELEASE_BEHAVIOR_FLUSH) {
       ctx_attribs[num_ctx_attribs++] = __DRI_CTX_ATTRIB_RELEASE_BEHAVIOR;
       ctx_attribs[num_ctx_attribs++] = release;
   }

   if (flags != 0) {
      ctx_attribs[num_ctx_attribs++] = __DRI_CTX_ATTRIB_FLAGS;

      /* The current __DRI_CTX_FLAG_* values are identical to the
       * GLX_CONTEXT_*_BIT values.
       */
      ctx_attribs[num_ctx_attribs++] = flags;

      if (flags & __DRI_CTX_FLAG_NO_ERROR)
         pcp->base.noError = GL_TRUE;
   }

   pcp->base.renderType = renderType;

   pcp->driContext =
      (*psc->copper->createContextAttribs) (psc->driScreen,
					    api,
					    config ? config->driConfig : 0,
					    shared,
					    num_ctx_attribs / 2,
					    ctx_attribs,
					    error,
					    pcp);
   if (pcp->driContext == NULL) {
      free(pcp);
      return NULL;
   }

   pcp->base.vtable = &copper_context_vtable;

   return &pcp->base;
}

/* XXX can we please do this generically for all drivers */
static struct glx_context *
copper_create_context(struct glx_screen *base,
		     struct glx_config *config_base,
		     struct glx_context *shareList, int renderType)
{
   uint32_t attribs[2], num_attribs = 0, error = 0;

   /* XXX maybe more */
   attribs[num_attribs++] = GLX_RENDER_TYPE;
   attribs[num_attribs++] = renderType;

   return copper_create_context_attribs(base, config_base, shareList,
                                       num_attribs / 2, attribs, &error);
}

static void
copperDestroyDrawable(__GLXDRIdrawable * pdraw)
{
   struct copper_drawable *pdp = (struct copper_drawable *) pdraw;
   struct copper_screen *psc = (struct copper_screen *) pdp->base.psc;

   (*psc->core->destroyDrawable) (pdp->driDrawable);

   // XDestroyDrawable(pdp, pdraw->psc->dpy, pdraw->drawable);
   free(pdp);
}

static __GLXDRIdrawable *
copperCreateDrawable(struct glx_screen *base, XID xDrawable,
		    GLXDrawable drawable, struct glx_config *modes)
{
   struct copper_drawable *pdp;
   __GLXDRIconfigPrivate *config = (__GLXDRIconfigPrivate *) modes;
   struct copper_screen *psc = (struct copper_screen *) base;
   const __DRIcopperExtension *copper = psc->copper;
   // Display *dpy = psc->base.dpy;

   pdp = calloc(1, sizeof(*pdp));
   if (!pdp)
      return NULL;

   /* Create a new drawable */
   pdp->driDrawable =
      (*copper->createNewDrawable) (psc->driScreen, config->driConfig, pdp);

   if (!pdp->driDrawable) {
      free(pdp);
      return NULL;
   }

   pdp->base.psc = base;
   pdp->base.destroyDrawable = copperDestroyDrawable;

   return &pdp->base;
}

static int64_t
copperSwapBuffers(__GLXDRIdrawable * pdraw,
                 int64_t target_msc, int64_t divisor, int64_t remainder,
                 Bool flush)
{
   struct copper_drawable *pdp = (struct copper_drawable *) pdraw;
   struct copper_screen *psc = (struct copper_screen *) pdp->base.psc;

   (void) target_msc;
   (void) divisor;
   (void) remainder;

   if (flush) {
      glFlush();
   }

   (*psc->copper->swapBuffers) (pdp->driDrawable);

   return 0;
}

static void
copperDestroyScreen(struct glx_screen *base)
{
   struct copper_screen *psc = (struct copper_screen *) base;

   /* Free the direct rendering per screen data */
   (*psc->core->destroyScreen) (psc->driScreen);
   // I think this is okay to skip?
   // driDestroyConfigs(psc->driver_configs);
   psc->driScreen = NULL;
   free(psc);
}

static const struct glx_screen_vtable copper_screen_vtable = {
   .create_context         = copper_create_context,
   .create_context_attribs = copper_create_context_attribs,
#if 0
   .query_renderer_integer = copper_query_renderer_integer,
   .query_renderer_string  = copper_query_renderer_string,
#endif
};

static void
copperBindExtensions(struct copper_screen *psc, const __DRIextension **extensions)
{
   __glXEnableDirectExtension(&psc->base, "GLX_SGI_make_current_read");
   __glXEnableDirectExtension(&psc->base, "GLX_ARB_create_context");
   __glXEnableDirectExtension(&psc->base, "GLX_ARB_create_context_profile");
   __glXEnableDirectExtension(&psc->base, "GLX_EXT_create_context_es_profile");
   __glXEnableDirectExtension(&psc->base, "GLX_EXT_create_context_es2_profile");
}

static VkPhysicalDevice
choose_pdev(struct copper_display *vkdpy)
{
   unsigned count = 0;
   VkPhysicalDevice *devs, dev;

   vkdpy->copper->EnumeratePhysicalDevices(vkdpy->instance, &count,
                                           (void ***)&devs);
   if (count == 0)
      return VK_NULL_HANDLE;

   /* XXX walk the list of devices for screen/visual compatibility */

   dev = devs[0];
   free(devs);
   return dev;
}

static struct glx_screen *
copperCreateScreen(int screen, struct glx_display *priv)
{
   __GLXDRIscreen *psp;
   const __DRIconfig **driver_configs;
   struct copper_screen *vkscr;
   struct copper_display *vkdpy = (struct copper_display *)priv->copperDisplay;
   const __DRIextension **extensions = vkdpy->extensions;
   struct glx_config *configs = NULL, *visuals = NULL;
   VkPhysicalDevice pdev;

   vkscr = calloc(1, sizeof *vkscr);
   if (vkscr == NULL)
      return NULL;
   vkscr->core = vkdpy->core;
   vkscr->copper = vkdpy->copper;
   vkscr->instance = vkdpy->instance;

   if (!glx_screen_init(&vkscr->base, screen, priv)) {
      free(vkscr);
      return NULL;
   }

   pdev = choose_pdev(vkdpy);

   vkscr->driScreen = vkscr->copper->createVkScreen(pdev, copper_loader,
                                                    extensions,
                                                    &driver_configs,
                                                    vkscr);
   if (vkscr->driScreen == NULL) {
      ErrorMessageF("failed to create copper screen\n");
      goto handle_error;
   }

   extensions = vkscr->core->getExtensions(vkscr->driScreen);
   copperBindExtensions(vkscr, extensions);

   // we're not "converting" any configs because no
   configs = vkscr->base.configs;
   visuals = vkscr->base.visuals;

   if (!configs || !visuals) {
       ErrorMessageF("No matching fbConfigs or visuals found\n");
       goto handle_error;
   }

#if 0
   glx_config_destroy_list(vkscr->base.configs);
   vkscr->base.configs = configs;
   glx_config_destroy_list(vkscr->base.visuals);
   vkscr->base.visuals = visuals;
#endif

   // vkscr->driver_configs = driver_configs;

   vkscr->base.vtable = &copper_screen_vtable;
   psp = &vkscr->vtable;
   vkscr->base.driScreen = psp;
   psp->destroyScreen = copperDestroyScreen;
   psp->createDrawable = copperCreateDrawable;
   psp->swapBuffers = copperSwapBuffers;

   return &vkscr->base;

 handle_error:
   if (configs)
       glx_config_destroy_list(configs);
   if (visuals)
       glx_config_destroy_list(visuals);
   if (vkscr->driScreen)
       vkscr->copper->destroyScreen(vkscr->driScreen);
   vkscr->driScreen = NULL;

   glx_screen_cleanup(&vkscr->base);
   free(vkscr);

   CriticalErrorMessageF("failed to load zink\n");

   return NULL;
}

static void
copperDestroyDisplay(__GLXDRIdisplay * dpy)
{
   struct copper_display *vkdpy = (struct copper_display *)dpy;
   
   // vkdpy->copper->DestroyInstance(vkdpy->instance, NULL);
   dlclose(vkdpy->driver);
   free(dpy);
}

_X_HIDDEN __GLXDRIdisplay *
copperCreateDisplay(Display * dpy)
{
   int i = 0;
   struct copper_display *vkdpy = NULL;
   __DRIcopperExtension *copper = NULL;
   const __DRIextension **extensions = NULL;

   vkdpy = calloc(1, sizeof *vkdpy);
   if (vkdpy == NULL)
      return NULL;

   /* Similar to kms_swrast, we use a different driver name in order to
    * select a different driver vtable. It's still zink, underneath.
    */
   vkdpy->extensions = extensions = driOpenDriver("copper", &vkdpy->driver);
   if (extensions == NULL)
      goto out;

   for (i = 0; extensions[i]; i++) {
      if (strcmp(extensions[i]->name, __DRI_CORE) == 0)
	 vkdpy->core = (__DRIcoreExtension *) extensions[i];
      if (strcmp(extensions[i]->name, __DRI_COPPER) == 0)
	 vkdpy->copper = copper = (__DRIcopperExtension *) extensions[i];
   }

   if (copper == NULL) {
      ErrorMessageF("copper extensions not found\n");
      goto out;
   }

   vkdpy->instance = copper->CreateInstance();
   if (vkdpy->instance == VK_NULL_HANDLE) {
      ErrorMessageF("Failed to create vulkan instance\n");
      goto out;
   }
   /* XXX check instance supports VK_KHR_xlib_surface */

#define GIPA(i, x) vkdpy->x = (PFN_##x)copper->GetInstanceProcAddr(i, #x)
   GIPA(vkdpy->instance, vkEnumeratePhysicalDevices);
   GIPA(vkdpy->instance, vkGetPhysicalDeviceProperties);

   vkdpy->base.destroyDisplay = copperDestroyDisplay;
   vkdpy->base.createScreen = copperCreateScreen;

   return &vkdpy->base;

out:

   free(vkdpy);
   return NULL;
}

#endif /* GLX_DIRECT_RENDERING */
