/*
 * Copyright © 2021 Valve Corporation
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
 *
 */


#ifndef AC_NIR_H
#define AC_NIR_H

#include "nir.h"
#include "ac_shader_args.h"
#include "ac_shader_util.h"
#include "amd_family.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of nir_builder so we don't have to include nir_builder.h here */
struct nir_builder;
typedef struct nir_builder nir_builder;

typedef nir_ssa_def *(*ac_nir_abi_callback)(nir_builder *, const void *);

typedef struct
{
   /* Descriptor where TCS outputs are stored for TES. */
   ac_nir_abi_callback load_tess_offchip_descriptor;

   /* Descriptor where TCS outputs are stored for the HW tessellator. */
   ac_nir_abi_callback load_tess_factors_descriptor;

   /* Number of patches processed by each TCS workgroup. */
   ac_nir_abi_callback load_tcs_num_patches;

   /* Number of input vertices per patch. */
   ac_nir_abi_callback load_tcs_in_patch_size;

   /* Number of output vertices per patch. */
   ac_nir_abi_callback load_tcs_out_patch_size;

} ac_nir_tess_io_abi;

typedef struct
{
   /* Descriptor where ES outputs are stored and GS inputs are loaded from.
    * Only used by legacy GS on GFX6-8.
    */
   ac_nir_abi_callback load_esgs_ring_descriptor;

} ac_nir_esgs_io_abi;

typedef struct
{
   /* Used by NGG GS to tell whether it should save shader query info to GDS. */
   ac_nir_abi_callback shader_query_enabled;

} ac_nir_ngg_abi;

nir_ssa_def *
ac_nir_load_arg(nir_builder *b, const struct ac_shader_args *ac_args, struct ac_arg arg);

void
ac_nir_lower_ls_outputs_to_mem(nir_shader *ls,
                               bool tcs_in_out_eq,
                               uint64_t tcs_temp_only_inputs,
                               unsigned num_reserved_ls_outputs,
                               const struct ac_shader_args *args,
                               const ac_nir_tess_io_abi *abi,
                               void *user);

void
ac_nir_lower_hs_inputs_to_mem(nir_shader *shader,
                              bool tcs_in_out_eq,
                              unsigned num_reserved_tcs_inputs,
                              const struct ac_shader_args *args,
                              const ac_nir_tess_io_abi *abi,
                              void *user);

void
ac_nir_lower_hs_outputs_to_mem(nir_shader *shader,
                               enum chip_class chip_class,
                               bool tes_reads_tessfactors,
                               uint64_t tes_inputs_read,
                               uint64_t tes_patch_inputs_read,
                               unsigned num_reserved_tcs_inputs,
                               unsigned num_reserved_tcs_outputs,
                               unsigned num_reserved_tcs_patch_outputs,
                               bool emit_tess_factor_write,
                               const struct ac_shader_args *args,
                               const ac_nir_tess_io_abi *abi,
                               void *user);

void
ac_nir_lower_tes_inputs_to_mem(nir_shader *shader,
                               unsigned num_reserved_tcs_outputs,
                               unsigned num_reserved_tcs_patch_outputs,
                               const struct ac_shader_args *args,
                               const ac_nir_tess_io_abi *abi,
                               void *user);

void
ac_nir_lower_es_outputs_to_mem(nir_shader *shader,
                               enum chip_class chip_class,
                               unsigned num_reserved_es_outputs,
                               const struct ac_shader_args *args,
                               const ac_nir_esgs_io_abi *abi,
                               void *user);

void
ac_nir_lower_gs_inputs_to_mem(nir_shader *shader,
                              enum chip_class chip_class,
                              unsigned num_reserved_es_outputs,
                              const struct ac_shader_args *args,
                              const ac_nir_esgs_io_abi *abi,
                              void *user);

bool
ac_nir_lower_indirect_derefs(nir_shader *shader,
                             enum chip_class chip_class);

void
ac_nir_lower_ngg_nogs(nir_shader *shader,
                      unsigned max_num_es_vertices,
                      unsigned num_vertices_per_primitive,
                      unsigned max_workgroup_size,
                      unsigned wave_size,
                      bool can_cull,
                      bool early_prim_export,
                      bool passthrough,
                      bool export_prim_id,
                      bool provoking_vtx_last,
                      bool use_edgeflags,
                      uint32_t instance_rate_inputs,
                      const struct ac_shader_args *args,
                      const ac_nir_ngg_abi *abi,
                      const void *user);

void
ac_nir_lower_ngg_gs(nir_shader *shader,
                    unsigned wave_size,
                    unsigned max_workgroup_size,
                    unsigned esgs_ring_lds_bytes,
                    unsigned gs_out_vtx_bytes,
                    unsigned gs_total_out_vtx_bytes,
                    bool provoking_vtx_last,
                    const struct ac_shader_args *args,
                    const ac_nir_ngg_abi *abi,
                    const void *user);

nir_ssa_def *
ac_nir_cull_triangle(nir_builder *b,
                     nir_ssa_def *initially_accepted,
                     nir_ssa_def *pos[3][4]);

#ifdef __cplusplus
}
#endif

#endif /* AC_NIR_H */
