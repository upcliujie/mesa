/*
 * Copyright © Microsoft Corporation
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef MESA_CLC_H
#define MESA_CLC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct clc_named_value {
   const char *name;
   const char *value;
};

struct clc_compile_args {
   const struct clc_named_value *headers;
   unsigned num_headers;
   struct clc_named_value source;
   const char * const *args;
   unsigned num_args;
};

struct clc_linker_args {
   const struct clc_object * const *in_objs;
   unsigned num_in_objs;
   unsigned create_library;
};

typedef void (*clc_msg_callback)(void *priv, const char *msg);

struct clc_logger {
   void *priv;
   clc_msg_callback error;
   clc_msg_callback warning;
};

struct spirv_binary {
   uint32_t *data;
   size_t size;
};

enum clc_kernel_arg_type_qualifier {
   CLC_KERNEL_ARG_TYPE_CONST = 1 << 0,
   CLC_KERNEL_ARG_TYPE_RESTRICT = 1 << 1,
   CLC_KERNEL_ARG_TYPE_VOLATILE = 1 << 2,
};

enum clc_kernel_arg_access_qualifier {
   CLC_KERNEL_ARG_ACCESS_READ = 1 << 0,
   CLC_KERNEL_ARG_ACCESS_WRITE = 1 << 1,
};

enum clc_kernel_arg_address_qualifier {
   CLC_KERNEL_ARG_ADDRESS_PRIVATE,
   CLC_KERNEL_ARG_ADDRESS_CONSTANT,
   CLC_KERNEL_ARG_ADDRESS_LOCAL,
   CLC_KERNEL_ARG_ADDRESS_GLOBAL,
};

struct clc_kernel_arg {
   const char *name;
   const char *type_name;
   unsigned type_qualifier;
   unsigned access_qualifier;
   enum clc_kernel_arg_address_qualifier address_qualifier;
};

enum clc_vec_hint_type {
   CLC_VEC_HINT_TYPE_CHAR = 0,
   CLC_VEC_HINT_TYPE_SHORT = 1,
   CLC_VEC_HINT_TYPE_INT = 2,
   CLC_VEC_HINT_TYPE_LONG = 3,
   CLC_VEC_HINT_TYPE_HALF = 4,
   CLC_VEC_HINT_TYPE_FLOAT = 5,
   CLC_VEC_HINT_TYPE_DOUBLE = 6
};

struct clc_kernel_info {
   const char *name;
   size_t num_args;
   const struct clc_kernel_arg *args;

   unsigned vec_hint_size;
   enum clc_vec_hint_type vec_hint_type;
};

struct clc_object {
   struct spirv_binary spvbin;
   const struct clc_kernel_info *kernels;
   unsigned num_kernels;
};

struct clc_object *
clc_compile(const struct clc_compile_args *args,
            const struct clc_logger *logger);

struct clc_object *
clc_link(const struct clc_linker_args *args,
         const struct clc_logger *logger);

void clc_free_object(struct clc_object *obj);

#ifdef __cplusplus
}
#endif

#endif /* MESA_CLC_H */
