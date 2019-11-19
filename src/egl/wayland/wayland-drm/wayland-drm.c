/*
 * Copyright © 2011 Kristian Høgsberg
 * Copyright © 2011 Benjamin Franzke
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Kristian Høgsberg <krh@bitplanet.net>
 *    Benjamin Franzke <benjaminfranzke@googlemail.com>
 */

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>

#include <wayland-server.h>
#include "wayland-drm.h"
#include "wayland-drm-server-protocol.h"

#define MIN(x,y) (((x)<(y))?(x):(y))

static void
drm_create_buffer(struct wl_client *client, struct wl_resource *resource,
		  uint32_t id, uint32_t name, int32_t width, int32_t height,
		  uint32_t stride, uint32_t format)
{
        wl_resource_post_error(resource,
                               WL_DRM_ERROR_INVALID_FORMAT,
                               "invalid format");
}

static void
drm_create_planar_buffer(struct wl_client *client,
                         struct wl_resource *resource,
                         uint32_t id, uint32_t name,
                         int32_t width, int32_t height, uint32_t format,
                         int32_t offset0, int32_t stride0,
                         int32_t offset1, int32_t stride1,
                         int32_t offset2, int32_t stride2)
{
        wl_resource_post_error(resource,
                               WL_DRM_ERROR_INVALID_FORMAT,
                               "invalid format");
}

static void
drm_create_prime_buffer(struct wl_client *client,
                        struct wl_resource *resource,
                        uint32_t id, int fd,
                        int32_t width, int32_t height, uint32_t format,
                        int32_t offset0, int32_t stride0,
                        int32_t offset1, int32_t stride1,
                        int32_t offset2, int32_t stride2)
{
        wl_resource_post_error(resource,
                               WL_DRM_ERROR_INVALID_FORMAT,
                               "invalid format");
        close(fd);
}

static void
drm_authenticate(struct wl_client *client,
		 struct wl_resource *resource, uint32_t id)
{
	struct wl_drm *drm = wl_resource_get_user_data(resource);

	if (drm->callbacks.authenticate(drm->user_data, id) < 0)
		wl_resource_post_error(resource,
				       WL_DRM_ERROR_AUTHENTICATE_FAIL,
				       "authenicate failed");
	else
		wl_resource_post_event(resource, WL_DRM_AUTHENTICATED);
}

static const struct wl_drm_interface drm_interface = {
	drm_authenticate,
	drm_create_buffer,
        drm_create_planar_buffer,
        drm_create_prime_buffer
};

static void
bind_drm(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_drm *drm = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &wl_drm_interface,
				      MIN(version, 2), id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &drm_interface, data, NULL);

	wl_resource_post_event(resource, WL_DRM_DEVICE, drm->device_name);

        if (version >= 2)
           wl_resource_post_event(resource, WL_DRM_CAPABILITIES, 0);
}

struct wl_drm *
wayland_drm_init(struct wl_display *display, char *device_name,
                 const struct wayland_drm_callbacks *callbacks, void *user_data)
{
	struct wl_drm *drm;

	drm = malloc(sizeof *drm);
	if (!drm)
		return NULL;

	drm->display = display;
	drm->device_name = strdup(device_name);
	drm->callbacks = *callbacks;
	drm->user_data = user_data;

	drm->wl_drm_global =
		wl_global_create(display, &wl_drm_interface, 2,
				 drm, bind_drm);

	return drm;
}

void
wayland_drm_uninit(struct wl_drm *drm)
{
	free(drm->device_name);

	wl_global_destroy(drm->wl_drm_global);

	free(drm);
}
