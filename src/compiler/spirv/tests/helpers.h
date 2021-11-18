/*
 * Copyright © 2020 Valve Corporation
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
#ifndef SPIRV_TEST_HELPERS_H
#define SPIRV_TEST_HELPERS_H

#include <gtest/gtest.h>
#include "compiler/spirv/nir_spirv.h"
#include "compiler/nir/nir.h"

class spirv_test : public ::testing::Test {
protected:
   void *mem_ctx;
   spirv_to_nir_options spirv_options;
   nir_shader_compiler_options nir_options;
   nir_shader *shader;

   spirv_test()
   {
      glsl_type_singleton_init_or_ref();

      mem_ctx = ralloc_context(NULL);
      shader = NULL;

      memset(&spirv_options, 0, sizeof(spirv_options));
      spirv_options.environment = NIR_SPIRV_VULKAN;
      spirv_options.caps.vk_memory_model = true;
      spirv_options.caps.vk_memory_model_device_scope = true;
      spirv_options.ubo_addr_format = nir_address_format_32bit_index_offset;
      spirv_options.ssbo_addr_format = nir_address_format_32bit_index_offset;
      spirv_options.phys_ssbo_addr_format = nir_address_format_64bit_global;
      spirv_options.push_const_addr_format = nir_address_format_32bit_offset;
      spirv_options.shared_addr_format = nir_address_format_32bit_offset;

      memset(&nir_options, 0, sizeof(nir_options));
      nir_options.use_scoped_barrier = true;
   }

   ~spirv_test()
   {
      ralloc_free(mem_ctx);

      glsl_type_singleton_decref();
   }

   void get_nir(size_t num_words, const uint32_t *words)
   {
      shader = spirv_to_nir(words, num_words, NULL, 0,
                            MESA_SHADER_COMPUTE, "main", &spirv_options, &nir_options);
      ralloc_steal(mem_ctx, shader);
   }

   nir_intrinsic_instr *find_intrinsic(nir_intrinsic_op op, unsigned index=0)
   {
      nir_function_impl *impl = nir_shader_get_entrypoint(shader);
      nir_foreach_block(block, impl) {
         nir_foreach_instr(instr, block) {
            if (instr->type != nir_instr_type_intrinsic ||
                nir_instr_as_intrinsic(instr)->intrinsic != op)
               continue;
            if (index == 0)
               return nir_instr_as_intrinsic(instr);
            else
               index--;
         }
      }

      return NULL;
   }
};

#endif /* SPIRV_TEST_HELPERS_H */
