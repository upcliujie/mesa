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

#include "clc.h"
#include "clc_helpers.h"
#include "util/u_debug.h"

#include <stdlib.h>

enum clc_debug_flags {
   CLC_DEBUG_DUMP_SPIRV = 1 << 0,
   CLC_DEBUG_VERBOSE = 1 << 1,
};

static const struct debug_named_value clc_debug_options[] = {
   { "dump_spirv",  CLC_DEBUG_DUMP_SPIRV, "Dump spirv blobs" },
   { "verbose",  CLC_DEBUG_VERBOSE, NULL },
   DEBUG_NAMED_VALUE_END
};

DEBUG_GET_ONCE_FLAGS_OPTION(debug_clc, "CLC_DEBUG", clc_debug_options, 0)

static void
clc_print_kernels_info(const struct clc_object *obj)
{
   fprintf(stdout, "Kernels:\n");
   for (unsigned i = 0; i < obj->num_kernels; i++) {
      const struct clc_kernel_arg *args = obj->kernels[i].args;
      bool first = true;

      fprintf(stdout, "\tvoid %s(", obj->kernels[i].name);
      for (unsigned j = 0; j < obj->kernels[i].num_args; j++) {
         if (!first)
            fprintf(stdout, ", ");
         else
            first = false;

         switch (args[j].address_qualifier) {
         case CLC_KERNEL_ARG_ADDRESS_GLOBAL:
            fprintf(stdout, "__global ");
            break;
         case CLC_KERNEL_ARG_ADDRESS_LOCAL:
            fprintf(stdout, "__local ");
            break;
         case CLC_KERNEL_ARG_ADDRESS_CONSTANT:
            fprintf(stdout, "__constant ");
            break;
         default:
            break;
         }

         if (args[j].type_qualifier & CLC_KERNEL_ARG_TYPE_VOLATILE)
            fprintf(stdout, "volatile ");
         if (args[j].type_qualifier & CLC_KERNEL_ARG_TYPE_CONST)
            fprintf(stdout, "const ");
         if (args[j].type_qualifier & CLC_KERNEL_ARG_TYPE_RESTRICT)
            fprintf(stdout, "restrict ");

         fprintf(stdout, "%s %s", args[j].type_name, args[j].name);
      }
      fprintf(stdout, ");\n");
   }
}

struct clc_object *
clc_compile(const struct clc_compile_args *args,
            const struct clc_logger *logger)
{
   struct clc_object *obj;
   int ret;

   obj = calloc(1, sizeof(*obj));
   if (!obj) {
      clc_error(logger, "D3D12: failed to allocate a clc_object");
      return NULL;
   }

   ret = clc_to_spirv(args, &obj->spvbin, logger);
   if (ret < 0) {
      free(obj);
      return NULL;
   }

   if (debug_get_option_debug_clc() & CLC_DEBUG_DUMP_SPIRV)
      clc_dump_spirv(&obj->spvbin, stdout);

   return obj;
}

struct clc_object *
clc_link(const struct clc_linker_args *args,
         const struct clc_logger *logger)
{
   struct clc_object *out_obj;
   int ret;

   out_obj = malloc(sizeof(*out_obj));
   if (!out_obj) {
      clc_error(logger, "failed to allocate a clc_object");
      return NULL;
   }

   ret = clc_link_spirv_binaries(args, &out_obj->spvbin, logger);
   if (ret < 0) {
      free(out_obj);
      return NULL;
   }

   if (debug_get_option_debug_clc() & CLC_DEBUG_DUMP_SPIRV)
      clc_dump_spirv(&out_obj->spvbin, stdout);

   out_obj->kernels = clc_spirv_get_kernels_info(&out_obj->spvbin,
                                                 &out_obj->num_kernels);

   if (debug_get_option_debug_clc() & CLC_DEBUG_VERBOSE)
      clc_print_kernels_info(out_obj);

   return out_obj;
}

void clc_free_object(struct clc_object *obj)
{
   clc_free_kernels_info(obj->kernels, obj->num_kernels);
   clc_free_spirv_binary(&obj->spvbin);
   free(obj);
}
