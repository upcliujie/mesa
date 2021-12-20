/*
 * Copyright © 2015 Intel Corporation
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "anv_private.h"
#include "vk_enum_to_str.h"

void
__anv_perf_warn(struct anv_device *device,
                const struct vk_object_base *object,
                const char *file, int line, const char *format, ...)
{
   va_list ap;
   char buffer[256];

   va_start(ap, format);
   vsnprintf(buffer, sizeof(buffer), format, ap);
   va_end(ap);

   if (object) {
      __vk_log(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
               VK_LOG_OBJS(object), file, line,
               "PERF: %s", buffer);
   } else {
      __vk_log(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
               VK_LOG_NO_OBJS(device->physical->instance), file, line,
               "PERF: %s", buffer);
   }
}

void
anv_dump_pipe_bits(enum anv_pipe_bits bits)
{
   if (bits & ANV_PIPE_DEPTH_CACHE_FLUSH_BIT)
      fputs("+depth_flush ", stderr);
   if (bits & ANV_PIPE_DATA_CACHE_FLUSH_BIT)
      fputs("+dc_flush ", stderr);
   if (bits & ANV_PIPE_HDC_PIPELINE_FLUSH_BIT)
      fputs("+hdc_flush ", stderr);
   if (bits & ANV_PIPE_RENDER_TARGET_CACHE_FLUSH_BIT)
      fputs("+rt_flush ", stderr);
   if (bits & ANV_PIPE_TILE_CACHE_FLUSH_BIT)
      fputs("+tile_flush ", stderr);
   if (bits & ANV_PIPE_STATE_CACHE_INVALIDATE_BIT)
      fputs("+state_inval ", stderr);
   if (bits & ANV_PIPE_CONSTANT_CACHE_INVALIDATE_BIT)
      fputs("+const_inval ", stderr);
   if (bits & ANV_PIPE_VF_CACHE_INVALIDATE_BIT)
      fputs("+vf_inval ", stderr);
   if (bits & ANV_PIPE_TEXTURE_CACHE_INVALIDATE_BIT)
      fputs("+tex_inval ", stderr);
   if (bits & ANV_PIPE_INSTRUCTION_CACHE_INVALIDATE_BIT)
      fputs("+ic_inval ", stderr);
   if (bits & ANV_PIPE_STALL_AT_SCOREBOARD_BIT)
      fputs("+pb_stall ", stderr);
   if (bits & ANV_PIPE_PSS_STALL_SYNC_BIT)
      fputs("+pss_stall ", stderr);
   if (bits & ANV_PIPE_DEPTH_STALL_BIT)
      fputs("+depth_stall ", stderr);
   if (bits & ANV_PIPE_CS_STALL_BIT)
      fputs("+cs_stall ", stderr);
   if (bits & ANV_PIPE_END_OF_PIPE_SYNC_BIT)
      fputs("+eop ", stderr);
}

void
dump_anv_pipe_bits(const char* prefix, VkAccessFlags2KHR bits) {
   fprintf(stderr, "%s", prefix);
   if (bits & VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR ) fprintf(stderr, "INDIRECT_COMMAND_READ_BIT_KHR, ");
   if (bits & VK_ACCESS_2_INDEX_READ_BIT_KHR            ) fprintf(stderr, "INDEX_READ_BIT_KHR, ");
   if (bits & VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR ) fprintf(stderr, "VERTEX_ATTRIBUTE_READ_BIT_KHR, ");
   if (bits & VK_ACCESS_2_UNIFORM_READ_BIT_KHR          ) fprintf(stderr, "UNIFORM_READ_BIT_KHR, ");

   if (bits & VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT_KHR ) fprintf(stderr, "INPUT_ATTACHMENT_READ_BIT_KHR, ");
   if (bits & VK_ACCESS_2_SHADER_READ_BIT_KHR           ) fprintf(stderr, "SHADER_READ_BIT_KHR, ");
   if (bits & VK_ACCESS_2_SHADER_WRITE_BIT_KHR          ) fprintf(stderr, "SHADER_WRITE_BIT_KHR, ");
   if (bits & VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR ) fprintf(stderr, "COLOR_ATTACHMENT_READ_BIT_KHR, ");

   if (bits & VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR) fprintf(stderr, "COLOR_ATTACHMENT_WRITE_BIT_KHR, ");
   if (bits & VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR ) fprintf(stderr, "DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR, ");
   if (bits & VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR ) fprintf(stderr, "DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR, ");
   if (bits & VK_ACCESS_2_TRANSFER_READ_BIT_KHR         ) fprintf(stderr, "TRANSFER_READ_BIT_KHR, ");

   if (bits & VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR        ) fprintf(stderr, "TRANSFER_WRITE_BIT_KHR, ");
   if (bits & VK_ACCESS_2_HOST_READ_BIT_KHR             ) fprintf(stderr, "HOST_READ_BIT_KHR, ");
   if (bits & VK_ACCESS_2_HOST_WRITE_BIT_KHR            ) fprintf(stderr, "HOST_WRITE_BIT_KHR, ");
   if (bits & VK_ACCESS_2_MEMORY_READ_BIT_KHR           ) fprintf(stderr, "MEMORY_READ_BIT_KHR, ");

   if (bits & VK_ACCESS_2_MEMORY_WRITE_BIT_KHR          ) fprintf(stderr, "MEMORY_WRITE_BIT_KHR, ");

   if (bits & VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR   ) fprintf(stderr, "SHADER_SAMPLED_READ_BIT_KHR, ");
   if (bits & VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR   ) fprintf(stderr, "SHADER_STORAGE_READ_BIT_KHR, ");
   if (bits & VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR  ) fprintf(stderr, "SHADER_STORAGE_WRITE_BIT_KHR, ");

   if (bits & VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT           ) fprintf(stderr, "TRANSFORM_FEEDBACK_WRITE_BIT_EXT, ");
   if (bits & VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT    ) fprintf(stderr, "TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT, ");
   if (bits & VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT   ) fprintf(stderr, "TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT, ");
   if (bits & VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT         ) fprintf(stderr, "CONDITIONAL_RENDERING_READ_BIT_EXT, ");
   if (bits & VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_NV             ) fprintf(stderr, "COMMAND_PREPROCESS_READ_BIT_NV, ");
   if (bits & VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV            ) fprintf(stderr, "COMMAND_PREPROCESS_WRITE_BIT_NV, ");
   if (bits & VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR  ) fprintf(stderr, "FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR, ");
   if (bits & VK_ACCESS_2_SHADING_RATE_IMAGE_READ_BIT_NV             ) fprintf(stderr, "SHADING_RATE_IMAGE_READ_BIT_NV, ");
   if (bits & VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR        ) fprintf(stderr, "ACCELERATION_STRUCTURE_READ_BIT_KHR, ");
   if (bits & VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR       ) fprintf(stderr, "ACCELERATION_STRUCTURE_WRITE_BIT_KHR, ");
   if (bits & VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_NV         ) fprintf(stderr, "ACCELERATION_STRUCTURE_READ_BIT_NV , ");
   if (bits & VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_NV        ) fprintf(stderr, "ACCELERATION_STRUCTURE_WRITE_BIT_NV, ");
   if (bits & VK_ACCESS_2_FRAGMENT_DENSITY_MAP_READ_BIT_EXT          ) fprintf(stderr, "FRAGMENT_DENSITY_MAP_READ_BIT_EXT , ");
   if (bits & VK_ACCESS_2_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT  ) fprintf(stderr, "COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT, ");
   if (bits & VK_ACCESS_2_INVOCATION_MASK_READ_BIT_HUAWEI            ) fprintf(stderr, "INVOCATION_MASK_READ_BIT_HUAWEI, ");
}

void
dump_hw_unit_bits(const char* prefix, enum intel_hw_cache_unit bits) {
   fprintf(stderr, "%s", prefix);
   if( bits & INTEL_HW_CACHE_UNIT_VF            ) fprintf(stderr, "VF, ");
   if( bits & INTEL_HW_CACHE_UNIT_DEPTH         ) fprintf(stderr, "DEPTH, ");
   if( bits & INTEL_HW_CACHE_UNIT_CONSTANT      ) fprintf(stderr, "CONSTANT," );
   if( bits & INTEL_HW_CACHE_UNIT_DATA          ) fprintf(stderr, "DATA, ");
   if( bits & INTEL_HW_CACHE_UNIT_TEXTURE       ) fprintf(stderr, "TEXTURE, ");
   if( bits & INTEL_HW_CACHE_UNIT_RENDERTARGET  ) fprintf(stderr, "RENDERTARGET, ");
   if( bits & INTEL_HW_CACHE_UNIT_L3            ) fprintf(stderr, "L3, ");
   if( bits & INTEL_HW_CACHE_UNIT_MAIN_MEMORY   ) fprintf(stderr, "MAIN_MEMORY, ");
   if( bits & INTEL_HW_CACHE_UNIT_CS            ) fprintf(stderr, "CS, ");
   if( bits & INTEL_HW_CACHE_UNIT_CPU           ) fprintf(stderr, "CPU, ");
}
