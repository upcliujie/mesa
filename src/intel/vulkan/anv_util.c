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

/** Log an error message.  */
void anv_printflike(1, 2)
anv_loge(const char *format, ...)
{
   va_list va;

   va_start(va, format);
   anv_loge_v(format, va);
   va_end(va);
}

/** \see anv_loge() */
void
anv_loge_v(const char *format, va_list va)
{
   mesa_loge_v(format, va);
}

void
__anv_perf_warn(struct anv_device *device,
                const struct vk_object_base *object,
                const char *file, int line, const char *format, ...)
{
   va_list ap;
   char buffer[256];
   char report[512];

   va_start(ap, format);
   vsnprintf(buffer, sizeof(buffer), format, ap);
   va_end(ap);

   snprintf(report, sizeof(report), "%s: %s", file, buffer);

   vk_debug_report(&device->physical->instance->vk,
                   VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,
                   object, line, 0, "anv", report);

   mesa_logw("%s:%d: PERF: %s", file, line, buffer);
}

VkResult
__vk_errorv(struct anv_instance *instance,
            const struct vk_object_base *object, VkResult error,
            const char *file, int line, const char *format, va_list ap)
{
   char buffer[256];
   char report[512];

   const char *error_str = vk_Result_to_str(error);

   if (format) {
      vsnprintf(buffer, sizeof(buffer), format, ap);

      snprintf(report, sizeof(report), "%s:%d: %s (%s)", file, line, buffer,
               error_str);
   } else {
      snprintf(report, sizeof(report), "%s:%d: %s", file, line, error_str);
   }

   if (instance) {
      vk_debug_report(&instance->vk, VK_DEBUG_REPORT_ERROR_BIT_EXT,
                      object, line, 0, "anv", report);
   }

   mesa_loge("%s", report);

   return error;
}

VkResult
__vk_errorf(struct anv_instance *instance,
            const struct vk_object_base *object, VkResult error,
            const char *file, int line, const char *format, ...)
{
   va_list ap;

   va_start(ap, format);
   __vk_errorv(instance, object, error, file, line, format, ap);
   va_end(ap);

   return error;
}

enum anv_pipe_bits
anv_pipe_flush_bits_for_access_flags(struct anv_device *device,
                                     VkAccessFlags flags)
{
   enum anv_pipe_bits pipe_bits = 0;

   unsigned b;
   for_each_bit(b, flags) {
      switch ((VkAccessFlagBits)(1 << b)) {
      case VK_ACCESS_SHADER_WRITE_BIT:
         /* We're transitioning a buffer that was previously used as write
          * destination through the data port. To make its content available
          * to future operations, flush the data cache.
          */
         pipe_bits |= ANV_PIPE_DATA_CACHE_FLUSH_BIT;
         break;
      case VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:
         /* We're transitioning a buffer that was previously used as render
          * target. To make its content available to future operations, flush
          * the render target cache.
          */
         pipe_bits |= ANV_PIPE_RENDER_TARGET_CACHE_FLUSH_BIT;
         break;
      case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:
         /* We're transitioning a buffer that was previously used as depth
          * buffer. To make its content available to future operations, flush
          * the depth cache.
          */
         pipe_bits |= ANV_PIPE_DEPTH_CACHE_FLUSH_BIT;
         break;
      case VK_ACCESS_TRANSFER_WRITE_BIT:
         /* We're transitioning a buffer that was previously used as a
          * transfer write destination. Generic write operations include color
          * & depth operations as well as buffer operations like :
          *     - vkCmdClearColorImage()
          *     - vkCmdClearDepthStencilImage()
          *     - vkCmdBlitImage()
          *     - vkCmdCopy*(), vkCmdUpdate*(), vkCmdFill*()
          *
          * Most of these operations are implemented using Blorp which writes
          * through the render target, so flush that cache to make it visible
          * to future operations. And for depth related operations we also
          * need to flush the depth cache.
          */
         pipe_bits |= ANV_PIPE_RENDER_TARGET_CACHE_FLUSH_BIT;
         pipe_bits |= ANV_PIPE_DEPTH_CACHE_FLUSH_BIT;
         break;
      case VK_ACCESS_MEMORY_WRITE_BIT:
         /* We're transitioning a buffer for generic write operations. Flush
          * all the caches.
          */
         pipe_bits |= ANV_PIPE_FLUSH_BITS;
         break;
      case VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT:
      case VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT:
         /* We're transitioning a buffer written either from VS stage or from
          * the command streamer (see CmdEndTransformFeedbackEXT), we just
          * need to stall the CS.
          */
         pipe_bits |= ANV_PIPE_CS_STALL_BIT;
         break;
      default:
         break; /* Nothing to do */
      }
   }

   return pipe_bits;
}

