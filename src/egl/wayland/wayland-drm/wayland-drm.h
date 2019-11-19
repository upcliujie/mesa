#ifndef WAYLAND_DRM_H
#define WAYLAND_DRM_H

#include <wayland-server.h>

struct wl_display;

struct wayland_drm_callbacks {
	int (*authenticate)(void *user_data, uint32_t id);
};


struct wl_drm {
	struct wl_display *display;
	struct wl_global *wl_drm_global;

	void *user_data;
	char *device_name;

	struct wayland_drm_callbacks callbacks;
};

struct wl_drm *
wayland_drm_init(struct wl_display *display, char *device_name,
		 const struct wayland_drm_callbacks *callbacks, void *user_data);

void
wayland_drm_uninit(struct wl_drm *drm);

#endif
