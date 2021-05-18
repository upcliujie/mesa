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

/* Kind of like a hash table, but the key is always a 32-bit number and
 * the element is a pointer.
 *
 * This is much faster than the hash_table structure and lookups are always
 * thread-safe without locking. Other operations do need locking. The user
 * should lock manually.
 */

#ifndef IDTABLE_H
#define IDTABLE_H

#include "util/u_idalloc.h"
#include "util/sparse_array.h"

struct util_idtable {
   struct util_sparse_array table;
   struct util_idalloc ids;
};

void
util_idtable_init(struct util_idtable *table);

void
util_idtable_deinit(struct util_idtable *table);

bool
util_idtable_initialized(struct util_idtable *table);

void
util_idtable_insert(struct util_idtable *table, uint32_t key, void *data);

void
util_idtable_remove(struct util_idtable *table, uint32_t key);

void
util_idtable_remove_all(struct util_idtable *table,
                        void (*destroy)(void *data, void *user_data),
                        void *user_data);

uint32_t
util_idtable_alloc_key_range(struct util_idtable *table, uint32_t num_keys);

void
util_idtable_alloc_keys(struct util_idtable *table, uint32_t *keys,
                        uint32_t num_keys);


static inline void *
util_idtable_lookup(struct util_idtable *table, uint32_t key)
{
   return *(void**)util_sparse_array_get(&table->table, key);
}

#define util_idtable_foreach(idtable, id, obj) \
   util_idalloc_foreach(&(idtable)->ids, id) \
      for (void *obj; id; id = 0) /* Skip id == 0 and obj == NULL. */ \
         if ((obj = util_idtable_lookup((idtable), id)))

#endif /* IDTABLE_H */
