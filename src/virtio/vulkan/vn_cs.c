/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vn_cs.h"

void
vn_cs_init(struct vn_cs *cs,
           const VkAllocationCallbacks *alloc,
           VkSystemAllocationScope alloc_scope,
           size_t out_min_size)
{
   memset(cs, 0, sizeof(*cs));
   cs->allocator = alloc;
   cs->alloc_scope = alloc_scope;

   cs->out.min_iov_size = out_min_size;
}

void
vn_cs_fini(struct vn_cs *cs)
{
   for (uint32_t i = 0; i < cs->out.iov_count; i++)
      vk_free(cs->allocator, cs->out.iovs[i].iov_base);
   if (cs->out.iovs)
      vk_free(cs->allocator, cs->out.iovs);
}

static void
vn_cs_reset_in(struct vn_cs *cs)
{
   memset(&cs->in, 0, sizeof(cs->in));
}

static void
vn_cs_reset_out(struct vn_cs *cs)
{
   if (unlikely(!cs->out.iov_count))
      return;

   /* free all but the last iov */
   for (uint32_t i = 0; i < cs->out.iov_count - 1; i++)
      vk_free(cs->allocator, cs->out.iovs[i].iov_base);

   /* move the last iov to the beginning */
   struct vn_cs_iovec *iov = &cs->out.iovs[0];
   iov->iov_base = cs->out.iovs[cs->out.iov_count - 1].iov_base;
   iov->iov_len = 0;
   cs->out.iov_count = 1;

   cs->out.total_iov_len = 0;

   cs->out.cur = iov->iov_base;
   cs->out.end = iov->iov_base + cs->out.last_iov_size;
}

/**
 * Reset a cs for reuse.
 */
void
vn_cs_reset(struct vn_cs *cs)
{
   /* cs->error is sticky */
   vn_cs_reset_in(cs);
   vn_cs_reset_out(cs);
}

void
vn_cs_set_in_data(struct vn_cs *cs, const void *data, size_t size)
{
   assert(size >= cs->in.reserved);

   cs->in.cur = data;
   cs->in.end = data + size;
}

static uint32_t
grow_array_size(uint32_t size,
                uint32_t used,
                uint32_t growth,
                uint32_t min_size)
{
   assert(size >= used && min_size);
   if (!size)
      size = min_size;

   uint32_t new_size = size;
   while (new_size - used < growth) {
      new_size *= 2;
      if (new_size < size)
         return 0;
   }
   return new_size;
}

static size_t
grow_buffer_size(size_t size, size_t used, size_t growth, size_t min_size)
{
   assert(size >= used && min_size);
   if (!size)
      size = min_size;

   size_t new_size = size;
   while (new_size - used < growth) {
      new_size *= 2;
      if (new_size < size)
         return 0;
   }
   return new_size;
}

static bool
vn_cs_grow_out_iovs(struct vn_cs *cs)
{
   const uint32_t iov_max =
      grow_array_size(cs->out.iov_max, cs->out.iov_count, 1, 4);
   if (!iov_max)
      return false;

   void *iovs =
      vk_realloc(cs->allocator, cs->out.iovs, sizeof(*cs->out.iovs) * iov_max,
                 VN_DEFAULT_ALIGN, cs->alloc_scope);
   if (!iovs)
      return false;

   cs->out.iovs = iovs;
   cs->out.iov_max = iov_max;

   return true;
}

static void
vn_cs_set_out_iov_len(struct vn_cs *cs)
{
   if (unlikely(!cs->out.iov_count))
      return;

   struct vn_cs_iovec *iov = &cs->out.iovs[cs->out.iov_count - 1];
   assert(!iov->iov_len && iov->iov_base <= cs->out.cur);
   iov->iov_len = cs->out.cur - iov->iov_base;
   assert(iov->iov_len <= cs->out.last_iov_size);
   cs->out.total_iov_len += iov->iov_len;

   cs->out.end = cs->out.cur;
}

/**
 * Add a new iovec to a cs.
 */
bool
vn_cs_reserve_out_internal(struct vn_cs *cs, size_t size)
{
   if (cs->out.iov_count >= cs->out.iov_max) {
      if (!vn_cs_grow_out_iovs(cs))
         return false;
      assert(cs->out.iov_count < cs->out.iov_max);
   }

   const size_t iov_size =
      grow_buffer_size(cs->out.last_iov_size, cs->out.last_iov_size, size,
                       cs->out.min_iov_size);
   if (!iov_size)
      return false;

   void *base =
      vk_alloc(cs->allocator, iov_size, VN_DEFAULT_ALIGN, cs->alloc_scope);
   if (!base)
      return false;

   vn_cs_set_out_iov_len(cs);

   /* add a new iov */
   struct vn_cs_iovec *iov = &cs->out.iovs[cs->out.iov_count++];
   iov->iov_base = base;
   iov->iov_len = 0;
   cs->out.last_iov_size = iov_size;

   /* switch to the new iov */
   cs->out.cur = iov->iov_base;
   cs->out.end = iov->iov_base + cs->out.last_iov_size;

   return true;
}

/*
 * End command emission.
 */
void
vn_cs_end_out(struct vn_cs *cs)
{
   vn_cs_set_out_iov_len(cs);
}
