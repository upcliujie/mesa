/*
 * Copyright © 2009-2018 Intel Corporation
 * Copyright (C) 2016 Advanced Micro Devices, Inc.
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

#include "hash_table.h"

#include <gtest/gtest.h>

static void entry_free(struct hash_entry *entry)
{
   free((void *)entry->key);
}

static uint32_t
key_value(const void *key)
{
   return *(const uint32_t *)key;
}

static bool
uint32_t_key_equals(const void *a, const void *b)
{
   return key_value(a) == key_value(b);
}

static bool
uint32_t_key_is_even(struct hash_entry *entry)
{
   return (key_value(entry->key) & 1) == 0;
}

// Return collisions, so we can test the deletion behavior for chained
// objects.
static uint32_t
badhash(const void *key)
{
   (void) key;
   return 1;
}

TEST(HashTable, Collision)
{
   struct hash_table *ht;
   const char *str1 = strdup("test1");
   const char *str2 = strdup("test2");
   const char *str3 = strdup("test3");
   struct hash_entry *entry1, *entry2;
   uint32_t bad_hash = 5;
   int i;

   ht = _mesa_hash_table_create(NULL, NULL, _mesa_key_string_equal);

   // Insert some items.  Inserting 3 items forces a rehash and the new table
   // size is big enough that we don't get rehashes later.
   _mesa_hash_table_insert_pre_hashed(ht, bad_hash, str1, NULL);
   _mesa_hash_table_insert_pre_hashed(ht, bad_hash, str2, NULL);
   _mesa_hash_table_insert_pre_hashed(ht, bad_hash, str3, NULL);

   entry1 = _mesa_hash_table_search_pre_hashed(ht, bad_hash, str1);
   ASSERT_EQ(entry1->key, str1);

   entry2 = _mesa_hash_table_search_pre_hashed(ht, bad_hash, str2);
   ASSERT_EQ(entry2->key, str2);

   // Check that we can still find #1 after inserting #2.
   entry1 = _mesa_hash_table_search_pre_hashed(ht, bad_hash, str1);
   ASSERT_EQ(entry1->key, str1);

   // Remove the collided entry and look again.
   _mesa_hash_table_remove(ht, entry1);
   entry2 = _mesa_hash_table_search_pre_hashed(ht, bad_hash, str2);
   ASSERT_EQ(entry2->key, str2);

   // Try inserting #2 again and make sure it gets overwritten.
   _mesa_hash_table_insert_pre_hashed(ht, bad_hash, str2, NULL);
   entry2 = _mesa_hash_table_search_pre_hashed(ht, bad_hash, str2);
   hash_table_foreach(ht, search_entry) {
      ASSERT_TRUE(search_entry == entry2 || search_entry->key != str2);
   }

   // Put str1 back, then spam junk into the table to force a
   // resize and make sure we can still find them both.
   _mesa_hash_table_insert_pre_hashed(ht, bad_hash, str1, NULL);
   for (i = 0; i < 100; i++) {
      char *key = (char *)malloc(10);
      sprintf(key, "spam%d", i);
      _mesa_hash_table_insert_pre_hashed(ht, _mesa_hash_string(key), key, NULL);
   }
   entry1 = _mesa_hash_table_search_pre_hashed(ht, bad_hash, str1);
   ASSERT_EQ(entry1->key, str1);
   entry2 = _mesa_hash_table_search_pre_hashed(ht, bad_hash, str2);
   ASSERT_EQ(entry2->key, str2);

   _mesa_hash_table_destroy(ht, entry_free);
}

TEST(HashTable, DeleteAndLookup)
{
   struct hash_table *ht;
   const char *str1 = "test1";
   const char *str2 = "test2";
   struct hash_entry *entry;

   ht = _mesa_hash_table_create(NULL, badhash, _mesa_key_string_equal);

   _mesa_hash_table_insert(ht, str1, NULL);
   _mesa_hash_table_insert(ht, str2, NULL);

   entry = _mesa_hash_table_search(ht, str2);
   ASSERT_STREQ((const char *)entry->key, str2);

   entry = _mesa_hash_table_search(ht, str1);
   ASSERT_STREQ((const char *)entry->key, str1);

   _mesa_hash_table_remove(ht, entry);

   entry = _mesa_hash_table_search(ht, str1);
   ASSERT_FALSE(entry);

   entry = _mesa_hash_table_search(ht, str2);
   ASSERT_STREQ((const char *)entry->key, str2);

   _mesa_hash_table_destroy(ht, NULL);
}

TEST(HashTable, DeleteManagement)
{
   struct hash_table *ht;
   struct hash_entry *entry;
   uint32_t keys[10000];
   uint32_t i;

   ht = _mesa_hash_table_create(NULL, key_value, uint32_t_key_equals);

   for (i = 0; i < ARRAY_SIZE(keys); i++) {
      keys[i] = i;

      _mesa_hash_table_insert(ht, keys + i, NULL);

      if (i >= 100) {
         uint32_t delete_value = i - 100;
         entry = _mesa_hash_table_search(ht, &delete_value);
         _mesa_hash_table_remove(ht, entry);
      }
   }

   // Make sure that all our entries were present at the end.
   for (i = ARRAY_SIZE(keys) - 100; i < ARRAY_SIZE(keys); i++) {
      entry = _mesa_hash_table_search(ht, keys + i);
      ASSERT_TRUE(entry);
      ASSERT_EQ(key_value(entry->key), i);
   }

   // Make sure that no extra entries got in.
   for (entry = _mesa_hash_table_next_entry(ht, NULL);
        entry != NULL;
        entry = _mesa_hash_table_next_entry(ht, entry)) {
      ASSERT_GE(key_value(entry->key), ARRAY_SIZE(keys) - 100);
      ASSERT_LE(key_value(entry->key), ARRAY_SIZE(keys));
   }
   ASSERT_EQ(ht->entries, 100);

   _mesa_hash_table_destroy(ht, NULL);
}

struct DestroyCallbackData {
   const char *str1;
   const char *str2;
   bool delete_str1;
   bool delete_str2;
};

// DestroyCallback for HashTable doesn't have a data pointer, so we use a
// global to keep track.
static DestroyCallbackData g_DestroyCallbackData = {
   "test1",
   "test2",
   false,
   false,
};

static void
delete_callback(struct hash_entry *entry)
{
   if (strcmp((const char *)entry->key, g_DestroyCallbackData.str1) == 0)
      g_DestroyCallbackData.delete_str1 = true;
   else if (strcmp((const char *)entry->key, g_DestroyCallbackData.str2) == 0)
      g_DestroyCallbackData.delete_str2 = true;
   else
      abort();
}

TEST(HashTable, DestroyCallback)
{
   struct hash_table *ht;

   ht = _mesa_hash_table_create(NULL, _mesa_hash_string,
                                _mesa_key_string_equal);

   _mesa_hash_table_insert(ht, g_DestroyCallbackData.str1, NULL);
   _mesa_hash_table_insert(ht, g_DestroyCallbackData.str2, NULL);

   g_DestroyCallbackData.delete_str1 = false;
   g_DestroyCallbackData.delete_str2 = false;

   _mesa_hash_table_destroy(ht, delete_callback);

   ASSERT_TRUE(g_DestroyCallbackData.delete_str1);
   ASSERT_TRUE(g_DestroyCallbackData.delete_str2);
}

TEST(HashTable, InsertAndLookup)
{
   struct hash_table *ht;
   const char *str1 = "test1";
   const char *str2 = "test2";
   struct hash_entry *entry;

   ht = _mesa_hash_table_create(NULL, _mesa_hash_string,
                                _mesa_key_string_equal);

   _mesa_hash_table_insert(ht, str1, NULL);
   _mesa_hash_table_insert(ht, str2, NULL);

   entry = _mesa_hash_table_search(ht, str1);
   ASSERT_STREQ((const char *)entry->key, str1);

   entry = _mesa_hash_table_search(ht, str2);
   ASSERT_STREQ((const char *)entry->key, str2);

   _mesa_hash_table_destroy(ht, NULL);
}

TEST(HashTable, InsertMany)
{
   struct hash_table *ht;
   struct hash_entry *entry;
   uint32_t keys[10000];
   uint32_t i;

   ht = _mesa_hash_table_create(NULL, key_value, uint32_t_key_equals);

   for (i = 0; i < ARRAY_SIZE(keys); i++) {
      keys[i] = i;

      _mesa_hash_table_insert(ht, keys + i, NULL);
   }

   for (i = 0; i < ARRAY_SIZE(keys); i++) {
      entry = _mesa_hash_table_search(ht, keys + i);
      ASSERT_TRUE(entry);
      ASSERT_EQ(key_value(entry->key), i);
   }
   ASSERT_EQ(ht->entries, ARRAY_SIZE(keys));

   _mesa_hash_table_destroy(ht, NULL);
}

TEST(HashTable, NullDestroy)
{
   _mesa_hash_table_destroy(NULL, NULL);
}

TEST(HashTable, RandomEntry)
{
   struct hash_table *ht;
   struct hash_entry *entry;
   uint32_t keys[10000];
   uint32_t i, random_value = 0;

   ht = _mesa_hash_table_create(NULL, key_value, uint32_t_key_equals);

   for (i = 0; i < ARRAY_SIZE(keys); i++) {
      keys[i] = i;

      _mesa_hash_table_insert(ht, keys + i, NULL);
   }

   // Test the no-predicate case.
   entry = _mesa_hash_table_random_entry(ht, NULL);
   ASSERT_TRUE(entry);

   // Check that we're getting different entries and that the predicate works.
   for (i = 0; i < 100; i++) {
      entry = _mesa_hash_table_random_entry(ht, uint32_t_key_is_even);
      ASSERT_TRUE(entry);
      ASSERT_EQ(key_value(entry->key) & 1, 0);
      if (i == 0 || key_value(entry->key) != random_value)
         break;
      random_value = key_value(entry->key);
   }
   ASSERT_NE(i, 100);

   _mesa_hash_table_destroy(ht, NULL);
}

TEST(HashTable, RemoveKey)
{
   struct hash_table *ht;
   const char *str1 = "test1";
   const char *str2 = "test2";
   struct hash_entry *entry;

   ht = _mesa_hash_table_create(NULL, _mesa_hash_string, _mesa_key_string_equal);

   _mesa_hash_table_insert(ht, str1, NULL);
   _mesa_hash_table_insert(ht, str2, NULL);

   entry = _mesa_hash_table_search(ht, str2);
   ASSERT_STREQ((const char *)entry->key, str2);

   entry = _mesa_hash_table_search(ht, str1);
   ASSERT_STREQ((const char *)entry->key, str1);

   _mesa_hash_table_remove_key(ht, str1);

   entry = _mesa_hash_table_search(ht, str1);
   ASSERT_FALSE(entry);

   entry = _mesa_hash_table_search(ht, str2);
   ASSERT_STREQ((const char *)entry->key, str2);

   _mesa_hash_table_destroy(ht, NULL);
}

TEST(HashTable, RemoveNull)
{
   struct hash_table *ht;

   ht = _mesa_hash_table_create(NULL, NULL, _mesa_key_string_equal);

   _mesa_hash_table_remove(ht, NULL);

   _mesa_hash_table_destroy(ht, NULL);
}

TEST(HashTable, Replacement)
{
   struct hash_table *ht;
   char *str1 = strdup("test1");
   char *str2 = strdup("test1");
   struct hash_entry *entry;

   ASSERT_NE(str1, str2);

   ht = _mesa_hash_table_create(NULL, _mesa_hash_string,
                                _mesa_key_string_equal);

   _mesa_hash_table_insert(ht, str1, str1);
   _mesa_hash_table_insert(ht, str2, str2);

   entry = _mesa_hash_table_search(ht, str1);
   ASSERT_TRUE(entry);
   ASSERT_EQ(entry->data, str2);

   _mesa_hash_table_remove(ht, entry);

   entry = _mesa_hash_table_search(ht, str1);
   ASSERT_FALSE(entry);

   _mesa_hash_table_destroy(ht, NULL);
   free(str1);
   free(str2);
}

static void *clear_make_key(uint32_t i)
{
   return (void *)(uintptr_t)(1 + i);
}

static uint32_t clear_key_id(const void *key)
{
   return (uintptr_t)key - 1;
}

static uint32_t clear_key_hash(const void *key)
{
   return (uintptr_t)key;
}

static bool clear_key_equal(const void *a, const void *b)
{
   return a == b;
}

static void clear_delete_function(struct hash_entry *entry)
{
   bool *deleted = (bool *)entry->data;
   assert(!*deleted);
   *deleted = true;
}

TEST(HashTable, Clear)
{
   struct hash_table *ht;
   bool flags[1000];
   uint32_t i;

   ht = _mesa_hash_table_create(NULL, clear_key_hash, clear_key_equal);

   for (i = 0; i < ARRAY_SIZE(flags); ++i) {
      flags[i] = false;
      _mesa_hash_table_insert(ht, clear_make_key(i), &flags[i]);
   }

   _mesa_hash_table_clear(ht, clear_delete_function);
   ASSERT_FALSE(_mesa_hash_table_next_entry(ht, NULL));

   // Check that delete_function was called and that repopulating the table
   // works.
   for (i = 0; i < ARRAY_SIZE(flags); ++i) {
      ASSERT_TRUE(flags[i]);
      flags[i] = false;
      _mesa_hash_table_insert(ht, clear_make_key(i), &flags[i]);
   }

   // Check that exactly the right set of entries is in the table.
   for (i = 0; i < ARRAY_SIZE(flags); ++i) {
      struct hash_entry *entry = _mesa_hash_table_search(ht, clear_make_key(i));
      ASSERT_TRUE(entry);
   }

   hash_table_foreach(ht, entry) {
      ASSERT_LT(clear_key_id(entry->key), ARRAY_SIZE(flags));
   }
   _mesa_hash_table_clear(ht, NULL);
   ASSERT_EQ(ht->entries, 0);
   ASSERT_EQ(ht->deleted_entries, 0);
   hash_table_foreach(ht, entry) {
      FAIL();
   }

   for (i = 0; i < ARRAY_SIZE(flags); ++i) {
      flags[i] = false;
      _mesa_hash_table_insert(ht, clear_make_key(i), &flags[i]);
   }
   hash_table_foreach_remove(ht, entry) {
      ASSERT_LT(clear_key_id(entry->key), ARRAY_SIZE(flags));
   }
   ASSERT_EQ(ht->entries, 0);
   ASSERT_EQ(ht->deleted_entries, 0);

   _mesa_hash_table_destroy(ht, NULL);
}
