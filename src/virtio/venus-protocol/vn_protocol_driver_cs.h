/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_PROTOCOL_DRIVER_CS_H
#define VN_PROTOCOL_DRIVER_CS_H

#include <assert.h>

/*
 * These types/functions are expected
 *
 *   struct vn_cs
 *   vn_cs_reset
 *   vn_cs_set_error
 *   vn_cs_set_in_data
 *   vn_cs_in
 *   vn_cs_in_peek
 *   vn_cs_has_out
 *   vn_cs_reserve_out
 *   vn_cs_out
 *   vn_cs_out_begin_reply_stream
 *   vn_cs_out_end_reply_stream
 *   vn_cs_end_out
 *   vn_cs_handle_load_id
 *   vn_cs_handle_store_id
 */
#include "vn_cs.h"

static inline void
vn_encode(struct vn_cs *cs, size_t size, const void *data, size_t data_size)
{
   assert(size % 4 == 0);
   /* TODO check if the generated code is optimal */
   vn_cs_out(cs, size, data, data_size);
}

static inline void
vn_decode(struct vn_cs *cs, size_t size, void *data, size_t data_size)
{
   assert(size % 4 == 0);
   vn_cs_in(cs, size, data, data_size);
}

#endif /* VN_PROTOCOL_DRIVER_CS_H */
