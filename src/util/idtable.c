/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2006  Brian Paul   All Rights Reserved.
 * Copyright (C) 2021 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "util/idtable.h"

void
util_idtable_init(struct util_idtable *table)
{
   /* TODO: what is node_size of sparse_array? */
   util_sparse_array_init(&table->table, sizeof(void*), 512);

   util_idalloc_init(&table->ids, 32);
   ASSERTED uint32_t reserved0 = util_idalloc_alloc(&table->ids);
   assert(reserved0 == 0);
}

void
util_idtable_deinit(struct util_idtable *table)
{
   util_sparse_array_finish(&table->table);
   util_idalloc_fini(&table->ids);
}

bool
util_idtable_initialized(struct util_idtable *table)
{
   return util_idalloc_initialized(&table->ids);
}

void
util_idtable_insert(struct util_idtable *table, uint32_t key, void *data)
{
   assert(key);

   util_idalloc_reserve(&table->ids, key);
   *(void**)util_sparse_array_get(&table->table, key) = data;
}

void
util_idtable_remove(struct util_idtable *table, uint32_t key)
{
   assert(key);

   if (!util_idalloc_exists(&table->ids, key))
      return;

   *(void**)util_sparse_array_get(&table->table, key) = NULL;
   util_idalloc_free(&table->ids, key);
}

/**
 * Remove all entries from the table.
 * Invoke the given destroy function for each table entry.
 *
 * \param table  the ID table to iterate
 * \param destroy  the destroy function
 * \param user_data  arbitrary pointer to pass along to the destroy
 *                  (this is typically a struct gl_context pointer)
 */
void
util_idtable_remove_all(struct util_idtable *table,
                        void (*destroy)(void *data, void *user_data),
                        void *user_data)
{
   assert(destroy);

   util_idtable_foreach(table, id, obj) {
      destroy(obj, user_data);
      *(void**)util_sparse_array_get(&table->table, id) = NULL;
   }

   util_idalloc_clear(&table->ids);
}

/**
 * Allocate a block of adjacent unused keys.
 *
 * \return The first key of the block.
 */
uint32_t
util_idtable_alloc_key_range(struct util_idtable *table, uint32_t num_keys)
{
   return util_idalloc_alloc_range(&table->ids, num_keys);
}

void
util_idtable_alloc_keys(struct util_idtable *table, uint32_t *keys,
                        uint32_t num_keys)
{
   for (int i = 0; i < num_keys; i++)
      keys[i] = util_idalloc_alloc(&table->ids);
}
