/*
 * Copyright © Microsoft Corporation
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#if defined(GLX_DIRECT_RENDERING)

#include <dlfcn.h>

#include "glxclient.h"
#include "dri_common.h"

// Wrap the DXCore stuff in a namespace, so that the Win32 types
// don't conflict with the X11 types - specifically BOOL
namespace DXCore
{
#ifndef _WIN32
#include <wsl/winadapter.h>
#endif

#include <directx/dxcore.h>
}

struct dxg_context : public glx_context
{
    __DRIcontext *driContext;
};

struct dxg_screen : public glx_screen
{
    __DRIscreen *driScreen;
    const __DRIcoreExtension *core;
    const __DRIdxgExtension *dxg;
};

struct dxg_drawable : public __GLXDRIdrawable
{
    __DRIdrawable *driDrawable;
    struct glx_config *config;
    void *d3d_resource;
};

static void
dxg_destroy_drawable(__GLXDRIdrawable *draw)
{
    dxg_drawable *dxgdraw = static_cast<dxg_drawable*>(draw);
    dxg_screen *dxgscr = static_cast<dxg_screen*>(draw->psc);
    dxgscr->core->destroyDrawable(dxgdraw->driDrawable);
    free(dxgdraw);
}

static __GLXDRIdrawable *
dxg_create_drawable(glx_screen *screen,
                    XID drawable,
                    GLXDrawable glxDrawable,
                    glx_config *config)
{
    dxg_drawable *dxgdraw = reinterpret_cast<dxg_drawable*>(calloc(1, sizeof(*dxgdraw)));
    if (!dxgdraw)
        return nullptr;

    dxg_screen *dxgscr = static_cast<dxg_screen*>(screen);
    __GLXDRIconfigPrivate *config_priv = reinterpret_cast<__GLXDRIconfigPrivate*>(config);

    dxgdraw->driDrawable = dxgscr->dxg->createNewDrawable(dxgscr->driScreen,
                                                          config_priv->driConfig,
                                                          dxgdraw);
    if (!dxgdraw->driDrawable) {
        free(dxgdraw);
        return nullptr;
    }

    dxgdraw->psc = dxgscr;
    dxgdraw->destroyDrawable = dxg_destroy_drawable;

    return dxgdraw;
}

static void
dxg_destroy_context(glx_context *context)
{
    dxg_context *dxgctx = static_cast<dxg_context*>(context);
    dxg_screen *dxgscr = static_cast<dxg_screen*>(context->psc);
    driReleaseDrawables(context);
    dxgscr->core->destroyContext(dxgctx->driContext);
    free(dxgctx);
}

static int
dxg_bind_context(glx_context *context, glx_context *old,
                 GLXDrawable draw, GLXDrawable read)
{
    dxg_drawable *pdraw = static_cast<dxg_drawable*>(driFetchDrawable(context, draw)),
                 *pread = static_cast<dxg_drawable*>(driFetchDrawable(context, read));
    driReleaseDrawables(context);

    if (static_cast<dxg_screen*>(context->psc)->core->bindContext(
        static_cast<dxg_context*>(context)->driContext,
        pdraw ? pdraw->driDrawable : nullptr,
        pread ? pread->driDrawable : nullptr))
        return Success;
    return GLXBadContext;
}

static void
dxg_unbind_context(glx_context *context, glx_context *)
{
    static_cast<dxg_screen*>(context->psc)->core->unbindContext(
        static_cast<dxg_context*>(context)->driContext);
}

glx_context_vtable dxg_context_vtable {
    dxg_destroy_context,
    dxg_bind_context,
    dxg_unbind_context,
};

static glx_context *
dxg_create_context_attribs(glx_screen *psc,
						   glx_config *config,
						   glx_context *shareList,
						   unsigned num_attribs,
						   const uint32_t *attribs,
						   unsigned *error)
{
    dxg_screen *dxgscr = static_cast<dxg_screen*>(psc);
    if (!dxgscr->driScreen)
        return nullptr;

    uint32_t minor_ver;
    uint32_t major_ver;
    uint32_t renderType;
    uint32_t flags;
    unsigned api;
    int reset;
    int release;
    uint32_t ctx_attribs[2 * 5];
    unsigned num_ctx_attribs = 0;

    /* Remap the GLX tokens to DRI2 tokens.
     */
    if (!dri2_convert_glx_attribs(num_attribs, attribs,
                                  &major_ver, &minor_ver, &renderType, &flags,
                                  &api, &reset, &release, error))
        return nullptr;

    if (!dri2_check_no_error(flags, shareList, major_ver, error))
        return nullptr;

    /* Check the renderType value */
    if (!validate_renderType_against_config(config, renderType)) {
        return nullptr;
    }

    if (reset != __DRI_CTX_RESET_NO_NOTIFICATION)
        return nullptr;

    if (release != __DRI_CTX_RELEASE_BEHAVIOR_FLUSH &&
        release != __DRI_CTX_RELEASE_BEHAVIOR_NONE)
        return nullptr;

    dxg_context *dxgctx_shared = nullptr;
    __DRIcontext *shared = nullptr;
    if (shareList) {
        if (shareList->vtable->destroy != dxg_destroy_context)
            return nullptr;
        dxgctx_shared = static_cast<dxg_context*>(shareList);
        shared = dxgctx_shared->driContext;
    }

    dxg_context *dxgctx = reinterpret_cast<dxg_context*>(calloc(1, sizeof(*dxgctx)));
    if (!dxgctx)
        return nullptr;

    if (!glx_context_init(dxgctx, dxgscr, config)) {
        free(dxgctx);
        return nullptr;
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
            dxgctx->noError = GL_TRUE;
    }

    dxgctx->renderType = renderType;

    dxgctx->driContext =
        dxgscr->dxg->createNewContext(dxgscr->driScreen,
                                      api,
                                      config ? reinterpret_cast<__GLXDRIconfigPrivate*>(config)->driConfig : 0,
                                      shared,
                                      num_ctx_attribs / 2,
                                      ctx_attribs,
                                      error,
                                      dxgctx);
    if (dxgctx->driContext == NULL) {
        free(dxgctx);
        return nullptr;
    }

    dxgctx->vtable = &dxg_context_vtable;

    return dxgctx;
}

