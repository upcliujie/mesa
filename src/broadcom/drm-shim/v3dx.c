/*
 * Copyright © 2014-2017 Broadcom
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

/* @file
 *
 * v3d driver code interacting v3dv3 simulator/fpga library.
 *
 * This is compiled per V3D version we support, since the register definitions
 * conflict.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "util/bitscan.h"
#include "util/macros.h"
#include "util/u_mm.h"
#include "broadcom/common/v3d_macros.h"
#include "v3d_simulator_wrapper.h"
#include "drm-shim/drm_shim.h"
#include "drm-uapi/v3d_drm.h"
#include "v3d.h"

#define HW_REGISTER_RO(x) (x)
#define HW_REGISTER_RW(x) (x)
#if V3D_VERSION >= 41
#include "libs/core/v3d/registers/4.1.35.0/v3d.h"
#else
#include "libs/core/v3d/registers/3.3.0.0/v3d.h"
#endif

#define V3D_WRITE(reg, val) v3d_hw_write_reg(v3d.hw, reg, val)
#define V3D_READ(reg) v3d_hw_read_reg(v3d.hw, reg)

static void
v3d_flush_l3()
{
#if V3D_VERSION < 40
        uint32_t gca_ctrl = V3D_READ(V3D_GCA_CACHE_CTRL);

        V3D_WRITE(V3D_GCA_CACHE_CTRL, gca_ctrl | V3D_GCA_CACHE_CTRL_FLUSH_SET);
        V3D_WRITE(V3D_GCA_CACHE_CTRL, gca_ctrl & ~V3D_GCA_CACHE_CTRL_FLUSH_SET);
#endif
}

/* Invalidates the L2 cache.  This is a read-only cache. */
static void
v3d_flush_l2(void)
{
        V3D_WRITE(V3D_CTL_0_L2CACTL,
                  V3D_CTL_0_L2CACTL_L2CCLR_SET |
                  V3D_CTL_0_L2CACTL_L2CENA_SET);
}

/* Invalidates texture L2 cachelines */
static void
v3d_flush_l2t(void)
{
        V3D_WRITE(V3D_CTL_0_L2TFLSTA, 0);
        V3D_WRITE(V3D_CTL_0_L2TFLEND, ~0);
        V3D_WRITE(V3D_CTL_0_L2TCACTL,
                  V3D_CTL_0_L2TCACTL_L2TFLS_SET |
                  (0 << V3D_CTL_0_L2TCACTL_L2TFLM_LSB));
}

/* Invalidates the slice caches.  These are read-only caches. */
static void
v3d_flush_slices(void)
{
        V3D_WRITE(V3D_CTL_0_SLCACTL, ~0);
}

static void
v3d_flush_caches(void)
{
        v3d_flush_l3();
        v3d_flush_l2();
        v3d_flush_l2t();
        v3d_flush_slices();
}

static void
v3d_simulator_copy_in_handle(struct shim_fd *shim_fd, int handle)
{
        if (!handle)
                return;

        struct v3d_bo *bo = v3d_bo_lookup(shim_fd, handle);

        memcpy(bo->sim_vaddr, bo->gem_vaddr, bo->base.size);
}

static void
v3d_simulator_copy_out_handle(struct shim_fd *shim_fd, int handle)
{
        if (!handle)
                return;

        struct v3d_bo *bo = v3d_bo_lookup(shim_fd, handle);

        memcpy(bo->gem_vaddr, bo->sim_vaddr, bo->base.size);
}

static int
v3dX(v3d_ioctl_submit_cl)(int fd, unsigned long request, void *arg)
{
        struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
        struct drm_v3d_submit_cl *submit = arg;
        uint32_t *bo_handles = (uint32_t *)(uintptr_t)submit->bo_handles;

        for (int i = 0; i < submit->bo_handle_count; i++)
                v3d_simulator_copy_in_handle(shim_fd, bo_handles[i]);

        v3d_flush_caches();

        if (submit->qma) {
                V3D_WRITE(V3D_CLE_0_CT0QMA, submit->qma);
                V3D_WRITE(V3D_CLE_0_CT0QMS, submit->qms);
        }
#if V3D_VERSION >= 41
        if (submit->qts) {
                V3D_WRITE(V3D_CLE_0_CT0QTS,
                          V3D_CLE_0_CT0QTS_CTQTSEN_SET |
                          submit->qts);
        }
#endif

        fprintf(stderr, "submit %x..%x!\n", submit->bcl_start, submit->bcl_end);

        V3D_WRITE(V3D_CLE_0_CT0QBA, submit->bcl_start);
        V3D_WRITE(V3D_CLE_0_CT0QEA, submit->bcl_end);

        /* Wait for bin to complete before firing render, as it seems the
         * simulator doesn't implement the semaphores.
         */
        while (V3D_READ(V3D_CLE_0_CT0CA) !=
               V3D_READ(V3D_CLE_0_CT0EA)) {
                v3d_hw_tick(v3d.hw);
        }

        fprintf(stderr, "submit %x..%x!\n", submit->rcl_start, submit->rcl_end);

        v3d_flush_caches();

        V3D_WRITE(V3D_CLE_0_CT1QBA, submit->rcl_start);
        V3D_WRITE(V3D_CLE_0_CT1QEA, submit->rcl_end);

        while (V3D_READ(V3D_CLE_0_CT1CA) !=
               V3D_READ(V3D_CLE_0_CT1EA)) {
                v3d_hw_tick(v3d.hw);
        }

        for (int i = 0; i < submit->bo_handle_count; i++)
                v3d_simulator_copy_out_handle(shim_fd, bo_handles[i]);

        return 0;
}

