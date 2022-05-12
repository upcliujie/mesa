// SPDX-License-Identifier: GPL-2.0-only
/*
 * Sync File validation framework
 *
 * Copyright (C) 2012 Google, Inc.
 */

#ifndef __SW_SYNC_H__
#define __SW_SYNC_H__

#include <linux/types.h>
#include <asm/ioctl.h>

/*
 * SW SYNC validation framework
 *
 * A sync object driver that uses a 32bit counter to coordinate
 * synchronization.  Useful when there is no hardware primitive backing
 * the synchronization.
 *
 * To start the framework just open:
 *
 * <debugfs>/sync/sw_sync
 *
 * That will create a sync timeline, all fences created under this timeline
 * file descriptor will belong to the this timeline.
 *
 * The 'sw_sync' file can be opened many times as to create different
 * timelines.
 *
 * Fences can be created with SW_SYNC_IOC_CREATE_FENCE ioctl with struct
 * sw_sync_create_fence_data as parameter.
 *
 * To increment the timeline counter, SW_SYNC_IOC_INC ioctl should be used
 * with the increment as u32. This will update the last signaled value
 * from the timeline and signal any fence that has a seqno smaller or equal
 * to it.
 *
 * struct sw_sync_create_fence_data
 * @value:      the seqno to initialise the fence with
 * @name:       the name of the new sync point
 * @fence:      return the fd of the new sync_file with the created fence
 */
struct sw_sync_create_fence_data {
        __u32   value;
        char    name[32];
        __s32   fence; /* fd of new fence */
};

#define SW_SYNC_IOC_MAGIC       'W'

#define SW_SYNC_IOC_CREATE_FENCE        _IOWR(SW_SYNC_IOC_MAGIC, 0,\
                struct sw_sync_create_fence_data)

#define SW_SYNC_IOC_INC                 _IOW(SW_SYNC_IOC_MAGIC, 1, __u32)

#endif /*  __SW_SYNC_H__ */