static void
dxg_destroy_screen(glx_screen *scr)
{
    dxg_screen *dxgscr = static_cast<dxg_screen*>(scr);
    dxgscr->core->destroyScreen(dxgscr->driScreen);
    free(dxgscr);
}

glx_screen_vtable dxg_glx_screen_vtable {
    dri_common_create_context,
    dxg_create_context_attribs,
    // XXX
};

__GLXDRIscreen dxg_glx_dri_screen_vtable {
    dxg_destroy_screen,
    dxg_create_drawable,
};

struct dxg_display : public __GLXDRIdisplay
{
    void *driver;
    const __DRIcoreExtension *core;
    const __DRIdxgExtension *dxg;
    const __DRIextension **extensions;

    DXCore::IDXCoreAdapterFactory *dxcore_factory;
    void *libdxcore;
};

static glx_screen *
dxg_create_screen(int screen, glx_display * priv)
{
    const __DRIextension *empty_extensions[] = { NULL };

    dxg_screen *dxgscr = reinterpret_cast<dxg_screen*>(calloc(1, sizeof(*dxgscr)));
    if (!dxgscr)
        return nullptr;

    dxg_display *dxgdpy = static_cast<dxg_display*>(priv->dxgDisplay);
    glx_screen *base = dxgscr;
    dxgscr->core = dxgdpy->core;
    dxgscr->dxg = dxgdpy->dxg;

    if (!glx_screen_init(dxgscr, screen, priv)) {
        free(dxgscr);
        return nullptr;
    }

    const __DRIconfig **driver_configs;
    glx_config *configs = nullptr, *visuals = nullptr;
    dxgscr->driScreen = dxgscr->dxg->createD3DScreen(nullptr, empty_extensions,
                                                     dxgdpy->extensions,
                                                     &driver_configs, dxgscr);

    if (!dxgscr->driScreen) {
        ErrorMessageF("failed to create D3D screen\n");
        goto fail;
    }

    __glXEnableDirectExtension(dxgscr, "GLX_SGI_make_current_read");
    __glXEnableDirectExtension(dxgscr, "GLX_ARB_create_context");
    __glXEnableDirectExtension(dxgscr, "GLX_ARB_create_context_profile");
    __glXEnableDirectExtension(dxgscr, "GLX_EXT_create_context_es_profile");
    __glXEnableDirectExtension(dxgscr, "GLX_EXT_create_context_es2_profile");

    configs = driConvertConfigs(dxgscr->core, dxgscr->configs, driver_configs);
    glx_config_destroy_list(dxgscr->configs);
    dxgscr->configs = configs;

    visuals = driConvertConfigs(dxgscr->core, dxgscr->visuals, driver_configs);
    glx_config_destroy_list(dxgscr->visuals);
    dxgscr->visuals = visuals;

    base->vtable = &dxg_glx_screen_vtable; // XXX
    base->driScreen = &dxg_glx_dri_screen_vtable;

    return dxgscr;

fail:
    glx_screen_cleanup(dxgscr);
    free(dxgscr);

    CriticalErrorMessageF("failed to load d3d12\n");

    return nullptr;
}

