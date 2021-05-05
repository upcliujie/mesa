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

#include "dxbc.h"
#include <string.h>
#include <stdio.h>

#if defined(_MSC_VER) && !defined(snprintf)
#define snprintf _snprintf
#endif


static int
ConvertSignatureEntry(const struct DXBC_DDISignatureEntry *src,
                      struct DXBC_SignatureEntry *dst,
                      int isPixelOutput)
{
   dst->systemValueSemantic = 0;
   dst->semanticIndex = 0;

   if (src->systemName == DXBC_DDI_SYSTEM_NAME_UNDEFINED) {
      if (isPixelOutput) {
         if (src->regNum == 0xffffffff) {
            strncpy(dst->semanticName, "SV_Depth", sizeof dst->semanticName);
         } else {
            strncpy(dst->semanticName, "SV_Target", sizeof dst->semanticName);
            dst->semanticIndex = src->regNum;
         }
      } else {
         char suffix;

         /*
          * There can be multiple entries with the same regNum value.
          * Discriminate between them by using a suffix identifying
          * the first used component in register mask.
          */

         if (src->mask & 1) {
            suffix = 'a';
         } else if (src->mask & 2) {
            suffix = 'b';
         } else if (src->mask & 4) {
            suffix = 'c';
         } else {
            suffix = 'd';
         }
         snprintf(dst->semanticName, sizeof dst->semanticName, "_%c%c%c",
                     'A' + (src->regNum % ('Z' - 'A' + 1)),
                     'A' + (src->regNum / ('Z' - 'A' + 1)),
                     suffix);
      }
   } else {
      switch (src->systemName) {
      case DXBC_DDI_SYSTEM_NAME_POSITION:
         strncpy(dst->semanticName, "SV_Position", sizeof dst->semanticName);
         dst->systemValueSemantic = 1;
         break;
      case DXBC_DDI_SYSTEM_NAME_CLIP_DISTANCE:
         /* XXX: Indexed */
         strncpy(dst->semanticName, "SV_ClipDistance", sizeof dst->semanticName);
         break;
      case DXBC_DDI_SYSTEM_NAME_CULL_DISTANCE:
         /* XXX: Indexed */
         strncpy(dst->semanticName, "SV_CullDistance", sizeof dst->semanticName);
         break;
      case DXBC_DDI_SYSTEM_NAME_RENDER_TARGET_ARRAY_INDEX:
         strncpy(dst->semanticName, "SV_RenderTargetArrayIndex", sizeof dst->semanticName);
         break;
      case DXBC_DDI_SYSTEM_NAME_VIEWPORT_ARRAY_INDEX:
         strncpy(dst->semanticName, "SV_ViewportArrayIndex", sizeof dst->semanticName);
         break;
      case DXBC_DDI_SYSTEM_NAME_VERTEX_ID:
         strncpy(dst->semanticName, "SV_VertexID", sizeof dst->semanticName);
         break;
      case DXBC_DDI_SYSTEM_NAME_PRIMITIVE_ID:
         strncpy(dst->semanticName, "SV_PrimitiveID", sizeof dst->semanticName);
         break;
      case DXBC_DDI_SYSTEM_NAME_INSTANCE_ID:
         strncpy(dst->semanticName, "SV_InstanceID", sizeof dst->semanticName);
         break;
      case DXBC_DDI_SYSTEM_NAME_IS_FRONT_FACE:
         strncpy(dst->semanticName, "SV_IsFrontFace", sizeof dst->semanticName);
         break;
      case DXBC_DDI_SYSTEM_NAME_SAMPLE_INDEX:
         strncpy(dst->semanticName, "SV_SampleIndex", sizeof dst->semanticName);
         break;
      }
   }

   dst->componentType = DXBC_COMPONENT_TYPE_FLOAT;
   dst->registerIndex = src->regNum;
   return 0;
}

static int
ConvertInputSignature(const struct DXBC_DDISignature *ddi,
                      struct DXBC_Signature *dxbc)
{
   unsigned int i;

   for (i = 0; i < ddi->numEntries; i++) {
      const struct DXBC_DDISignatureEntry *src = &ddi->entries[i];
      struct DXBC_SignatureEntry *dst = &dxbc->entries[i];

      if (ConvertSignatureEntry(src, dst, 0)) {
         return 1;
      }
      dst->mask = (src->mask & 0xf) | ((src->mask & 0xf) << 8);
   }
   dxbc->numEntries = ddi->numEntries;
   dxbc->flags = 8;

   return 0;
}

static int
ConvertOutputSignature(const struct DXBC_DDISignature *ddi,
                       struct DXBC_Signature *dxbc,
                       int isPixel)
{
   unsigned int i;

   for (i = 0; i < ddi->numEntries; i++) {
      const struct DXBC_DDISignatureEntry *src = &ddi->entries[i];
      struct DXBC_SignatureEntry *dst = &dxbc->entries[i];

      if (ConvertSignatureEntry(src, dst, isPixel)) {
         return 1;
      }
      dst->mask = (src->mask & 0xf) | ((~src->mask & 0xf) << 8);
   }
   dxbc->numEntries = ddi->numEntries;
   dxbc->flags = 8;

   return 0;
}

int
DXBC_FromDDI(const struct DXBC_DDIInfo *ddi,
             struct DXBC_Info *dxbc)
{
   int isPixel = ddi->shader && (ddi->shader[0] & 0xffff0000) == 0;

   if (ConvertInputSignature(&ddi->input, &dxbc->input)) {
      return 1;
   }
   if (ConvertOutputSignature(&ddi->output, &dxbc->output, isPixel)) {
      return 1;
   }
   dxbc->shader = ddi->shader;
   return 0;
}
