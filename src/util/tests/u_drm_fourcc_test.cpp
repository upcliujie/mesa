/*
 * Copyright (c) 2022 Intel Corporation
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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "util/hash_table.h"
#include "util/u_drm_fourcc.h"

#include <gtest/gtest.h>

TEST(drm_fourcc_modifiers, get_mod_by_names)
{
   const char *name, *prev_name = NULL;
   uint64_t mod;
   foreach_drm_fourcc_modifier_by_name(name, mod) {
      if (prev_name != NULL) {
         EXPECT_LT(strcmp(prev_name, name), 0);
      }
      EXPECT_EQ(u_get_drm_fourcc_modifier_by_name(name), mod);
      const char *mod_to_name = u_get_drm_fourcc_modifier_name(mod);
      EXPECT_NE(mod_to_name, (const char *)NULL);

      /**
       * The result of u_get_drm_fourcc_modifier_name() might not match the
       * original name, but if we take that name and call
       * u_get_drm_fourcc_modifier_by_name(), then we should get back the same
       * modifier number.
       */
      if (mod_to_name != NULL) {
         EXPECT_EQ(u_get_drm_fourcc_modifier_by_name(mod_to_name), mod);
      }
      prev_name = name;
   }
}

TEST(drm_fourcc_modifiers, get_mod_from_str)
{
   const char *name;
   uint64_t mod;
   foreach_drm_fourcc_modifier_by_name(name, mod) {
      EXPECT_EQ(u_get_drm_fourcc_modifier_from_string(name), mod);
   }

   /* Test hex strings with a leading 0x */
#define HEX_EXPECT_EQ(h) \
   EXPECT_EQ(u_get_drm_fourcc_modifier_from_string(#h), h)
   HEX_EXPECT_EQ(0xffffffffffffffff);
   HEX_EXPECT_EQ(0x0000ffffffff0000);
   HEX_EXPECT_EQ(0x0000000100000000);
   HEX_EXPECT_EQ(0x00000000ffffffff);
   HEX_EXPECT_EQ(0xffffffff);
   HEX_EXPECT_EQ(0x1);
   HEX_EXPECT_EQ(0x0);
#undef HEX_EXPECT_EQ

   /* Test hex strings without a leading 0x */
#define HEX_EXPECT_EQ(h) \
   EXPECT_EQ(u_get_drm_fourcc_modifier_from_string(#h), 0x ## h)
   HEX_EXPECT_EQ(ffffffffffffffff);
   HEX_EXPECT_EQ(0000ffffffff0000);
   HEX_EXPECT_EQ(0000000100000000);
   HEX_EXPECT_EQ(00000000ffffffff);
   HEX_EXPECT_EQ(ffffffff);
   HEX_EXPECT_EQ(1);
   HEX_EXPECT_EQ(0);
#undef HEX_EXPECT_EQ
}

TEST(drm_fourcc_modifiers, get_mod_by_mod)
{
   const char *name;
   uint64_t mod, prev_mod = 0;
   foreach_drm_fourcc_modifier_by_mod(name, mod) {
      EXPECT_LE(prev_mod, mod);
      EXPECT_EQ(u_get_drm_fourcc_modifier_by_name(name), mod);
      const char *mod_to_name = u_get_drm_fourcc_modifier_name(mod);
      EXPECT_NE(mod_to_name, (const char *)NULL);

      /**
       * The result of u_get_drm_fourcc_modifier_name() might not match the
       * original name, but if we take that name and call
       * u_get_drm_fourcc_modifier_by_name(), then we should get back the same
       * modifier number.
       */
      if (mod_to_name != NULL) {
         EXPECT_EQ(u_get_drm_fourcc_modifier_by_name(mod_to_name), mod);
      }
      prev_mod = mod;
   }
}

static void
free_hash_table_entry_data(struct hash_entry *entry)
{
   free(entry->data);
}

TEST(drm_fourcc_modifiers, compare_name_and_mod_sets)
{
   struct hash_table *ht;
   const char *name;
   uint64_t mod;

   ht = _mesa_hash_table_create(NULL, _mesa_hash_string,
                                _mesa_key_string_equal);

   foreach_drm_fourcc_modifier_by_name(name, mod) {
      uint64_t *mod_data = (uint64_t *)malloc(sizeof(uint64_t));
      *mod_data = mod;
      _mesa_hash_table_insert(ht, name, mod_data);
   }
   EXPECT_GT(_mesa_hash_table_num_entries(ht), 0);

   foreach_drm_fourcc_modifier_by_mod(name, mod) {
      struct hash_entry *entry;
      entry = _mesa_hash_table_search(ht, name);
      EXPECT_NE(entry, (struct hash_entry *)NULL);
      if (entry != NULL) {
         uint64_t *mod_data = (uint64_t *)entry->data;
         EXPECT_EQ(*mod_data, mod);
         _mesa_hash_table_remove(ht, entry);
         free(mod_data);
      }
   }

   EXPECT_EQ(_mesa_hash_table_num_entries(ht), 0);
   _mesa_hash_table_destroy(ht, free_hash_table_entry_data);
}
