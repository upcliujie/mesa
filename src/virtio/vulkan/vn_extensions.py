COPYRIGHT = """\
/*
 * Copyright 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
"""

import os.path
import sys

VULKAN_UTIL = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../vulkan/util'))
sys.path.append(VULKAN_UTIL)

from vk_extensions import ApiVersion, Extension, VkVersion

API_VERSIONS = [
    ApiVersion(VkVersion('1.0.68'),  True),
    ApiVersion(VkVersion('1.1.107'), True),
    ApiVersion(VkVersion('1.2.131'), '!ANDROID'),
]

MAX_API_VERSION = API_VERSIONS[-1].version

EXTENSIONS = [
    # promoted to VK_VERSION_1_1
    Extension('VK_KHR_16bit_storage',                     0, False),
    Extension('VK_KHR_bind_memory2',                      0, False),
    Extension('VK_KHR_dedicated_allocation',              0, False),
    Extension('VK_KHR_descriptor_update_template',        0, False),
    Extension('VK_KHR_device_group',                      0, False),
    Extension('VK_KHR_device_group_creation',             0, False),
    Extension('VK_KHR_external_fence',                    0, False),
    Extension('VK_KHR_external_fence_capabilities',       0, False),
    Extension('VK_KHR_external_memory',                   0, False),
    Extension('VK_KHR_external_memory_capabilities',      0, False),
    Extension('VK_KHR_external_semaphore',                0, False),
    Extension('VK_KHR_external_semaphore_capabilities',   0, False),
    Extension('VK_KHR_get_memory_requirements2',          0, False),
    Extension('VK_KHR_get_physical_device_properties2',   0, False),
    Extension('VK_KHR_maintenance1',                      0, False),
    Extension('VK_KHR_maintenance2',                      0, False),
    Extension('VK_KHR_maintenance3',                      0, False),
    Extension('VK_KHR_multiview',                         0, False),
    Extension('VK_KHR_relaxed_block_layout',              0, False),
    Extension('VK_KHR_sampler_ycbcr_conversion',          0, False),
    Extension('VK_KHR_shader_draw_parameters',            0, False),
    Extension('VK_KHR_storage_buffer_storage_class',      0, False),
    Extension('VK_KHR_variable_pointers',                 0, False),
    # promoted to VK_VERSION_1_2
    Extension('VK_KHR_8bit_storage',                      0, False),
    Extension('VK_KHR_buffer_device_address',             0, False),
    Extension('VK_KHR_create_renderpass2',                0, False),
    Extension('VK_KHR_depth_stencil_resolve',             0, False),
    Extension('VK_KHR_draw_indirect_count',               0, False),
    Extension('VK_KHR_driver_properties',                 0, False),
    Extension('VK_KHR_image_format_list',                 0, False),
    Extension('VK_KHR_imageless_framebuffer',             0, False),
    Extension('VK_KHR_sampler_mirror_clamp_to_edge',      0, False),
    Extension('VK_KHR_separate_depth_stencil_layouts',    0, False),
    Extension('VK_KHR_shader_atomic_int64',               0, False),
    Extension('VK_KHR_shader_float16_int8',               0, False),
    Extension('VK_KHR_shader_float_controls',             0, False),
    Extension('VK_KHR_shader_subgroup_extended_types',    0, False),
    Extension('VK_KHR_spirv_1_4',                         0, False),
    Extension('VK_KHR_timeline_semaphore',                0, False),
    Extension('VK_KHR_uniform_buffer_standard_layout',    0, False),
    Extension('VK_KHR_vulkan_memory_model',               0, False),
    Extension('VK_EXT_descriptor_indexing',               0, False),
    Extension('VK_EXT_host_query_reset',                  0, False),
    Extension('VK_EXT_sampler_filter_minmax',             0, False),
    Extension('VK_EXT_scalar_block_layout',               0, False),
    Extension('VK_EXT_separate_stencil_usage',            0, False),
    Extension('VK_EXT_shader_viewport_index_layer',       0, False),
    # WSI
    Extension('VK_KHR_display',                           0, False),
    Extension('VK_KHR_display_swapchain',                 0, False),
    Extension('VK_KHR_get_display_properties2',           0, False),
    Extension('VK_KHR_get_surface_capabilities2',         0, False),
    Extension('VK_KHR_incremental_present',               0, False),
    Extension('VK_KHR_shared_presentable_image',          0, False),
    Extension('VK_KHR_surface',                           0, False),
    Extension('VK_KHR_surface_protected_capabilities',    0, False),
    Extension('VK_KHR_swapchain',                         0, False),
    Extension('VK_KHR_swapchain_mutable_format',          0, False),
    Extension('VK_KHR_wayland_surface',                   0, False),
    Extension('VK_KHR_xcb_surface',                       0, False),
    Extension('VK_KHR_xlib_surface',                      0, False),
    # KHR
    Extension('VK_KHR_external_fence_fd',                 0, False),
    Extension('VK_KHR_external_memory_fd',                0, False),
    Extension('VK_KHR_external_semaphore_fd',             0, False),
    Extension('VK_KHR_performance_query',                 0, False),
    Extension('VK_KHR_pipeline_executable_properties',    0, False),
    Extension('VK_KHR_push_descriptor',                   0, False),
    Extension('VK_KHR_shader_clock',                      0, False),
    Extension('VK_KHR_shader_non_semantic_info',          0, False),
    # EXT
    Extension('VK_EXT_4444_formats',                      0, False),
    Extension('VK_EXT_acquire_xlib_display',              0, False),
    Extension('VK_EXT_buffer_device_address',             0, False),
    Extension('VK_EXT_calibrated_timestamps',             0, False),
    Extension('VK_EXT_conditional_rendering',             0, False),
    Extension('VK_EXT_conservative_rasterization',        0, False),
    Extension('VK_EXT_custom_border_color',               0, False),
    Extension('VK_EXT_debug_report',                      0, False),
    Extension('VK_EXT_depth_clip_enable',                 0, False),
    Extension('VK_EXT_depth_range_unrestricted',          0, False),
    Extension('VK_EXT_direct_mode_display',               0, False),
    Extension('VK_EXT_discard_rectangles',                0, False),
    Extension('VK_EXT_display_control',                   0, False),
    Extension('VK_EXT_display_surface_counter',           0, False),
    Extension('VK_EXT_extended_dynamic_state',            0, False),
    Extension('VK_EXT_external_memory_dma_buf',           0, False),
    Extension('VK_EXT_external_memory_host',              0, False),
    Extension('VK_EXT_fragment_shader_interlock',         0, False),
    Extension('VK_EXT_global_priority',                   0, False),
    Extension('VK_EXT_image_drm_format_modifier',         0, False),
    Extension('VK_EXT_image_robustness',                  0, False),
    Extension('VK_EXT_index_type_uint8',                  0, False),
    Extension('VK_EXT_inline_uniform_block',              0, False),
    Extension('VK_EXT_line_rasterization',                0, False),
    Extension('VK_EXT_memory_budget',                     0, False),
    Extension('VK_EXT_memory_priority',                   0, False),
    Extension('VK_EXT_pci_bus_info',                      0, False),
    Extension('VK_EXT_pipeline_creation_cache_control',   0, False),
    Extension('VK_EXT_pipeline_creation_feedback',        0, False),
    Extension('VK_EXT_post_depth_coverage',               0, False),
    Extension('VK_EXT_private_data',                      0, False),
    Extension('VK_EXT_queue_family_foreign',              0, False),
    Extension('VK_EXT_robustness2',                       0, False),
    Extension('VK_EXT_sample_locations',                  0, False),
    Extension('VK_EXT_shader_atomic_float',               0, False),
    Extension('VK_EXT_shader_demote_to_helper_invocation',0, False),
    Extension('VK_EXT_shader_stencil_export',             0, False),
    Extension('VK_EXT_shader_subgroup_ballot',            0, False),
    Extension('VK_EXT_shader_subgroup_vote',              0, False),
    Extension('VK_EXT_subgroup_size_control',             0, False),
    Extension('VK_EXT_texel_buffer_alignment',            0, False),
    Extension('VK_EXT_transform_feedback',                0, False),
    Extension('VK_EXT_vertex_attribute_divisor',          0, False),
    Extension('VK_EXT_ycbcr_image_arrays',                0, False),
    # vendors
    Extension('VK_ANDROID_external_memory_android_hardware_buffer', 0, False),
    Extension('VK_ANDROID_native_buffer',                 0, False),
    Extension('VK_GOOGLE_decorate_string',                0, False),
    Extension('VK_GOOGLE_hlsl_functionality1',            0, False),
    Extension('VK_GOOGLE_user_type',                      0, False),
]
