/*
 * Copyright © 2014 Broadcom
 * Copyright (C) 2012 Rob Clark <robclark@freedesktop.org>
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef U_DRM_H
#define U_DRM_H

#include <stdint.h>
#include <stdbool.h>
#include <xf86drm.h>

/* Does the u64 array contain the listed u64? */

static inline bool
util_array_contains_u64(uint64_t needle, const uint64_t *haystack, unsigned count)
{
   unsigned i;

   for (i = 0; i < count; i++) {
      if (haystack[i] == needle)
         return true;
   }

   return false;
}

/* Given a list of DRM modifiers and a desired modifier, returns whether the
 * modifier is found */

static inline bool
drm_find_modifier(uint64_t modifier, const uint64_t *modifiers, unsigned count)
{
   return util_array_contains_u64(modifier, modifiers, count);
}

static inline int util_set_buffer_label(int fd, int handle, char *label)
{
    struct drm_handle_label args = {};
    int ret;

    args.handle = handle;
    args.label = (uintptr_t) label;

    if (args.label)
        args.len = strlen(label) + 1;
    else
        args.len = 0;

    ret = drmIoctl(fd, DRM_IOCTL_HANDLE_SET_LABEL, &args);

    return ret;
}

static inline char* util_get_buffer_label(int fd, int handle)
{
    struct drm_handle_label args = {};
    int ret;

    args.handle = handle;

    ret = drmIoctl(fd, DRM_IOCTL_HANDLE_GET_LABEL, &args);
    if (ret != 0 || args.len == 0)
        return NULL;

    args.label = (uintptr_t) realloc(&(args.label), args.len);
    if (!args.label)
        return NULL;

    ret = drmIoctl(fd, DRM_IOCTL_HANDLE_GET_LABEL, &args);
    if (!ret)
        return NULL;

    return (char *)args.label;
}

#endif