static int
v3dX(v3d_ioctl_submit_tfu)(int fd, unsigned long request, void *arg)
{
        struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
        struct drm_v3d_submit_tfu *submit = arg;

        v3d_simulator_copy_in_handle(shim_fd, submit->bo_handles[0]);
        v3d_simulator_copy_in_handle(shim_fd, submit->bo_handles[1]);
        v3d_simulator_copy_in_handle(shim_fd, submit->bo_handles[2]);
        v3d_simulator_copy_in_handle(shim_fd, submit->bo_handles[3]);

        int last_vtct = V3D_READ(V3D_TFU_CS) & V3D_TFU_CS_CVTCT_SET;

        V3D_WRITE(V3D_TFU_IIA, submit->iia);
        V3D_WRITE(V3D_TFU_IIS, submit->iis);
        V3D_WRITE(V3D_TFU_ICA, submit->ica);
        V3D_WRITE(V3D_TFU_IUA, submit->iua);
        V3D_WRITE(V3D_TFU_IOA, submit->ioa);
        V3D_WRITE(V3D_TFU_IOS, submit->ios);
        V3D_WRITE(V3D_TFU_COEF0, submit->coef[0]);
        V3D_WRITE(V3D_TFU_COEF1, submit->coef[1]);
        V3D_WRITE(V3D_TFU_COEF2, submit->coef[2]);
        V3D_WRITE(V3D_TFU_COEF3, submit->coef[3]);

        V3D_WRITE(V3D_TFU_ICFG, submit->icfg);

        while ((V3D_READ(V3D_TFU_CS) & V3D_TFU_CS_CVTCT_SET) == last_vtct) {
                v3d_hw_tick(v3d.hw);
        }

        v3d_simulator_copy_out_handle(shim_fd, submit->bo_handles[0]);

        return 0;
}

static int
v3dX(v3d_ioctl_create_bo)(int fd, unsigned long request, void *arg)
{
        struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
        struct drm_v3d_create_bo *create = arg;
        struct v3d_bo *bo = calloc(1, sizeof(*bo));

        drm_shim_bo_init(&bo->base, create->size);
        bo->offset = util_vma_heap_alloc(&v3d.heap, create->size, 4096);
        if (bo->offset == 0)
                return -ENOMEM;

        bo->sim_vaddr = v3d.mem + bo->offset - v3d.mem_base;
#if 0
        /* Place a mapping of the BO inside of the simulator's address space
         * for V3D memory.  This lets us avoid copy in/out for simpenrose, but
         * I'm betting we'll need something else for FPGA.
         */
        void *sim_addr = v3d.mem + bo->block->ofs;
        void *mmap_ret = mmap(sim_addr, create->size, PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_FIXED, shim_fd->fd, 0);
        assert(mmap_ret == sim_addr);
#else
        /* Make a simulator-private mapping of the shim GEM object. */
        bo->gem_vaddr = mmap(NULL, bo->base.size,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             shim_fd->fd, 0);
        if (bo->gem_vaddr == MAP_FAILED) {
                fprintf(stderr, "v3d: mmap of shim bo failed\n");
                abort();
        }
#endif

        create->offset = bo->offset;
        create->handle = drm_shim_bo_get_handle(shim_fd, &bo->base);

        drm_shim_bo_put(&bo->base);

        return 0;
}

