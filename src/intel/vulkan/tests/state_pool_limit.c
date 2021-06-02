/*
 * Copyright © 2018 Intel Corporation
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
   struct anv_state_pool state_pool;

   const uint32_t block_size = 4096;
   const uint32_t max_size = 64 * block_size;

   pthread_mutex_init(&device.mutex, NULL);
   anv_bo_cache_init(&device.bo_cache);
   VkResult result = anv_state_pool_init(&state_pool, &device, "test",
                                         4096 /* base_address */,
                                         0 /* start_offset*/,
                                         block_size,
                                         max_size);
   assert(result == VK_SUCCESS);

   struct anv_state *states = malloc(sizeof(*states) * 8192 / 64);
   /* Grab the entire pool */
   for (uint32_t i = 0; i < max_size / 64; i++) {
      result = anv_state_pool_alloc(&state_pool, 64, 64, &states[i]);
      assert(result == VK_SUCCESS);
   }

   /* Grab one more an fail */
   struct anv_state state;
   result = anv_state_pool_alloc(&state_pool, 64, 64, &state);
   assert(result == VK_ERROR_OUT_OF_DEVICE_MEMORY);

   for (uint32_t i = 0; i < 3; i++)
      anv_state_pool_free(&state_pool, states[i]);

   for (uint32_t i = 0; i < 3; i++) {
      result = anv_state_pool_alloc(&state_pool, 64, 64, &state);
      assert(result == VK_SUCCESS);
   }

   result = anv_state_pool_alloc(&state_pool, 64, 64, &state);
   assert(result == VK_ERROR_OUT_OF_DEVICE_MEMORY);

   anv_state_pool_finish(&state_pool);
}
