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

#include "vk_video.h"
#include "vk_util.h"
#include "vk_alloc.h"
#include "vk_device.h"

#ifdef VK_ENABLE_BETA_EXTENSIONS

VkResult
vk_video_session_init(struct vk_device *device,
                      struct vk_video_session *vid,
                      const VkVideoSessionCreateInfoKHR *create_info)
{
   vk_object_base_init(device, &vid->base, VK_OBJECT_TYPE_VIDEO_SESSION_KHR);

   vid->op = create_info->pVideoProfile->videoCodecOperation;
   vid->max_coded = create_info->maxCodedExtent;
   vid->picture_format = create_info->pictureFormat;
   vid->ref_format = create_info->referencePicturesFormat;
   vid->max_ref_pic_slots = create_info->maxReferencePicturesSlotsCount;
   vid->max_ref_pic_active = create_info->maxReferencePicturesActiveCount;

   switch (vid->op) {
   case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_EXT: {
      const struct VkVideoDecodeH264ProfileEXT *h264_profile =
         vk_find_struct_const(create_info->pVideoProfile->pNext, VIDEO_DECODE_H264_PROFILE_EXT);
      vid->h264.profile_idc = h264_profile->stdProfileIdc;
      break;
   }
   case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_EXT: {
      const struct VkVideoDecodeH265ProfileEXT *h265_profile =
         vk_find_struct_const(create_info->pVideoProfile->pNext, VIDEO_DECODE_H265_PROFILE_EXT);
      vid->h265.profile_idc = h265_profile->stdProfileIdc;
      break;
   }
   default:
      return VK_ERROR_FEATURE_NOT_PRESENT;
   }

   return VK_SUCCESS;
}


static void
init_add_h264_session_parameters(struct vk_video_session_parameters *params,
                                 const struct VkVideoDecodeH264SessionParametersAddInfoEXT *h264_add)
{
   unsigned i, j;
   for (i = 0; i < h264_add->spsStdCount; i++) {
      for (j = 0; j < params->h264_dec.sps_std_count; j++) {
         if (params->h264_dec.sps_std[j].seq_parameter_set_id == h264_add->pSpsStd[i].seq_parameter_set_id) {
            typed_memcpy(&params->h264_dec.sps_std[j], &h264_add->pSpsStd[i], 1);
            break;
         }
      }
      if (j < params->h264_dec.sps_std_count)
         continue;

      typed_memcpy(&params->h264_dec.sps_std[params->h264_dec.sps_std_count], &h264_add->pSpsStd[i], 1);
      params->h264_dec.sps_std_count++;
   }

   for (i = 0; i < h264_add->ppsStdCount; i++) {
      for (j = 0; j < params->h264_dec.pps_std_count; j++) {
         if (params->h264_dec.pps_std[j].pic_parameter_set_id == h264_add->pPpsStd[i].pic_parameter_set_id) {
            typed_memcpy(&params->h264_dec.pps_std[j], &h264_add->pPpsStd[i], 1);
            break;
         }
      }
      if (j < params->h264_dec.pps_std_count)
         continue;

      typed_memcpy(&params->h264_dec.pps_std[params->h264_dec.pps_std_count], &h264_add->pPpsStd[i], 1);
      params->h264_dec.pps_std_count++;
   }
}

static void
init_add_h265_session_parameters(struct vk_video_session_parameters *params,
                                 const struct VkVideoDecodeH265SessionParametersAddInfoEXT *h265_add)
{
   unsigned i, j;
   for (i = 0; i < h265_add->spsStdCount; i++) {
      for (j = 0; j < params->h265_dec.sps_std_count; j++) {
         if (params->h265_dec.sps_std[j].sps_seq_parameter_set_id == h265_add->pSpsStd[i].sps_seq_parameter_set_id) {
            typed_memcpy(&params->h265_dec.sps_std[j], &h265_add->pSpsStd[i], 1);
            break;
         }
      }
      if (j < params->h265_dec.sps_std_count)
         continue;

      typed_memcpy(&params->h265_dec.sps_std[params->h265_dec.sps_std_count], &h265_add->pSpsStd[i], 1);
      params->h265_dec.sps_std_count++;
   }

   for (i = 0; i < h265_add->ppsStdCount; i++) {
      for (j = 0; j < params->h265_dec.pps_std_count; j++) {
         if (params->h265_dec.pps_std[j].pps_pic_parameter_set_id == h265_add->pPpsStd[i].pps_pic_parameter_set_id) {
            typed_memcpy(&params->h265_dec.pps_std[j], &h265_add->pPpsStd[i], 1);
            break;
         }
      }
      if (j < params->h265_dec.pps_std_count)
         continue;

      typed_memcpy(&params->h265_dec.pps_std[params->h265_dec.pps_std_count], &h265_add->pPpsStd[i], 1);
      params->h265_dec.pps_std_count++;
   }
}