static int
v3dX(v3d_ioctl_get_param)(int fd, unsigned long request, void *arg)
{
        struct drm_v3d_get_param *gp = arg;
        static const uint32_t reg_map[] = {
                [DRM_V3D_PARAM_V3D_UIFCFG] = V3D_HUB_CTL_UIFCFG,
                [DRM_V3D_PARAM_V3D_HUB_IDENT1] = V3D_HUB_CTL_IDENT1,
                [DRM_V3D_PARAM_V3D_HUB_IDENT2] = V3D_HUB_CTL_IDENT2,
                [DRM_V3D_PARAM_V3D_HUB_IDENT3] = V3D_HUB_CTL_IDENT3,
                [DRM_V3D_PARAM_V3D_CORE0_IDENT0] = V3D_CTL_0_IDENT0,
                [DRM_V3D_PARAM_V3D_CORE0_IDENT1] = V3D_CTL_0_IDENT1,
                [DRM_V3D_PARAM_V3D_CORE0_IDENT2] = V3D_CTL_0_IDENT2,
        };

        switch (gp->param) {
        case DRM_V3D_PARAM_SUPPORTS_TFU:
                gp->value = 1;
                return 0;
        }

        if (gp->param < ARRAY_SIZE(reg_map) && reg_map[gp->param]) {
                gp->value = V3D_READ(reg_map[gp->param]);
                return 0;
        }

        fprintf(stderr, "Unknown DRM_IOCTL_V3D_GET_PARAM %d\n", gp->param);
        return -1;
}

static ioctl_fn_t driver_ioctls[] = {
        [DRM_V3D_SUBMIT_CL] = v3dX(v3d_ioctl_submit_cl),
        [DRM_V3D_SUBMIT_TFU] = v3dX(v3d_ioctl_submit_tfu),
        [DRM_V3D_WAIT_BO] = v3d_ioctl_wait_bo,
        [DRM_V3D_CREATE_BO] = v3dX(v3d_ioctl_create_bo),
        [DRM_V3D_GET_PARAM] = v3dX(v3d_ioctl_get_param),
        [DRM_V3D_MMAP_BO] = v3d_ioctl_mmap_bo,
        [DRM_V3D_GET_BO_OFFSET] = v3d_ioctl_get_bo_offset,
};

static void
v3d_isr_core(unsigned core)
{
        /* FIXME: so far we are assuming just one core, and using only the _0_
         * registers. If we add multiple-core on the simulator, we would need
         * to pass core as a parameter, and chose the proper registers.
         */
        assert(core == 0);
        uint32_t core_status = V3D_READ(V3D_CTL_0_INT_STS);
        V3D_WRITE(V3D_CTL_0_INT_CLR, core_status);

        if (core_status & V3D_CTL_0_INT_STS_INT_OUTOMEM_SET) {
                /* FIXME: how to port the equivalent C&P from the simulator
                 * here?
                 */
                /* uint32_t size = 256 * 1024; */
                /* uint32_t offset = v3d_simulator_get_spill(size); */

                /* v3d_reload_gmp(v3d); */

                /* V3D_WRITE(V3D_PTB_0_BPOA, offset); */
                /* V3D_WRITE(V3D_PTB_0_BPOS, size); */
                /* return; */
                fprintf(stderr, "OUT OF MEM\n");
                abort();
        }

        if (core_status & V3D_CTL_0_INT_STS_INT_GMPV_SET) {
                fprintf(stderr, "GMP violation at 0x%08x\n",
                        V3D_READ(V3D_GMP_VIO_ADDR));
                abort();
        } else {
                fprintf(stderr,
                        "Unexpected ISR with core status 0x%08x\n",
                        core_status);
        }
        abort();
}

static void
handle_mmu_interruptions(uint32_t hub_status)
{
        bool wrv = hub_status & V3D_HUB_CTL_INT_STS_INT_MMU_WRV_SET;
        bool pti = hub_status & V3D_HUB_CTL_INT_STS_INT_MMU_PTI_SET;
        bool cap = hub_status & V3D_HUB_CTL_INT_STS_INT_MMU_CAP_SET;

        if (!(pti || cap || wrv))
                return;

        const char *client = "?";
        uint32_t axi_id = V3D_READ(V3D_MMU_VIO_ID);
        uint32_t va_width = 30;

#if V3D_VERSION >= 41
        static const char *const v3d41_axi_ids[] = {
                "L2T",
                "PTB",
                "PSE",
                "TLB",
                "CLE",
                "TFU",
                "MMU",
                "GMP",
        };

        axi_id = axi_id >> 5;
        if (axi_id < ARRAY_SIZE(v3d41_axi_ids))
                client = v3d41_axi_ids[axi_id];

        uint32_t mmu_debug = V3D_READ(V3D_MMU_DEBUG_INFO);

        va_width += ((mmu_debug & V3D_MMU_DEBUG_INFO_VA_WIDTH_SET)
                     >> V3D_MMU_DEBUG_INFO_VA_WIDTH_LSB);
#endif
        /* Only the top bits (final number depends on the gen) of the virtual
         * address are reported in the MMU VIO_ADDR register.
         */
        uint64_t vio_addr = ((uint64_t)V3D_READ(V3D_MMU_VIO_ADDR) <<
                             (va_width - 32));

        /* Difference with the kernal: here were are going to abort after
         * logging, so we don't bother with some stuff that the kernel does,
         * like restoring the MMU ctrl bits
         */

        fprintf(stderr, "MMU error from client %s (%d) at 0x%llx%s%s%s\n",
                client, axi_id, (long long) vio_addr,
                wrv ? ", write violation" : "",
                pti ? ", pte invalid" : "",
                cap ? ", cap exceeded" : "");

        abort();
}

