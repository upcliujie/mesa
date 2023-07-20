# Copyright © 2017 Intel Corporation

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

inc_include = [include_directories('.')]
inc_d3d9 = include_directories('D3D9')

# Most things assume that Android headers are in the default include
# path when compiling for Android so add the stub headers to
# inc_include when using the stub.
if with_android_stub
  inc_include += [include_directories('android_stub')]
endif

if with_gallium_st_nine
  install_headers(
    'd3dadapter/d3dadapter9.h', 'd3dadapter/drm.h', 'd3dadapter/present.h',
    subdir : 'd3dadapter',
  )
endif

opencl_headers = files(
  'CL/cl.h',
  'CL/cl.hpp',
  'CL/cl2.hpp',
  'CL/cl_d3d10.h',
  'CL/cl_d3d11.h',
  'CL/cl_dx9_media_sharing.h',
  'CL/cl_dx9_media_sharing_intel.h',
  'CL/cl_egl.h',
  'CL/cl_ext.h',
  'CL/cl_ext_intel.h',
  'CL/cl_gl.h',
  'CL/cl_gl_ext.h',
  'CL/cl_icd.h',
  'CL/cl_platform.h',
  'CL/cl_va_api_media_sharing_intel.h',
  'CL/cl_version.h',
  'CL/opencl.h',
  'CL/opencl.hpp',
)
# Only install the headers if we are building a stand alone implementation and
# not an ICD enabled implementation
if with_gallium_opencl and not with_opencl_icd
  install_headers(
    opencl_headers,
    subdir: 'CL'
  )
endif
