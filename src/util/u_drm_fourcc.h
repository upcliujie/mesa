/* Copyright (C) 2022 Intel Corporation
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * Attempt to locate a modifier by name.
 *
 * If the modifier name is unknown, then DRM_FORMAT_MOD_INVALID will be
 * returned.
 *
 * @param name The name of the modifier to search for
 */
uint64_t
u_get_drm_fourcc_modifier_by_name(const char *name);

/**
 * Attempt to convert a string into a modifier value.
 *
 * Similar to u_get_drm_fourcc_modifier_by_name(), but the string can also be
 * a hex number.
 *
 * If the modifier name is unknown and the string it not a hex
 * number, then DRM_FORMAT_MOD_INVALID will be returned.
 *
 * @param str The string to convert into a modifier value
 */
uint64_t
u_get_drm_fourcc_modifier_from_string(const char *str);

/**
 * Try to find a know name for the specified modifier.
 *
 * If the modifier is unknown, then NULL will be returned.
 *
 * @param mod The modifier to search for
 */
const char *
u_get_drm_fourcc_modifier_name(uint64_t mod);

/**
 * Retrieve a drm_fourcc modifier at the specified index (`n`). Start with n=0
 * and increment until false is returned from the function. The returned items
 * will be in order by name.
 *
 * Multiple items may have the same modifier value, but the names will all be
 * unique.
 *
 * If the index, n, exists, true is returned and `name` and `mod` will be
 * updated. If index, n, doesn't exist, then false will be returned.
 *
 * @param n The index item to retrieve
 * @param name Pointer to string pointer to return name
 * @param mod Pointer to mod to return the modifier number
 */
bool
u_get_drm_fourcc_modifier_n_by_name(int n, const char **name, uint64_t *mod);

/**
 * Retrieve a drm_fourcc modifier at the specified index (`n`). Start with n=0
 * and increment until false is returned from the function. The returned items
 * will be in order by modifier.
 *
 * Multiple items may have the same modifier value, but the names will all be
 * unique.
 *
 * If the index, n, exists, true is returned and `name` and `mod` will be
 * updated. If index, n, doesn't exist, then false will be returned.
 *
 * @param n The index item to retrieve
 * @param name Pointer to string pointer to return name
 * @param mod Pointer to mod to return the modifier number
 */
bool
u_get_drm_fourcc_modifier_n_by_mod(int n, const char **name, uint64_t *mod);

/**
 * Iterates over all the known modifiers, returned in ascending order of
 * names.
 *
 * Multiple items may have the same modifier value, but the names will all be
 * unique.
 *
 * @param __name A `const char *` variable to store the modifier name pointer
 * @param __mod A uint64_t variable which will be updated with the modifier
 *              value
 */
#define foreach_drm_fourcc_modifier_by_name(__name, __mod)              \
   for (int __i = 0;                                                    \
        u_get_drm_fourcc_modifier_n_by_name(__i, &__name, &__mod);      \
        __i++)

/**
 * Iterates over all the known modifiers, returned in ascending order of
 * modifier values.
 *
 * Multiple items may have the same modifier value, but the names will all be
 * unique.
 *
 * @param __name A `const char *` variable to store the modifier name pointer
 * @param __mod A uint64_t variable which will be updated with the modifier
 *              value
 */
#define foreach_drm_fourcc_modifier_by_mod(__name, __mod)               \
   for (int __i = 0;                                                    \
        u_get_drm_fourcc_modifier_n_by_mod(__i, &__name, &__mod);       \
        __i++)

#ifdef __cplusplus
}
#endif
