/*
 * Copyright © 2021 Intel Corporation
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

#include "anv_private.h"
#include "test_common.h"

int main(void)
{
   struct anv_physical_device physical_device = {
      .use_softpin = true,
   };
   struct anv_device device = {
      .physical = &physical_device,
   };
   struct anv_block_pool pool;

   /* Create a pool with initial size smaller than the block allocated, so
    * that it must grow in the first allocation.
    */
   const uint32_t block_size = 16 * 1024;

   pthread_mutex_init(&device.mutex, NULL);
   anv_bo_cache_init(&device.bo_cache);
   anv_block_pool_init(&pool, &device, "test", 4096, 10 * block_size, block_size);

   uint32_t padding;
   int32_t offset;

   for (uint32_t i = 0; i < 10; i++) {
      VkResult result = anv_block_pool_alloc(&pool, block_size, &offset, &padding);
      assert(result == VK_SUCCESS);
   }

   VkResult result = anv_block_pool_alloc(&pool, block_size, &offset, &padding);
   assert(result == VK_ERROR_OUT_OF_DEVICE_MEMORY);

   anv_block_pool_finish(&pool);
}
