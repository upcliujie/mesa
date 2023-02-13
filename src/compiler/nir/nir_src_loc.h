#ifndef NIR_SRC_LOC_H
#define NIR_SRC_LOC_H

#include <stdint.h>

typedef struct nir_src_loc {
   const char *file;

   uint32_t line, col;

   size_t spirv_offset;
} nir_src_loc;

#endif
