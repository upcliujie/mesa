# Copyright © Microsoft Corporation

# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

llvm_libdir = dep_llvm.get_variable(cmake : 'LLVM_LIBRARY_DIR', configtool: 'libdir')
dep_clang = cpp.find_library('clang-cpp', dirs : llvm_libdir, required : false)

# meson will return clang-cpp from system dirs if it's not found in llvm_libdir
linker_rpath_arg = '-Wl,--rpath=@0@'.format(llvm_libdir)
clang_test_code = '''
  #include <clang/Basic/Version.h>
  int main (void) {
    size_t found_pos = clang::getClangFullVersion().find(CLANG_VERSION_STRING);
    return found_pos == ::std::string::npos ? 1 : 0;
  }
'''
can_check_clang = (not meson.is_cross_build() or meson.has_exe_wrapper()) and cpp.has_link_argument(linker_rpath_arg)
if can_check_clang
  test_run = cpp.run(clang_test_code, name : 'dep-clang-usable',
                     dependencies : [dep_llvm, dep_clang], args : linker_rpath_arg)
  dep_clang_usable = test_run.compiled() and test_run.returncode() == 0
else
  dep_clang_usable = true
endif
if not (dep_clang.found() and dep_clang_usable)
  dep_clang = [
    cpp.find_library('clangCodeGen', dirs : llvm_libdir),
    cpp.find_library('clangFrontendTool', dirs : llvm_libdir),
    cpp.find_library('clangFrontend', dirs : llvm_libdir),
    cpp.find_library('clangDriver', dirs : llvm_libdir),
    cpp.find_library('clangSerialization', dirs : llvm_libdir),
    cpp.find_library('clangParse', dirs : llvm_libdir),
    cpp.find_library('clangSema', dirs : llvm_libdir),
    cpp.find_library('clangAnalysis', dirs : llvm_libdir),
    cpp.find_library('clangAST', dirs : llvm_libdir),
    cpp.find_library('clangASTMatchers', dirs : llvm_libdir),
    cpp.find_library('clangEdit', dirs : llvm_libdir),
    cpp.find_library('clangLex', dirs : llvm_libdir),
    cpp.find_library('clangBasic', dirs : llvm_libdir),
    polly_dep, polly_isl_dep,
  ]
  # check clang once more
  if can_check_clang
    test_run = cpp.run(clang_test_code, name : 'dep-clang-usable',
                       dependencies : [dep_llvm, dep_clang], args : linker_rpath_arg)
    if not test_run.compiled() or test_run.returncode() != 0
      error('No usable clang found!')
    endif
  endif
endif

clang_resource_dir = join_paths(
  dep_llvm.get_variable(cmake : 'LLVM_LIBRARY_DIR', configtool: 'libdir'),
  'clang', dep_llvm.version(), 'include',
)

opencl_c_h = custom_target(
  'opencl-c.h',
  input : [files_xxd, join_paths(clang_resource_dir, 'opencl-c.h')],
  output : 'opencl-c.h.h',
  command : [prog_python, '@INPUT@', '@OUTPUT@', '-n', 'opencl_c_source'],
)
opencl_c_base_h = custom_target(
  'opencl-c-base.h',
  input : [files_xxd, join_paths(clang_resource_dir, 'opencl-c-base.h')],
  output : 'opencl-c-base.h.h',
  command : [prog_python, '@INPUT@', '@OUTPUT@', '-n', 'opencl_c_base_source'],
)

files_libclc = files(
  'clc.c',
  'clc_helpers.cpp',
)

_libclc = static_library(
  'libclc',
  [files_libclc, sha1_h],
  include_directories : [inc_include, inc_src, inc_mapi, inc_mesa, inc_compiler, inc_gallium, inc_spirv],
  cpp_args : ['-DCLANG_RESOURCE_DIR="@0@"'.format(clang_resource_dir)],
  dependencies: [dep_clang, dep_llvm, dep_llvmspirvlib,
    idep_mesautil, dep_spirv_tools]
)

idep_libclc = declare_dependency(
  link_with : _libclc,
  include_directories : include_directories('.'),
)
