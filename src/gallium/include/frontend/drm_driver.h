
#ifndef _DRM_DRIVER_H_
#define _DRM_DRIVER_H_

#include "pipe/p_compiler.h"

#include "winsys_handle.h"

struct pipe_screen;
struct pipe_screen_config;
struct pipe_context;
struct pipe_resource;

struct drm_driver_descriptor
{
   /**
    * Identifying prefix/suffix of the binary, used by the pipe-loader.
    */
   const char *driver_name;

   /**
    * Optional pointer to the array of driOptionDescription describing
    * driver-specific driconf options.
    */
   const struct driOptionDescription *driconf;

   /* Number of entries in the driconf array. */
   unsigned driconf_count;

   /**
    * Create a pipe srcreen.
    *
    * This function does any wrapping of the screen.
    * For example wrapping trace or rbug debugging drivers around it.
    */
   struct pipe_screen* (*create_screen)(int drm_fd,
                                        const struct pipe_screen_config *config);

   /**
    * Get the device name (ie. equiv to GL_RENDERER string).
    *
    * This function returns the device name, to differentiate different
    * GPUs supported by a single driver.  Only required if the driver
    * utilizes driconf options specific to a particular device.
    *
    * Note that the return is 'const char *', the caller is not expected
    * to free().
    */
   const char * (*device_name)(int drm_fd);
};

extern const struct drm_driver_descriptor driver_descriptor;

#endif