VkResult
vk_video_session_parameters_init(struct vk_device *device,
                                 struct vk_video_session_parameters *params,
                                 const struct vk_video_session *vid,
                                 const struct vk_video_session_parameters *templ,
                                 const VkVideoSessionParametersCreateInfoKHR *create_info)
{
   vk_object_base_init(device, &params->base, VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR);

   params->op = vid->op;

   switch (vid->op) {
   case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_EXT: {
      const struct VkVideoDecodeH264SessionParametersCreateInfoEXT *h264_create =
         vk_find_struct_const(create_info->pNext, VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_EXT);

      if (templ) {
         params->h264_dec.max_sps_std_count = templ->h264_dec.max_sps_std_count;
         params->h264_dec.max_pps_std_count = templ->h264_dec.max_pps_std_count;
      } else {
         params->h264_dec.max_sps_std_count = h264_create->maxSpsStdCount;
         params->h264_dec.max_pps_std_count = h264_create->maxPpsStdCount;
      }

      uint32_t sps_size = params->h264_dec.max_sps_std_count * sizeof(StdVideoH264SequenceParameterSet);
      uint32_t pps_size = params->h264_dec.max_pps_std_count * sizeof(StdVideoH264PictureParameterSet);

      params->h264_dec.sps_std = vk_alloc(&device->alloc, sps_size, 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      params->h264_dec.pps_std = vk_alloc(&device->alloc, pps_size, 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      if (!params->h264_dec.sps_std || !params->h264_dec.pps_std) {
         vk_free(&device->alloc, params->h264_dec.sps_std);
         vk_free(&device->alloc, params->h264_dec.pps_std);
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }

      if (templ) {
         typed_memcpy(params->h264_dec.sps_std, templ->h264_dec.sps_std, templ->h264_dec.sps_std_count);
         params->h264_dec.sps_std_count = templ->h264_dec.sps_std_count;
         typed_memcpy(params->h264_dec.pps_std, templ->h264_dec.pps_std, templ->h264_dec.pps_std_count);
         params->h264_dec.pps_std_count = templ->h264_dec.pps_std_count;
      } else {
         params->h264_dec.sps_std_count = 0;
         params->h264_dec.pps_std_count = 0;
      }

      if (h264_create->pParametersAddInfo)
         init_add_h264_session_parameters(params, h264_create->pParametersAddInfo);
      break;
   }
   case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_EXT: {
      const struct VkVideoDecodeH265SessionParametersCreateInfoEXT *h265_create =
         vk_find_struct_const(create_info->pNext, VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_EXT);

      if (templ) {
         params->h265_dec.max_sps_std_count = templ->h265_dec.max_sps_std_count;
         params->h265_dec.max_pps_std_count = templ->h265_dec.max_pps_std_count;
      } else {
         params->h265_dec.max_sps_std_count = h265_create->maxSpsStdCount;
         params->h265_dec.max_pps_std_count = h265_create->maxPpsStdCount;
      }

      uint32_t sps_size = params->h265_dec.max_sps_std_count * sizeof(StdVideoH265SequenceParameterSet);
      uint32_t pps_size = params->h265_dec.max_pps_std_count * sizeof(StdVideoH265PictureParameterSet);

      params->h265_dec.sps_std = vk_alloc(&device->alloc, sps_size, 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      params->h265_dec.pps_std = vk_alloc(&device->alloc, pps_size, 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

      if (!params->h265_dec.sps_std || !params->h265_dec.pps_std) {
         vk_free(&device->alloc, params->h265_dec.sps_std);
         vk_free(&device->alloc, params->h265_dec.pps_std);
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }

      if (templ) {
         typed_memcpy(params->h265_dec.sps_std, templ->h265_dec.sps_std, templ->h265_dec.sps_std_count);
         params->h265_dec.sps_std_count = templ->h265_dec.sps_std_count;
         typed_memcpy(params->h265_dec.pps_std, templ->h265_dec.pps_std, templ->h265_dec.pps_std_count);
         params->h265_dec.pps_std_count = templ->h265_dec.pps_std_count;
      } else {
         params->h265_dec.sps_std_count = 0;
         params->h265_dec.pps_std_count = 0;
      }

      if (h265_create->pParametersAddInfo)
         init_add_h265_session_parameters(params, h265_create->pParametersAddInfo);
   }
   default:
      break;
   }
   return VK_SUCCESS;
}

static VkResult
update_h264_session_parameters(struct vk_video_session_parameters *params,
                               const struct VkVideoDecodeH264SessionParametersAddInfoEXT *h264_add)
{
   VkResult result = VK_SUCCESS;
   if (params->h264_dec.sps_std_count + h264_add->spsStdCount >= params->h264_dec.max_sps_std_count)
      result = VK_ERROR_TOO_MANY_OBJECTS;
   else {
      typed_memcpy(&params->h264_dec.sps_std[params->h264_dec.sps_std_count], h264_add->pSpsStd,
                   h264_add->spsStdCount);
      params->h264_dec.sps_std_count += h264_add->spsStdCount;
   }

   if (params->h264_dec.pps_std_count + h264_add->ppsStdCount >= params->h264_dec.max_pps_std_count)
      result = VK_ERROR_TOO_MANY_OBJECTS;
   else {
      typed_memcpy(&params->h264_dec.pps_std[params->h264_dec.pps_std_count], h264_add->pPpsStd,
                   h264_add->ppsStdCount);
      params->h264_dec.pps_std_count += h264_add->ppsStdCount;
   }
   return result;
}

static VkResult
update_h265_session_parameters(struct vk_video_session_parameters *params,
                               const struct VkVideoDecodeH265SessionParametersAddInfoEXT *h265_add)
{
   VkResult result = VK_SUCCESS;
   if (params->h265_dec.sps_std_count + h265_add->spsStdCount >= params->h265_dec.max_sps_std_count)
      result = VK_ERROR_TOO_MANY_OBJECTS;
   else {
      typed_memcpy(&params->h265_dec.sps_std[params->h265_dec.sps_std_count], h265_add->pSpsStd,
                   h265_add->spsStdCount);
      params->h265_dec.sps_std_count += h265_add->spsStdCount;
   }

   if (params->h265_dec.pps_std_count + h265_add->ppsStdCount >= params->h265_dec.max_pps_std_count)
      result = VK_ERROR_TOO_MANY_OBJECTS;
   else {
      typed_memcpy(&params->h265_dec.pps_std[params->h265_dec.pps_std_count], h265_add->pPpsStd,
                   h265_add->ppsStdCount);
      params->h265_dec.pps_std_count += h265_add->ppsStdCount;
   }
   return result;
}

VkResult
vk_video_session_parameters_update(struct vk_video_session_parameters *params,
                                   const VkVideoSessionParametersUpdateInfoKHR *update)
{
   /* 39.6.5. Decoder Parameter Sets -
    * "The provided H.264 SPS/PPS parameters must be within the limits specified during decoder
    * creation for the decoder specified in VkVideoSessionParametersCreateInfoKHR."
    */
   VkResult result = VK_SUCCESS;

   switch (params->op) {
   case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_EXT: {
      const struct VkVideoDecodeH264SessionParametersAddInfoEXT *h264_add =
         vk_find_struct_const(update->pNext, VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_EXT);
      return update_h264_session_parameters(params, h264_add);
   }
   case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_EXT: {
      const struct VkVideoDecodeH265SessionParametersAddInfoEXT *h265_add =
         vk_find_struct_const(update->pNext, VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_EXT);

      return update_h265_session_parameters(params, h265_add);
   }
   default:
      unreachable("Unknown codec\n");
   }
   return result;
}

const StdVideoH264SequenceParameterSet *
vk_video_find_h264_sps(const struct vk_video_session_parameters *params,
                       uint32_t sps_id)
{
   for (unsigned i = 0; i < params->h264_dec.sps_std_count; i++) {
      if (params->h264_dec.sps_std[i].seq_parameter_set_id == sps_id)
         return &params->h264_dec.sps_std[i];
   }
   return NULL;
}

const StdVideoH264PictureParameterSet *
vk_video_find_h264_pps(const struct vk_video_session_parameters *params,
                       uint32_t pps_id)
{
   for (unsigned i = 0; i < params->h264_dec.pps_std_count; i++) {
      if (params->h264_dec.pps_std[i].pic_parameter_set_id == pps_id)
         return &params->h264_dec.pps_std[i];
   }
   return NULL;
}

const StdVideoH265SequenceParameterSet *
vk_video_find_h265_sps(const struct vk_video_session_parameters *params,
                       uint32_t sps_id)
{
   for (unsigned i = 0; i < params->h265_dec.sps_std_count; i++) {
      if (params->h265_dec.sps_std[i].sps_seq_parameter_set_id == sps_id)
         return &params->h265_dec.sps_std[i];
   }
   return NULL;
}



const StdVideoH265PictureParameterSet *
vk_video_find_h265_pps(const struct vk_video_session_parameters *params,
                       uint32_t pps_id)
{
   for (unsigned i = 0; i < params->h265_dec.pps_std_count; i++) {
      if (params->h265_dec.pps_std[i].pps_pic_parameter_set_id == pps_id)
         return &params->h265_dec.pps_std[i];
   }
   return NULL;
}

void
vk_video_session_parameters_finish(struct vk_device *device,
                                   struct vk_video_session_parameters *params)
{
   switch (params->op) {
   case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_EXT:
      vk_free(&device->alloc, params->h264_dec.sps_std);
      vk_free(&device->alloc, params->h264_dec.pps_std);
      break;
   case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_EXT:
      vk_free(&device->alloc, params->h265_dec.sps_std);
      vk_free(&device->alloc, params->h265_dec.pps_std);
      break;
   default:
      break;
   }
   vk_object_base_finish(&params->base);
}

#endif
