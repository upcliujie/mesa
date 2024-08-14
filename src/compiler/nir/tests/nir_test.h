/*
 * Copyright © 2023 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef NIR_TESTS_NIR_TEST_H
#define NIR_TESTS_NIR_TEST_H

#include <gtest/gtest.h>

#include "nir.h"
#include "nir_builder.h"

static inline void
delete_annotation(hash_entry *he)
{
   free(he->data);
}

class nir_test : public ::testing::Test {
public:
   nir_test(const char *name)
   {
      glsl_type_singleton_init_or_ref();

      _b = nir_builder_init_simple_shader(MESA_SHADER_COMPUTE, &options, "%s", name);
      b = &_b;
   }

   virtual ~nir_test()
   {
      if (HasFailure()) {
         printf("\nShader from the failed test:\n\n");
         nir_print_shader_annotated(b->shader, stdout, annotations);
      }

      _mesa_hash_table_destroy(annotations, delete_annotation);

      ralloc_free(b->shader);

      glsl_type_singleton_decref();
   }

   nir_shader_compiler_options options = {};
   nir_builder _b;
   nir_builder *b;

   hash_table *annotations = nullptr;
};

#endif
