/**
 * \file hash.h
 * Generic hash table.
 */

/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2006  Brian Paul   All Rights Reserved.
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


#ifndef HASH_H
#define HASH_H


#include "util/idtable.h"

/**
 * The hash table data structure for OpenGL object IDs.
 */
struct _mesa_HashTable {
   struct util_idtable table;
   simple_mtx_t Mutex;                   /**< mutual exclusion lock */
};

extern struct _mesa_HashTable *_mesa_NewHashTable(void);

extern void _mesa_DeleteHashTable(struct _mesa_HashTable *table);

extern void _mesa_HashInsert(struct _mesa_HashTable *table, uint32_t key, void *data);

extern void _mesa_HashRemove(struct _mesa_HashTable *table, uint32_t key);

/**
 * Lock the hash table mutex.
 *
 * This function should be used when multiple objects need
 * to be looked up in the hash table, to avoid having to lock
 * and unlock the mutex each time.
 *
 * \param table the hash table.
 */
static inline void
_mesa_HashLockMutex(struct _mesa_HashTable *table)
{
   assert(table);
   simple_mtx_lock(&table->Mutex);
}


/**
 * Unlock the hash table mutex.
 *
 * \param table the hash table.
 */
static inline void
_mesa_HashUnlockMutex(struct _mesa_HashTable *table)
{
   assert(table);
   simple_mtx_unlock(&table->Mutex);
}

extern void *_mesa_HashLookupLocked(struct _mesa_HashTable *table, uint32_t key);
extern void *_mesa_HashLookup(struct _mesa_HashTable *table, uint32_t key);
extern void _mesa_HashInsertLocked(struct _mesa_HashTable *table,
                                   uint32_t key, void *data);

extern void _mesa_HashRemoveLocked(struct _mesa_HashTable *table, uint32_t key);

extern void
_mesa_HashDeleteAll(struct _mesa_HashTable *table,
                    void (*callback)(void *data, void *userData),
                    void *userData);

extern void
_mesa_HashWalk(struct _mesa_HashTable *table,
               void (*callback)(void *data, void *userData),
               void *userData);

extern void
_mesa_HashWalkLocked(struct _mesa_HashTable *table,
                     void (*callback)(void *data, void *userData),
                     void *userData);

extern uint32_t _mesa_HashFindFreeKeyBlock(struct _mesa_HashTable *table, uint32_t numKeys);

extern void
_mesa_HashFindFreeKeys(struct _mesa_HashTable *table, uint32_t* keys, uint32_t numKeys);

extern void _mesa_test_hash_functions(void);

static inline void
_mesa_HashWalkMaybeLocked(struct _mesa_HashTable *table,
                            void (*callback)(void *data, void *userData),
                            void *userData, bool locked)
{
   if (locked)
      _mesa_HashWalkLocked(table, callback, userData);
   else
      _mesa_HashWalk(table, callback, userData);
}

static inline struct gl_buffer_object *
_mesa_HashLookupMaybeLocked(struct _mesa_HashTable *table, uint32_t key,
                            bool locked)
{
   if (locked)
      return _mesa_HashLookupLocked(table, key);
   else
      return _mesa_HashLookup(table, key);
}

static inline void
_mesa_HashInsertMaybeLocked(struct _mesa_HashTable *table,
                            uint32_t key, void *data, bool locked)
{
   if (locked)
      _mesa_HashInsertLocked(table, key, data);
   else
      _mesa_HashInsert(table, key, data);
}

static inline void
_mesa_HashLockMaybeLocked(struct _mesa_HashTable *table, bool locked)
{
   if (!locked)
      _mesa_HashLockMutex(table);
}

static inline void
_mesa_HashUnlockMaybeLocked(struct _mesa_HashTable *table, bool locked)
{
   if (!locked)
      _mesa_HashUnlockMutex(table);
}

#endif
