/*
 * Copyright © 2022 Google LLC
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "util/log.h"
#include "util/u_process.h"

#include "vtest/vtest_protocol.h"

#include "virtgpu_vtest.h"

static int
connect_sock(void)
{
   struct sockaddr_un un;
   int sock, ret;

   sock = socket(PF_UNIX, SOCK_STREAM, 0);
   if (sock < 0)
      return -1;

   memset(&un, 0, sizeof(un));
   un.sun_family = AF_UNIX;
   snprintf(un.sun_path, sizeof(un.sun_path), "%s", VTEST_DEFAULT_SOCKET_NAME);

   do {
      ret = 0;
      if (connect(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
         ret = -errno;
      }
   } while (ret == -EINTR);

   if (ret) {
      close(sock);
      return ret;
   }

   return sock;
}

static void
send_init(struct vtest *v)
{
   uint32_t buf[VTEST_HDR_SIZE];
   const char *comm = util_get_process_name();

   buf[VTEST_CMD_LEN] = strlen(comm) + 1;
   buf[VTEST_CMD_ID] = VCMD_CREATE_RENDERER;

   vtest_write(v, &buf, sizeof(buf));
   vtest_write(v, (void *)comm, strlen(comm) + 1);
}

static int
negotiate_version(struct vtest *v)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t version_buf[VCMD_PROTOCOL_VERSION_SIZE];
   uint32_t busy_wait_buf[VCMD_BUSY_WAIT_SIZE];
   uint32_t busy_wait_result[1];
   ASSERTED int ret;

   vtest_hdr[VTEST_CMD_LEN] = VCMD_PING_PROTOCOL_VERSION_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_PING_PROTOCOL_VERSION;
   vtest_write(v, &vtest_hdr, sizeof(vtest_hdr));

   vtest_hdr[VTEST_CMD_LEN] = VCMD_BUSY_WAIT_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_RESOURCE_BUSY_WAIT;
   busy_wait_buf[VCMD_BUSY_WAIT_HANDLE] = 0;
   busy_wait_buf[VCMD_BUSY_WAIT_FLAGS] = 0;
   vtest_write(v, &vtest_hdr, sizeof(vtest_hdr));
   vtest_write(v, &busy_wait_buf, sizeof(busy_wait_buf));

   ret = vtest_read(v, vtest_hdr, sizeof(vtest_hdr));
   assert(ret);

   if (vtest_hdr[VTEST_CMD_ID] == VCMD_PING_PROTOCOL_VERSION) {
     /* Read dummy busy_wait response */
     ret = vtest_read(v, vtest_hdr, sizeof(vtest_hdr));
     assert(ret);
     ret = vtest_read(v, busy_wait_result, sizeof(busy_wait_result));
     assert(ret);

     vtest_hdr[VTEST_CMD_LEN] = VCMD_PROTOCOL_VERSION_SIZE;
     vtest_hdr[VTEST_CMD_ID] = VCMD_PROTOCOL_VERSION;
     version_buf[VCMD_PROTOCOL_VERSION_VERSION] = VTEST_PROTOCOL_VERSION;
     vtest_write(v, &vtest_hdr, sizeof(vtest_hdr));
     vtest_write(v, &version_buf, sizeof(version_buf));

     ret = vtest_read(v, vtest_hdr, sizeof(vtest_hdr));
     assert(ret);
     ret = vtest_read(v, version_buf, sizeof(version_buf));
     assert(ret);
     return version_buf[VCMD_PROTOCOL_VERSION_VERSION];
   }

   /* Read dummy busy_wait response */
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_RESOURCE_BUSY_WAIT);
   ret = vtest_read(v, busy_wait_result, sizeof(busy_wait_result));
   assert(ret);

   /* Old server, return version 0 */
   return 0;
}

struct vtest *
vtest_connect(void)
{
   int sock_fd = connect_sock();

   if (sock_fd < 0) {
      mesa_loge("failed to connect: %s", strerror(errno));
      return NULL;
   }

   struct vtest *v = calloc(1, sizeof(v));

   v->sock_fd = sock_fd;
   simple_mtx_init(&v->lock, mtx_plain);

   vtest_lock(v);
   send_init(v);
   v->protocol_version = negotiate_version(v);
   vtest_unlock(v);

   /* Version 1 is deprecated. */
   if (v->protocol_version == 1)
      v->protocol_version = 0;

   mesa_logi("vtest connected, protocol version %d", v->protocol_version);

   return v;
}

int
vtest_write(struct vtest *v, const void *buf, int size)
{
   simple_mtx_assert_locked(&v->lock);
   const void *ptr = buf;
   int left;
   int ret;
   left = size;
   do {
      ret = write(v->sock_fd, ptr, left);
      if (ret < 0)
         return -errno;
      left -= ret;
      ptr += ret;
   } while (left);
   return size;
}

int
vtest_read(struct vtest *v, void *buf, int size)
{
   simple_mtx_assert_locked(&v->lock);
   void *ptr = buf;
   int left;
   int ret;
   left = size;
   do {
      ret = read(v->sock_fd, ptr, left);
      if (ret <= 0) {
         mesa_loge("lost connection to rendering server on %d read %d %d",
                   size, ret, errno);
         abort();
         return ret < 0 ? -errno : 0;
      }
      left -= ret;
      ptr += ret;
   } while (left);
   return size;
}

int
vtest_receive_fd(struct vtest *v)
{
   simple_mtx_assert_locked(&v->lock);
   struct cmsghdr *cmsgh;
   struct msghdr msgh = { 0 };
   char buf[CMSG_SPACE(sizeof(int))], c;
   struct iovec iovec;

   iovec.iov_base = &c;
   iovec.iov_len = sizeof(char);

   msgh.msg_name = NULL;
   msgh.msg_namelen = 0;
   msgh.msg_iov = &iovec;
   msgh.msg_iovlen = 1;
   msgh.msg_control = buf;
   msgh.msg_controllen = sizeof(buf);
   msgh.msg_flags = 0;

   int size = recvmsg(v->sock_fd, &msgh, 0);
   if (size < 0) {
     mesa_loge("Failed with %s", strerror(errno));
     return -1;
   }

   cmsgh = CMSG_FIRSTHDR(&msgh);
   if (!cmsgh) {
     mesa_loge("No headers available");
     return -1;
   }

   if (cmsgh->cmsg_level != SOL_SOCKET) {
     mesa_loge("invalid cmsg_level %d", cmsgh->cmsg_level);
     return -1;
   }

   if (cmsgh->cmsg_type != SCM_RIGHTS) {
     mesa_loge("invalid cmsg_type %d", cmsgh->cmsg_type);
     return -1;
   }

   return *((int *) CMSG_DATA(cmsgh));
}
