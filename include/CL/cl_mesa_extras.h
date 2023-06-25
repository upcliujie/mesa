/* Modifications Copyright (C) 2010-2021 Advanced Micro Devices, Inc. */
/*******************************************************************************
 * Copyright (c) 2008-2020 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

/* Some vendors are silly and don't upstream their defines so we include them here */

#ifndef __CL_MESA_EXTRAS_H
#define __CL_MESA_EXTRAS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <CL/cl.h>

#ifndef CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD
#define CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD 1
typedef union
{
    struct { cl_uint type; cl_uint data[5]; } raw;
    struct { cl_uint type; cl_uchar unused[17]; cl_uchar bus; cl_uchar device; cl_uchar function; } pcie;
} cl_device_topology_amd;
#endif

#ifdef __cplusplus
}
#endif


#endif /* __CL_MESA_EXTRAS_H */