enum anv_pipe_bits
anv_pipe_invalidate_bits_for_access_flags(struct anv_device *device,
                                          VkAccessFlags flags)
{
   enum anv_pipe_bits pipe_bits = 0;

   unsigned b;
   for_each_bit(b, flags) {
      switch ((VkAccessFlagBits)(1 << b)) {
      case VK_ACCESS_INDIRECT_COMMAND_READ_BIT:
         /* Indirect draw commands take a buffer as input that we're going to
          * read from the command streamer to load some of the HW registers
          * (see genX_cmd_buffer.c:load_indirect_parameters). This requires a
          * command streamer stall so that all the cache flushes have
          * completed before the command streamer loads from memory.
          */
         pipe_bits |=  ANV_PIPE_CS_STALL_BIT;
         /* Indirect draw commands also set gl_BaseVertex & gl_BaseIndex
          * through a vertex buffer, so invalidate that cache.
          */
         pipe_bits |= ANV_PIPE_VF_CACHE_INVALIDATE_BIT;
         /* For CmdDipatchIndirect, we also load gl_NumWorkGroups through a
          * UBO from the buffer, so we need to invalidate constant cache.
          */
         pipe_bits |= ANV_PIPE_CONSTANT_CACHE_INVALIDATE_BIT;
         break;
      case VK_ACCESS_INDEX_READ_BIT:
      case VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT:
         /* We transitioning a buffer to be used for as input for vkCmdDraw*
          * commands, so we invalidate the VF cache to make sure there is no
          * stale data when we start rendering.
          */
         pipe_bits |= ANV_PIPE_VF_CACHE_INVALIDATE_BIT;
         break;
      case VK_ACCESS_UNIFORM_READ_BIT:
         /* We transitioning a buffer to be used as uniform data. Because
          * uniform is accessed through the data port & sampler, we need to
          * invalidate the texture cache (sampler) & constant cache (data
          * port) to avoid stale data.
          */
         pipe_bits |= ANV_PIPE_CONSTANT_CACHE_INVALIDATE_BIT;
         if (device->physical->compiler->indirect_ubos_use_sampler)
            pipe_bits |= ANV_PIPE_TEXTURE_CACHE_INVALIDATE_BIT;
         else
            pipe_bits |= ANV_PIPE_DATA_CACHE_FLUSH_BIT;
         break;
      case VK_ACCESS_SHADER_READ_BIT:
      case VK_ACCESS_INPUT_ATTACHMENT_READ_BIT:
      case VK_ACCESS_TRANSFER_READ_BIT:
         /* Transitioning a buffer to be read through the sampler, so
          * invalidate the texture cache, we don't want any stale data.
          */
         pipe_bits |= ANV_PIPE_TEXTURE_CACHE_INVALIDATE_BIT;
         break;
      case VK_ACCESS_MEMORY_READ_BIT:
         /* Transitioning a buffer for generic read, invalidate all the
          * caches.
          */
         pipe_bits |= ANV_PIPE_INVALIDATE_BITS;
         break;
      case VK_ACCESS_MEMORY_WRITE_BIT:
         /* Generic write, make sure all previously written things land in
          * memory.
          */
         pipe_bits |= ANV_PIPE_FLUSH_BITS;
         break;
      case VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT:
      case VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT:
         /* Transitioning a buffer for conditional rendering or transform
          * feedback. We'll load the content of this buffer into HW registers
          * using the command streamer, so we need to stall the command
          * streamer to make sure any in-flight flush operations have
          * completed.
          */
         pipe_bits |= ANV_PIPE_CS_STALL_BIT;
         break;
      default:
         break; /* Nothing to do */
      }
   }

   return pipe_bits;
}
