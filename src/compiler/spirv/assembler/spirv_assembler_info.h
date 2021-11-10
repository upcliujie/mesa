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

#ifndef _SPIRV_ASSEMBLER_INFO_H_
#define _SPIRV_ASSEMBLER_INFO_H_

#include "../spirv.h"

#ifdef __cplusplus
extern "C" {
#endif

SpvAddressingModel spirv_string_to_addressingmodel(const char *s);
SpvBuiltIn spirv_string_to_builtin(const char *s);
SpvCapability spirv_string_to_capability(const char *s);
SpvDecoration spirv_string_to_decoration(const char *s);
SpvDim spirv_string_to_dim(const char *s);
SpvExecutionMode spirv_string_to_executionmode(const char *s);
SpvExecutionModel spirv_string_to_executionmodel(const char *s);
SpvImageFormat spirv_string_to_imageformat(const char *s);
SpvMemoryModel spirv_string_to_memorymodel(const char *s);
SpvStorageClass spirv_string_to_storageclass(const char *s);
SpvFPRoundingMode spirv_string_to_fproundingmode(const char *s);
SpvFunctionControlMask spirv_string_to_functioncontrol(const char *s);
SpvImageOperandsMask spirv_string_to_imageoperands(const char *s);
SpvMemoryAccessMask spirv_string_to_memoryaccess(const char *s);
SpvLoopControlMask spirv_string_to_loopcontrol(const char *s);
SpvSelectionControlMask spirv_string_to_selectioncontrol(const char *s);
SpvSourceLanguage spirv_string_to_sourcelanguage(const char *s);

enum spirv_operands {
   NONE = 0,
   IDREF,
   IDRESULT = IDREF,
   IDRESULTTYPE = IDREF,
   IDSCOPE = IDREF,
   IDMEMORYSEMANTICS = IDREF,
   LITERALINTEGER,
   LITERALNUMBER = LITERALINTEGER,
   LITERALEXTINSTINTEGER = LITERALINTEGER,
   LITERALCONTEXTDEPENDENTNUMBER = LITERALINTEGER,
   LITERALSPECCONSTANTOPINTEGER = LITERALINTEGER,
   LITERALSTRING,
   IMAGEOPERANDS,
   MEMORYACCESS,
   SOURCELANGUAGE,
   ADDRESSINGMODEL,
   MEMORYMODEL,
   EXECUTIONMODEL,
   EXECUTIONMODE,
   CAPABILITY,
   DIM,
   IMAGEFORMAT,
   ACCESSQUALIFIER,
   GROUPOPERATION,
   DECORATION,
   SELECTIONCONTROL,
   LOOPCONTROL,
   PACKEDVECTORFORMAT,
   FUNCTIONCONTROL,
   STORAGECLASS,
   SAMPLERFILTERMODE,
   SAMPLERADDRESSINGMODE,
   PAIRLITERALINTEGERIDREF,
   PAIRIDREFLITERALINTEGER,
   PAIRIDREFIDREF,

   OPTIONAL = 128,
   STAR = 256,
   MAX = 2 * STAR,
};

struct spirv_op_info {
   SpvOp opcode;
   const enum spirv_operands *operands;
};

struct spirv_op_info spirv_string_to_op_info(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* SPIRV_ASSEMBLER_INFO_H */
