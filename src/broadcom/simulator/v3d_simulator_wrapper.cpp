/*
 * Copyright © 2017 Broadcom
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

/** @file
 *
 * Wraps bits of the V3D simulator interface in a C interface for the
 * v3d_simulator.c code to use.
 */

#ifdef USE_V3D_SIMULATOR

#include "v3d_simulator_wrapper.h"

#define V3D_TECH_VERSION 4
#define V3D_REVISION 1
#define V3D_SUB_REV 35
#define V3D_HIDDEN_REV 0
#define V3D_COMPAT_REV 0
#include "drm-uapi/v3d_drm.h"
#include "v3d_hw_auto.h"
#include "v3d_hw_memaccess.h"
#include "autoclif.h"
#include "simcom_memaccess.h"

extern "C" {

struct v3d_hw *v3d_hw_auto_new(void *in_params)
{
        return v3d_hw_auto_make_unique().release();
}


uint32_t v3d_hw_get_mem(const struct v3d_hw *hw, uint32_t *size, void **p)
{
        return hw->get_mem(size, p);
}

bool v3d_hw_alloc_mem(struct v3d_hw *hw, size_t min_size)
{
        return hw->alloc_mem(min_size) == V3D_HW_ALLOC_SUCCESS;
}

uint32_t v3d_hw_read_reg(struct v3d_hw *hw, uint32_t reg)
{
        return hw->read_reg(reg);
}

void v3d_hw_write_reg(struct v3d_hw *hw, uint32_t reg, uint32_t val)
{
        hw->write_reg(reg, val);
}

void v3d_hw_tick(struct v3d_hw *hw)
{
        return hw->tick();
}

int v3d_hw_get_version(struct v3d_hw *hw)
{
        const V3D_HUB_IDENT_T *ident = hw->get_hub_ident();

        return ident->tech_version * 10 + ident->revision;
}

static int
v3d_hw_get_num_cores(struct v3d_hw *hw)
{
   const V3D_HUB_IDENT_T *ident = hw->get_hub_ident();

   return ident->num_cores;
}

static int
v3d_hw_get_num_qpus_per_core(struct v3d_hw *hw)
{
   const V3D_IDENT_T *ident = hw->get_ident(0);

   return ident->num_slices * ident->num_qpus_per_slice;
}

void
v3d_hw_set_isr(struct v3d_hw *hw, void (*isr)(uint32_t status))
{
        hw->set_isr(isr);
}

uint32_t v3d_hw_get_hub_core()
{
        return V3D_HW_HUB_CORE;
}

void v3d_hw_autoclif_cl(struct v3d_hw *hw,
                        struct drm_v3d_submit_cl *submit,
                        const char *output)
{
   simcom_memaccess ma{};
   v3d_hw_init_host_ro_memaccess(&ma, hw);
   int cores = v3d_hw_get_num_cores(hw);
   int qpus_per_core = v3d_hw_get_num_qpus_per_core(hw);

   autoclif ac(&ma, cores, 4, qpus_per_core);
   ac.bin(0, submit->bcl_start, submit->bcl_end, submit->qma, submit->qms, submit->qts);
   ac.wait_bins();
   ac.auto_clean_core_caches();

   autoclif_addr ac_addr = submit->qma;
   ac.render(0, submit->rcl_start, submit->rcl_end, 1, &ac_addr);
   ac.wait_renders();
   ac.auto_clean_core_caches();

   ac.write_clif(output);
}

}
#endif /* USE_V3D_SIMULATOR */