static void
v3d_isr_hub()
{
        uint32_t hub_status = V3D_READ(V3D_HUB_CTL_INT_STS);

        /* Acknowledge the interrupts we're handling here */
        V3D_WRITE(V3D_HUB_CTL_INT_CLR, hub_status);

        if (hub_status & V3D_HUB_CTL_INT_STS_INT_TFUC_SET) {
                /* FIXME: we were not able to raise this exception. We let the
                 * unreachable here, so we could get one if it is raised on
                 * the future. In any case, note that for this case we would
                 * only be doing debugging log.
                 */
                unreachable("TFU Conversion Complete interrupt not handled");
        }

        handle_mmu_interruptions(hub_status);
}

static void
v3d_isr(uint32_t hub_status)
{
        uint32_t mask = hub_status;

        /* Check the hub_status bits */
        while (mask) {
                unsigned core = u_bit_scan(&mask);

                if (core == v3d_hw_get_hub_core())
                        v3d_isr_hub();
                else
                        v3d_isr_core(core);
        }

        return;
}

static void
v3dX(simulator_init_regs)(void)
{
#if V3D_VERSION == 33
        /* Set OVRTMUOUT to match kernel behavior.
         *
         * This means that the texture sampler uniform configuration's tmu
         * output type field is used, instead of using the hardware default
         * behavior based on the texture type.  If you want the default
         * behavior, you can still put "2" in the indirect texture state's
         * output_type field.
         */
        V3D_WRITE(V3D_CTL_0_MISCCFG, V3D_CTL_1_MISCCFG_OVRTMUOUT_SET);
#endif

        /* FIXME: the kernel captures some additional core interrupts here,
         * for tracing. Perhaps we should evaluate to do the same here and add
         * some debug options.
         */
        uint32_t core_interrupts = (V3D_CTL_0_INT_STS_INT_GMPV_SET |
                                    V3D_CTL_0_INT_STS_INT_OUTOMEM_SET);
        V3D_WRITE(V3D_CTL_0_INT_MSK_SET, ~core_interrupts);
        V3D_WRITE(V3D_CTL_0_INT_MSK_CLR, core_interrupts);

        uint32_t hub_interrupts =
           (V3D_HUB_CTL_INT_STS_INT_MMU_WRV_SET |  /* write violation */
            V3D_HUB_CTL_INT_STS_INT_MMU_PTI_SET |  /* page table invalid */
            V3D_HUB_CTL_INT_STS_INT_MMU_CAP_SET |  /* CAP exceeded */
            V3D_HUB_CTL_INT_STS_INT_TFUC_SET); /* TFU conversion */

        V3D_WRITE(V3D_HUB_CTL_INT_MSK_SET, ~hub_interrupts);
        V3D_WRITE(V3D_HUB_CTL_INT_MSK_CLR, hub_interrupts);

        v3d_hw_set_isr(v3d.hw, v3d_isr);
}

static void
v3d_bo_free(struct shim_bo *shim_bo)
{
        struct v3d_bo *bo = v3d_bo(shim_bo);

        if (bo->gem_vaddr)
                munmap(bo->gem_vaddr, shim_bo->size);

        util_vma_heap_free(&v3d.heap, bo->offset, bo->base.size);
}

void
v3dX(drm_shim_driver_init)(void)
{
        shim_device.driver_ioctls = driver_ioctls;
        shim_device.driver_ioctl_count = ARRAY_SIZE(driver_ioctls);

        shim_device.driver_bo_free = v3d_bo_free;

        /* Allocate a gig of memory to play in. */
        v3d_hw_alloc_mem(v3d.hw, 1024 * 1024 * 1024);
        v3d.mem_base =
                v3d_hw_get_mem(v3d.hw, &v3d.mem_size,
                               &v3d.mem);
        util_vma_heap_init(&v3d.heap, 4096, v3d.mem_size - 4096);

        v3dX(simulator_init_regs)();
}
