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

/*
 * ShaderDump.c --
 *    Functions for printing out shaders.
 */

#include "dxbc.h"

#include "DriverIncludes.h"

#include "ShaderDump.h"
#include "util/u_debug.h"


struct ID3D10Blob : public IUnknown {
public:
    virtual LPVOID STDMETHODCALLTYPE
    GetBufferPointer(void) = 0;

    virtual SIZE_T STDMETHODCALLTYPE
    GetBufferSize(void) = 0;
};

typedef ID3D10Blob ID3DBlob;
typedef ID3DBlob* LPD3DBLOB;

#define D3D_DISASM_ENABLE_COLOR_CODE            0x00000001
#define D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS  0x00000002
#define D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING 0x00000004
#define D3D_DISASM_ENABLE_INSTRUCTION_CYCLE     0x00000008
#define D3D_DISASM_DISABLE_DEBUG_INFO           0x00000010
#define D3D_DISASM_ENABLE_INSTRUCTION_OFFSET    0x00000020
#define D3D_DISASM_INSTRUCTION_ONLY             0x00000040

typedef HRESULT
(WINAPI *PFND3DDISASSEMBLE)(
    LPCVOID pSrcData,
    SIZE_T SrcDataSize,
    UINT Flags,
    LPCSTR szComments,
    ID3DBlob **ppDisassembly
);

static PFND3DDISASSEMBLE pfnD3DDisassemble = NULL;

typedef HRESULT
(WINAPI *PFND3D10DISASSEMBLESHADER)(
    const void *pShader,
    SIZE_T BytecodeLength,
    BOOL EnableColorCode,
    LPCSTR pComments,
    ID3D10Blob **ppDisassembly
);

static PFND3D10DISASSEMBLESHADER pfnD3D10DisassembleShader = NULL;


static HRESULT
disassembleShader(const void *pShader,
                  SIZE_T BytecodeLength,
                  ID3D10Blob **ppDisassembly)
{
   static bool firsttime = true;

   if (firsttime) {
      char szFilename[MAX_PATH];
      HMODULE hModule = NULL;

      int version;
      for (version = 44; version >= 33; --version) {
         _snprintf(szFilename, sizeof(szFilename), "d3dcompiler_%i.dll", version);
         hModule = LoadLibraryA(szFilename);
         if (hModule) {
            pfnD3DDisassemble = (PFND3DDISASSEMBLE)
               GetProcAddress(hModule, "D3DDisassemble");
            if (pfnD3DDisassemble) {
               break;
            }
         }
      }

      if (!pfnD3DDisassemble) {
         /*
          * Fallback to D3D10DisassembleShader, which should be always present.
          */
         if (GetSystemDirectoryA(szFilename, MAX_PATH)) {
            strcat(szFilename, "\\d3d10.dll");
            hModule = LoadLibraryA(szFilename);
            if (hModule) {
               pfnD3D10DisassembleShader = (PFND3D10DISASSEMBLESHADER)
                  GetProcAddress(hModule, "D3D10DisassembleShader");
            }
         }
      }

      firsttime = false;
   }

   if (pfnD3DDisassemble) {
      return pfnD3DDisassemble(pShader, BytecodeLength, 0, NULL, ppDisassembly);
   } else if (pfnD3D10DisassembleShader) {
      return pfnD3D10DisassembleShader(pShader, BytecodeLength, 0, NULL, ppDisassembly);
   } else {
      return E_FAIL;
   }
}



static void
dump_uints(const unsigned *data,
           unsigned count)
{
   unsigned i;

   for (i = 0; i < count; i++) {
      if (i % 8 == 7) {
         debug_printf("0x%08x,\n", data[i]);
      } else {
         debug_printf("0x%08x, ", data[i]);
      }
   }
   if (i % 8) {
      debug_printf("\n");
   }
}

void
dx10_shader_dump_binary(const unsigned *code)
{
   dump_uints(code, code[1]);
}


void
dx10_shader_dump_tokens(const unsigned *pShaderBytecode) {

   struct DXBC_Info info;
   memset(&info, 0, sizeof info);
   info.shader = (unsigned int *)pShaderBytecode;

   unsigned int written;
   unsigned char *binary;

   if (DXBC_Write(&info, NULL, 0, &written)) {
      assert(0);
      return;
   }

   binary = (unsigned char *)malloc(written);
   if (!binary) {
      assert(0);
      return;
   }

   if (DXBC_Write(&info, binary, written, &written)) {
      assert(0);
      return;
   }

   LPD3DBLOB pDisassembly = NULL;
   HRESULT hr = disassembleShader(binary, written, &pDisassembly);
   if (SUCCEEDED(hr)) {
      assert(pDisassembly);
      debug_printf((const char *)pDisassembly->GetBufferPointer());
   } else {
      debug_printf("%s: failed to disassemble shader\n", __FUNCTION__);
   }

   if (pDisassembly) {
      pDisassembly->Release();
   }

   free(binary);
}
