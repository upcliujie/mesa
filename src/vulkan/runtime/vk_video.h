/*
 * Copyright © 2021 Red Hat
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
#ifndef VK_VIDEO_H
#define VK_VIDEO_H

#ifdef VK_ENABLE_BETA_EXTENSIONS

#ifdef __cplusplus
extern "C" {
#endif
#include "vk_object.h"

struct vk_video_session {
   struct vk_object_base base;
   VkVideoCodecOperationFlagsKHR op;
   VkExtent2D max_coded;
   VkFormat picture_format;
   VkFormat ref_format;
   uint32_t max_ref_pic_slots;
   uint32_t max_ref_pic_active;

   union {
      struct {
         uint32_t level;
         StdVideoH264ProfileIdc profile_idc;
      } h264;
      struct {
         StdVideoH265ProfileIdc profile_idc;
      } h265;
   };
};

struct vk_video_session_parameters {
   struct vk_object_base base;
   VkVideoCodecOperationFlagsKHR op;
   union {
      struct {
         uint32_t max_sps_std_count;
         uint32_t max_pps_std_count;

         uint32_t sps_std_count;
         StdVideoH264SequenceParameterSet *sps_std;
         uint32_t pps_std_count;
         StdVideoH264PictureParameterSet *pps_std;
      } h264_dec;

      struct {
         uint32_t max_sps_std_count;
         uint32_t max_pps_std_count;

         uint32_t sps_std_count;
         StdVideoH265SequenceParameterSet *sps_std;
         uint32_t pps_std_count;
         StdVideoH265PictureParameterSet *pps_std;
      } h265_dec;
   };
};

VkResult vk_video_session_object_init(struct vk_device *device,
                                      struct vk_video_session *vid,
                                      const VkVideoSessionCreateInfoKHR *create_info);

VkResult vk_video_session_parameters_object_init(struct vk_device *device,
                                                 struct vk_video_session_parameters *params,
                                                 const struct vk_video_session *vid,
                                                 const VkVideoSessionParametersCreateInfoKHR *create_info);

void vk_video_session_parameters_object_update(struct vk_video_session_parameters *params,
                                               const VkVideoSessionParametersUpdateInfoKHR *update);

void vk_video_session_parameters_object_finish(struct vk_video_session_parameters *params);

#ifdef __cplusplus
}
#endif

#endif
#endif