static void
dxg_destroy_display(__GLXDRIdisplay *dpy)
{
    dxg_display *dxgdpy = static_cast<dxg_display*>(dpy);
    if (dxgdpy) {
        if (dxgdpy->dxcore_factory)
            dxgdpy->dxcore_factory->Release();
        if (dxgdpy->driver)
            dlclose(dxgdpy->driver);
        if (dxgdpy->libdxcore)
            dlclose(dxgdpy->libdxcore);
    }
    free(dxgdpy);
}

_X_HIDDEN __GLXDRIdisplay *
dxg_create_display(Display * dpy)
{
    dxg_display *dxgdpy = reinterpret_cast<dxg_display*>(calloc(1, sizeof(*dxgdpy)));
    if (!dxgdpy)
        return nullptr;

    dxgdpy->extensions = driOpenDriver("dxg", &dxgdpy->driver);
    if (!dxgdpy->extensions)
        goto fail;

    for (unsigned i = 0; dxgdpy->extensions[i]; ++i) {
        if (strcmp(dxgdpy->extensions[i]->name, __DRI_CORE) == 0)
            dxgdpy->core = reinterpret_cast<const __DRIcoreExtension*>(dxgdpy->extensions[i]);
        else if (strcmp(dxgdpy->extensions[i]->name, __DRI_DXG) == 0)
            dxgdpy->dxg = reinterpret_cast<const __DRIdxgExtension*>(dxgdpy->extensions[i]);
    }

    if (!dxgdpy->core || !dxgdpy->dxg) {
        ErrorMessageF("dxg extensions not found\n");
        goto fail;
    }

    dxgdpy->dxcore_factory = reinterpret_cast<DXCore::IDXCoreAdapterFactory*>(
        dxgdpy->dxg->createDXCoreFactory(&dxgdpy->libdxcore));

    if (!dxgdpy->dxcore_factory) {
        ErrorMessageF("Failed to create DXCore factory");
        goto fail;
    }

    dxgdpy->destroyDisplay = dxg_destroy_display;
    dxgdpy->createScreen = dxg_create_screen;

    return dxgdpy;
fail:
    dxg_destroy_display(dxgdpy);
    return nullptr;
}

#endif
