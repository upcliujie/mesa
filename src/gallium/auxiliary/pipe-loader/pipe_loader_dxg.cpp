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

#include "pipe_loader_priv.h"
#include "util/u_memory.h"

#ifndef _WIN32
#include <wsl/winadapter.h>
#endif

#include <directx/dxcore.h>

#include "d3d12/d3d12_public.h"

struct pipe_loader_dxg_device : public pipe_loader_device {
    IDXCoreAdapter *adapter;
};

static pipe_screen *
pipe_loader_dxg_create_screen(pipe_loader_device *dev,
                              const pipe_screen_config *config)
{
    pipe_loader_dxg_device *dxgdev = static_cast<pipe_loader_dxg_device*>(dev);
    return d3d12_create_screen_from_adapter(nullptr, dxgdev->adapter);
}

static void
pipe_loader_dxg_release(pipe_loader_device **devs)
{
    pipe_loader_base_release(devs);
}

static const driOptionDescription *
pipe_loader_dxg_get_driconf(pipe_loader_device *dev, unsigned *count)
{
    *count = 0;
    return nullptr;
}

static const pipe_loader_ops pipe_loader_dxg_ops {
    pipe_loader_dxg_create_screen,
    pipe_loader_dxg_get_driconf,
    pipe_loader_dxg_release
};

bool
pipe_loader_dxg_probe_one(pipe_loader_device **dev, void *dxcore_adapter)
{
    pipe_loader_dxg_device *dxgdev = CALLOC_STRUCT(pipe_loader_dxg_device);
    if (!dxgdev)
        return false;

    dxgdev->driver_name = "d3d12";
    dxgdev->ops = &pipe_loader_dxg_ops;
    dxgdev->adapter = static_cast<IDXCoreAdapter*>(dxcore_adapter);
    *dev = dxgdev;
    return true;
}

int
pipe_loader_dxg_probe(pipe_loader_device **devs, int ndev)
{
    // TODO
    return 0;
}
