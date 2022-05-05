#ifndef ACO_SHADER_INFO_H
#define ACO_SHADER_INFO_H

#include "shader_enums.h"
/* temporary */
#include "vulkan/radv_shader.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aco_shader_info {
   bool has_ngg_culling;
   bool has_ngg_early_prim_export;
   uint32_t num_tess_patches;
   unsigned workgroup_size;
   struct {
      struct radv_vs_output_info outinfo;
      bool tcs_in_out_eq;
      uint64_t tcs_temp_only_input_mask;
      bool use_per_attribute_vb_descs;
      uint32_t vb_desc_usage_mask;
      bool has_prolog;
      bool dynamic_inputs;
   } vs;
   struct {
      uint8_t output_usage_mask[VARYING_SLOT_VAR31 + 1];
      uint8_t num_stream_output_components[4];
      uint8_t output_streams[VARYING_SLOT_VAR31 + 1];
      unsigned vertices_out;
   } gs;
   struct {
      uint32_t num_lds_blocks;
   } tcs;
   struct {
      struct radv_vs_output_info outinfo;
   } tes;
   struct {
      bool writes_z;
      bool writes_stencil;
      bool writes_sample_mask;
      uint32_t num_interp;
      unsigned spi_ps_input;
   } ps;
   struct {
      uint8_t subgroup_size;
   } cs;
   struct {
      struct radv_vs_output_info outinfo;
   } ms;
   struct radv_streamout_info so;

   uint32_t gfx9_gs_ring_lds_size;
};

#ifdef __cplusplus
}
#endif
#endif
