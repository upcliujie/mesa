/**************************************************************************
 *
 * Copyright 2012-2021 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 **************************************************************************/

#ifndef __DXBC_H__
#define __DXBC_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DXBC_TAG_DXBC ('D' | ('X' << 8) | ('B' << 16) | ('C' << 24))
#define DXBC_TAG_ISGN ('I' | ('S' << 8) | ('G' << 16) | ('N' << 24))
#define DXBC_TAG_OSGN ('O' | ('S' << 8) | ('G' << 16) | ('N' << 24))
#define DXBC_TAG_SHDR ('S' | ('H' << 8) | ('D' << 16) | ('R' << 24))

#define DXBC_MAX_SIGNATURE_ENTRIES 32

#define DXBC_MAX_SEMANTIC_NAME 32

#define DXBC_COMPONENT_TYPE_UNKNOWN 0
#define DXBC_COMPONENT_TYPE_UINT32  1
#define DXBC_COMPONENT_TYPE_INT32   2
#define DXBC_COMPONENT_TYPE_FLOAT   3

#define DXBC_DDI_SYSTEM_NAME_UNDEFINED                 0
#define DXBC_DDI_SYSTEM_NAME_POSITION                  1
#define DXBC_DDI_SYSTEM_NAME_CLIP_DISTANCE             2
#define DXBC_DDI_SYSTEM_NAME_CULL_DISTANCE             3
#define DXBC_DDI_SYSTEM_NAME_RENDER_TARGET_ARRAY_INDEX 4
#define DXBC_DDI_SYSTEM_NAME_VIEWPORT_ARRAY_INDEX      5
#define DXBC_DDI_SYSTEM_NAME_VERTEX_ID                 6
#define DXBC_DDI_SYSTEM_NAME_PRIMITIVE_ID              7
#define DXBC_DDI_SYSTEM_NAME_INSTANCE_ID               8
#define DXBC_DDI_SYSTEM_NAME_IS_FRONT_FACE             9
#define DXBC_DDI_SYSTEM_NAME_SAMPLE_INDEX              10

struct DXBC_DDISignatureEntry {
   unsigned int systemName;   /* DXBC_DDI_SYSTEM_NAME_x */
   unsigned int regNum;
   unsigned int mask;
};

struct DXBC_DDISignature {
   unsigned int numEntries;
   struct DXBC_DDISignatureEntry entries[DXBC_MAX_SIGNATURE_ENTRIES];
};

struct DXBC_DDIInfo {
   struct DXBC_DDISignature input;
   struct DXBC_DDISignature output;
   unsigned int *shader;
};

struct DXBC_SignatureEntry {
   char semanticName[DXBC_MAX_SEMANTIC_NAME];
   unsigned int semanticIndex;
   unsigned int systemValueSemantic;
   unsigned int componentType;         /* DXBC_COMPONENT_TYPE_x */
   unsigned int registerIndex;
   unsigned int mask;
};

struct DXBC_Signature {
   unsigned int numEntries;
   unsigned int flags;  /* XXX: What's that? */
   struct DXBC_SignatureEntry entries[DXBC_MAX_SIGNATURE_ENTRIES];
};

struct DXBC_Info {
   struct DXBC_Signature input;
   struct DXBC_Signature output;
   unsigned int *shader;
};

void
DXBC_Checksum(const unsigned char *data,
              unsigned int dataSize,
              unsigned char checksum[16]);

int
DXBC_Read(const unsigned char *data,
          unsigned int dataSize,
          struct DXBC_Info *info);

int
DXBC_Write(const struct DXBC_Info *info,
           unsigned char *data,
           unsigned int dataSize,
           unsigned int *written);

int
DXBC_FromDDI(const struct DXBC_DDIInfo *ddi,
             struct DXBC_Info *dxbc);

void
DXBC_Dump(const struct DXBC_Info *info,
          FILE *out);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* __DXBC_H__ */
