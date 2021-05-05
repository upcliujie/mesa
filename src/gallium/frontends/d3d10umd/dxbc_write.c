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

#include <assert.h>
#include <memory.h>
#include <string.h>

#include "dxbc.h"


#define NO_SIGNATURES_CHUNKS 1


struct OutBuf {
   unsigned char *data;
   int size;
   int capacity;
};

static void
OutBufInit(struct OutBuf *out,
           unsigned char *data,
           int capacity)
{
   out->data = data;
   out->size = 0;
   out->capacity = capacity;
}

static void
OutAdvance(struct OutBuf *out,
           int size)
{
   out->size += size;
}

static void
OutWrite(struct OutBuf *out,
         const unsigned char *data,
         int dataSize)
{
   out->size += dataSize;
   if (out->size <= out->capacity) {
      memcpy(&out->data[out->size - dataSize], data, dataSize);
   }
}

static void
OutDword(struct OutBuf *out,
         unsigned int dw)
{
   OutWrite(out, (const unsigned char *)&dw, 4);
}

static void
UpdateDword(struct OutBuf *out,
            int pos,
            unsigned int dw)
{
   if (pos + 4 <= out->capacity) {
      memcpy(&out->data[pos], &dw, 4);
   }
}

static void
WriteSgn(const struct DXBC_Signature *sig,
         struct OutBuf *out)
{
   unsigned int i;
   struct OutBuf entries;

   /* Number of entries. */
   OutDword(out, sig->numEntries);

   /* Unknown. */
   OutDword(out, 8);

   /* Entry descriptors. */
   OutBufInit(&entries, &out->data[out->size], out->capacity - out->size);
   OutAdvance(out, sig->numEntries * 6 * 4);

   for (i = 0; i < sig->numEntries; i++) {
      const struct DXBC_SignatureEntry *entry = &sig->entries[i];

      /* Name offset */
      OutDword(&entries, out->size);
      OutWrite(out, (const unsigned char*)entry->semanticName, (int)strlen(entry->semanticName) + 1);

      OutDword(&entries, entry->semanticIndex);
      OutDword(&entries, entry->systemValueSemantic);
      OutDword(&entries, entry->componentType);
      OutDword(&entries, entry->registerIndex);
      OutDword(&entries, entry->mask);
   }

   /* Align names to double-word boundary. */
   if (out->size & 3) {
      OutAdvance(out, 4 - (out->size & 3));
   }
}

static void
WriteShdr(const struct DXBC_Info *info,
          struct OutBuf *out)
{
   OutWrite(out,
            (const unsigned char *)info->shader,
            info->shader[1] * 4);
}

#if NO_SIGNATURES_CHUNKS
#define CHUNK_SHDR 0
#define NUM_CHUNKS 1
#else
#define CHUNK_ISGN 0
#define CHUNK_OSGN 1
#define CHUNK_SHDR 2
#define NUM_CHUNKS 3
#endif

int
DXBC_Write(const struct DXBC_Info *info,
           unsigned char *data,
           unsigned int dataSize,
           unsigned int *written)
{
   struct OutBuf out;
   unsigned int i;
   int checksumPos, checksumStart;
   int sizePos;
   int chunkPos[NUM_CHUNKS];
   unsigned int chunkTags[NUM_CHUNKS] = {
#if NO_SIGNATURES_CHUNKS
      DXBC_TAG_SHDR
#else
      DXBC_TAG_ISGN,
      DXBC_TAG_OSGN,
      DXBC_TAG_SHDR
#endif
   };
   unsigned int numChunks = info->shader ? NUM_CHUNKS : NUM_CHUNKS - 1;

   OutBufInit(&out, data, dataSize);

   /* Header tag. */
   OutDword(&out, DXBC_TAG_DXBC);

   /* Checksum. */
   checksumPos = out.size;
   OutAdvance(&out, 16);
   checksumStart = out.size;

   /* Version? */
   OutDword(&out, 1);

   /* Total size. */
   sizePos = out.size;
   OutAdvance(&out, 4);

   /* Number of chunks. */
   OutDword(&out, numChunks);

   /* Chunk offsets. */
   for (i = 0; i < numChunks; i++) {
      chunkPos[i] = out.size;
      OutAdvance(&out, 4);
   }

   /* Chunks. */
   for (i = 0; i < numChunks; i++) {
      int sizePos;
      struct OutBuf chunk;

      /* Update chunk offset. */
      UpdateDword(&out, chunkPos[i], out.size);

      /* Chunk tag. */
      OutDword(&out, chunkTags[i]);

      /* Chunk size. */
      sizePos = out.size;
      OutAdvance(&out, 4);

      OutBufInit(&chunk, &out.data[out.size], out.capacity - out.size);

      switch (i) {
#if !NO_SIGNATURES_CHUNKS
      case CHUNK_ISGN:
         WriteSgn(&info->input, &chunk);
         break;
      case CHUNK_OSGN:
         WriteSgn(&info->output, &chunk);
         break;
#endif
      case CHUNK_SHDR:
         WriteShdr(info, &chunk);
         break;
      default:
         assert(0);
         break;
      }
      (void)WriteSgn;

      /* Update chunk size. */
      UpdateDword(&out, sizePos, chunk.size);

      out.size += chunk.size;
   }

   /* Update total size. */
   UpdateDword(&out, sizePos, out.size);

   /* Update checksum. */
   if (out.size <= out.capacity) {
      DXBC_Checksum(&out.data[checksumStart],
                    out.size - checksumStart,
                    &out.data[checksumPos]);
   }

   *written = (unsigned int)out.size;
   return 0;
}
