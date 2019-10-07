/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on virgl which is:
 * Copyright 2014, 2015 Red Hat.
 */

#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "util/u_process.h"
#define VIRGL_RENDERER_UNSTABLE_APIS
#include "virtio-gpu/virglrenderer_hw.h"
#include "vtest/vtest_protocol.h"

#include "vn_device.h"
#include "vn_renderer.h"

/* connect to remote socket */
#define VTEST_SOCKET_NAME "/tmp/.virgl_test"

struct vtest {
   struct vn_renderer base;
   struct vn_instance *instance;

   mtx_t sock_mutex;
   int sock_fd;

   uint32_t protocol_version;

   bool coherent_dmabuf_blob;
   struct virgl_renderer_capset_venus capset;

   uint32_t sync_queue_count;
   struct vn_renderer_sync *cpu_sync;
   uint64_t cpu_point;
};

struct vtest_bo {
   struct vn_renderer_bo base;
   struct vtest *vtest;

   VkDeviceSize size;
   bool is_dmabuf;
   int res_fd;
   void *res_ptr;
};

struct vtest_sync {
   struct vn_renderer_sync base;
   struct vtest *vtest;
};

static int
vtest_connect_socket(struct vn_instance *instance)
{
   struct sockaddr_un un;
   int sock;

   sock = socket(PF_UNIX, SOCK_STREAM, 0);
   if (sock < 0) {
      vn_log(instance, "failed to create a socket");
      return -1;
   }

   memset(&un, 0, sizeof(un));
   un.sun_family = AF_UNIX;
   memcpy(un.sun_path, VTEST_SOCKET_NAME, strlen(VTEST_SOCKET_NAME));

   if (connect(sock, (struct sockaddr *)&un, sizeof(un)) == -1) {
      vn_log(instance, "failed to connect to " VTEST_SOCKET_NAME ": %s",
             strerror(errno));
      close(sock);
      return -1;
   }

   return sock;
}

static void
vtest_write(struct vtest *vtest, const void *buf, size_t size)
{
   do {
      const ssize_t ret = write(vtest->sock_fd, buf, size);
      if (unlikely(ret < 0)) {
         vn_log(vtest->instance,
                "lost connection to rendering server on %zu write %zi %d",
                size, ret, errno);
         abort();
      }

      buf += ret;
      size -= ret;
   } while (size);
}

static void
vtest_read(struct vtest *vtest, void *buf, size_t size)
{
   do {
      const ssize_t ret = read(vtest->sock_fd, buf, size);
      if (unlikely(ret < 0)) {
         vn_log(vtest->instance,
                "lost connection to rendering server on %zu read %zi %d",
                size, ret, errno);
         abort();
      }

      buf += ret;
      size -= ret;
   } while (size);
}

static int
vtest_receive_fd(struct vtest *vtest)
{
   char cmsg_buf[CMSG_SPACE(sizeof(int))];
   char dummy;
   struct msghdr msg = {
      .msg_iov =
         &(struct iovec){
            .iov_base = &dummy,
            .iov_len = sizeof(dummy),
         },
      .msg_iovlen = 1,
      .msg_control = cmsg_buf,
      .msg_controllen = sizeof(cmsg_buf),
   };

   if (recvmsg(vtest->sock_fd, &msg, 0) < 0) {
      vn_log(vtest->instance, "recvmsg failed: %s", strerror(errno));
      abort();
   }

   struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
   if (!cmsg || cmsg->cmsg_level != SOL_SOCKET ||
       cmsg->cmsg_type != SCM_RIGHTS) {
      vn_log(vtest->instance, "invalid cmsghdr");
      abort();
   }

   return *((int *)CMSG_DATA(cmsg));
}

static void
vtest_vcmd_create_renderer(struct vtest *vtest, const char *name)
{
   const size_t size = strlen(name) + 1;

   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = size;
   vtest_hdr[VTEST_CMD_ID] = VCMD_CREATE_RENDERER;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, name, size);
}

static bool
vtest_vcmd_ping_protocol_version(struct vtest *vtest)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = VCMD_PING_PROTOCOL_VERSION_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_PING_PROTOCOL_VERSION;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));

   /* send a dummy busy wait to avoid blocking in vtest_read in case ping
    * protocol version is not supported
    */
   uint32_t vcmd_busy_wait[VCMD_BUSY_WAIT_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = VCMD_BUSY_WAIT_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_RESOURCE_BUSY_WAIT;
   vcmd_busy_wait[VCMD_BUSY_WAIT_HANDLE] = 0;
   vcmd_busy_wait[VCMD_BUSY_WAIT_FLAGS] = 0;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_busy_wait, sizeof(vcmd_busy_wait));

   uint32_t dummy;
   vtest_read(vtest, vtest_hdr, sizeof(vtest_hdr));
   if (vtest_hdr[VTEST_CMD_ID] == VCMD_PING_PROTOCOL_VERSION) {
      /* consume the dummy busy wait result */
      vtest_read(vtest, vtest_hdr, sizeof(vtest_hdr));
      assert(vtest_hdr[VTEST_CMD_ID] == VCMD_RESOURCE_BUSY_WAIT);
      vtest_read(vtest, &dummy, sizeof(dummy));
      return true;
   } else {
      /* no ping protocol version support */
      assert(vtest_hdr[VTEST_CMD_ID] == VCMD_RESOURCE_BUSY_WAIT);
      vtest_read(vtest, &dummy, sizeof(dummy));
      return false;
   }
}

static uint32_t
vtest_vcmd_protocol_version(struct vtest *vtest)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_protocol_version[VCMD_PROTOCOL_VERSION_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = VCMD_PROTOCOL_VERSION_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_PROTOCOL_VERSION;
   vcmd_protocol_version[VCMD_PROTOCOL_VERSION_VERSION] =
      VTEST_PROTOCOL_VERSION;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_protocol_version, sizeof(vcmd_protocol_version));

   vtest_read(vtest, vtest_hdr, sizeof(vtest_hdr));
   assert(vtest_hdr[VTEST_CMD_LEN] == VCMD_PROTOCOL_VERSION_SIZE);
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_PROTOCOL_VERSION);
   vtest_read(vtest, vcmd_protocol_version, sizeof(vcmd_protocol_version));

   return vcmd_protocol_version[VCMD_PROTOCOL_VERSION_VERSION];
}

static bool
vtest_vcmd_get_param(struct vtest *vtest,
                     enum vcmd_param param,
                     uint32_t *val)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_get_param[VCMD_GET_PARAM_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = VCMD_GET_PARAM_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_GET_PARAM;
   vcmd_get_param[VCMD_GET_PARAM_PARAM] = param;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_get_param, sizeof(vcmd_get_param));

   vtest_read(vtest, vtest_hdr, sizeof(vtest_hdr));
   assert(vtest_hdr[VTEST_CMD_LEN] == 2);
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_GET_PARAM);

   uint32_t resp[2];
   vtest_read(vtest, resp, sizeof(resp));

   if (resp[0])
      *val = resp[1];

   return resp[0];
}

static VkResult
vtest_vcmd_get_capset(struct vtest *vtest,
                      enum virgl_renderer_capset id,
                      uint32_t version,
                      void *capset,
                      size_t capset_size)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_get_capset[VCMD_GET_CAPSET_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = VCMD_GET_CAPSET_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_GET_CAPSET;
   vcmd_get_capset[VCMD_GET_CAPSET_ID] = id;
   vcmd_get_capset[VCMD_GET_CAPSET_VERSION] = version;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_get_capset, sizeof(vcmd_get_capset));

   vtest_read(vtest, vtest_hdr, sizeof(vtest_hdr));
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_GET_CAPSET);

   uint32_t valid;
   vtest_read(vtest, &valid, sizeof(valid));
   if (!valid) {
      vn_log(vtest->instance, "vtest server lacks vulkan support");
      return VK_ERROR_INCOMPATIBLE_DRIVER;
   }

   size_t read_size = (vtest_hdr[VTEST_CMD_LEN] - 1) * 4;
   if (capset_size >= read_size) {
      vtest_read(vtest, capset, read_size);
      memset(capset + read_size, 0, capset_size - read_size);
   } else {
      vtest_read(vtest, capset, capset_size);

      char temp[256];
      read_size -= capset_size;
      while (read_size) {
         const size_t temp_size = MIN2(read_size, ARRAY_SIZE(temp));
         vtest_read(vtest, temp, temp_size);
         read_size -= temp_size;
      }
   }

   return VK_SUCCESS;
}

static void
vtest_vcmd_context_init(struct vtest *vtest,
                        enum virgl_renderer_capset capset_id,
                        uint32_t capset_version)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_context_init[VCMD_CONTEXT_INIT_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = VCMD_CONTEXT_INIT_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_CONTEXT_INIT;
   vcmd_context_init[VCMD_CONTEXT_INIT_CAPSET_ID] = capset_id;
   vcmd_context_init[VCMD_CONTEXT_INIT_CAPSET_VERSION] = capset_version;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_context_init, sizeof(vcmd_context_init));
}

static uint32_t
vtest_vcmd_resource_create_blob(struct vtest *vtest,
                                enum vcmd_blob_type type,
                                uint32_t flags,
                                VkDeviceSize size,
                                vn_cs_object_id blob_id,
                                int *fd)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_SIZE];

   vtest_hdr[VTEST_CMD_LEN] = VCMD_RES_CREATE_BLOB_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_RESOURCE_CREATE_BLOB;

   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_TYPE] = type;
   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_FLAGS] = flags;
   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_SIZE_LO] = (uint32_t)size;
   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_SIZE_HI] =
      (uint32_t)(size >> 32);
   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_ID_LO] = (uint32_t)blob_id;
   vcmd_res_create_blob[VCMD_RES_CREATE_BLOB_ID_HI] =
      (uint32_t)(blob_id >> 32);

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_res_create_blob, sizeof(vcmd_res_create_blob));

   vtest_read(vtest, vtest_hdr, sizeof(vtest_hdr));
   assert(vtest_hdr[VTEST_CMD_LEN] == 1);
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_RESOURCE_CREATE_BLOB);

   uint32_t res_id;
   vtest_read(vtest, &res_id, sizeof(res_id));

   *fd = vtest_receive_fd(vtest);

   return res_id;
}

static void
vtest_vcmd_resource_unref(struct vtest *vtest, uint32_t res_id)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_res_unref[VCMD_RES_UNREF_SIZE];

   vtest_hdr[VTEST_CMD_LEN] = VCMD_RES_UNREF_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_RESOURCE_UNREF;
   vcmd_res_unref[VCMD_RES_UNREF_RES_HANDLE] = res_id;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_res_unref, sizeof(vcmd_res_unref));
}

static void
vtest_vcmd_transfer2(struct vtest *vtest,
                     uint32_t cmd_id,
                     uint32_t res_id,
                     uint32_t offset,
                     uint32_t size)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_transfer2[VCMD_TRANSFER2_HDR_SIZE];

   assert(cmd_id == VCMD_TRANSFER_PUT2 || cmd_id == VCMD_TRANSFER_GET2);

   vtest_hdr[VTEST_CMD_LEN] = VCMD_TRANSFER2_HDR_SIZE + (size + 3) / 4;
   vtest_hdr[VTEST_CMD_ID] = cmd_id;
   vcmd_transfer2[VCMD_TRANSFER2_RES_HANDLE] = res_id;
   vcmd_transfer2[VCMD_TRANSFER2_LEVEL] = 0;
   vcmd_transfer2[VCMD_TRANSFER2_X] = offset;
   vcmd_transfer2[VCMD_TRANSFER2_Y] = 0;
   vcmd_transfer2[VCMD_TRANSFER2_Z] = 0;
   vcmd_transfer2[VCMD_TRANSFER2_WIDTH] = size;
   vcmd_transfer2[VCMD_TRANSFER2_HEIGHT] = 1;
   vcmd_transfer2[VCMD_TRANSFER2_DEPTH] = 1;
   vcmd_transfer2[VCMD_TRANSFER2_DATA_SIZE] = size;
   vcmd_transfer2[VCMD_TRANSFER2_OFFSET] = offset;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_transfer2, sizeof(vcmd_transfer2));
}

static uint32_t
vtest_vcmd_sync_create(struct vtest *vtest, uint64_t point)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_sync_create[VCMD_SYNC_CREATE_SIZE];

   vtest_hdr[VTEST_CMD_LEN] = VCMD_SYNC_CREATE_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_SYNC_CREATE;

   vcmd_sync_create[VCMD_SYNC_CREATE_POINT_LO] = (uint32_t)point;
   vcmd_sync_create[VCMD_SYNC_CREATE_POINT_HI] = (uint32_t)(point >> 32);

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_sync_create, sizeof(vcmd_sync_create));

   vtest_read(vtest, vtest_hdr, sizeof(vtest_hdr));
   assert(vtest_hdr[VTEST_CMD_LEN] == 1);
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_SYNC_CREATE);

   uint32_t sync_id;
   vtest_read(vtest, &sync_id, sizeof(sync_id));

   return sync_id;
}

static void
vtest_vcmd_sync_unref(struct vtest *vtest, uint32_t sync_id)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_sync_unref[VCMD_SYNC_UNREF_SIZE];

   vtest_hdr[VTEST_CMD_LEN] = VCMD_SYNC_UNREF_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_SYNC_UNREF;
   vcmd_sync_unref[VCMD_SYNC_UNREF_ID] = sync_id;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_sync_unref, sizeof(vcmd_sync_unref));
}

static void
vtest_vcmd_sync_write(struct vtest *vtest, uint32_t sync_id, uint64_t point)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_sync_write[VCMD_SYNC_WRITE_SIZE];

   vtest_hdr[VTEST_CMD_LEN] = VCMD_SYNC_WRITE_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_SYNC_WRITE;

   vcmd_sync_write[VCMD_SYNC_WRITE_ID] = sync_id;
   vcmd_sync_write[VCMD_SYNC_WRITE_POINT_LO] = (uint32_t)point;
   vcmd_sync_write[VCMD_SYNC_WRITE_POINT_HI] = (uint32_t)(point >> 32);

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_sync_write, sizeof(vcmd_sync_write));
}

static uint64_t
vtest_vcmd_sync_read(struct vtest *vtest, uint32_t sync_id)
{
   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   uint32_t vcmd_sync_read[VCMD_SYNC_READ_SIZE];

   vtest_hdr[VTEST_CMD_LEN] = VCMD_SYNC_READ_SIZE;
   vtest_hdr[VTEST_CMD_ID] = VCMD_SYNC_READ;

   vcmd_sync_read[VCMD_SYNC_READ_ID] = sync_id;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, vcmd_sync_read, sizeof(vcmd_sync_read));

   vtest_read(vtest, vtest_hdr, sizeof(vtest_hdr));
   assert(vtest_hdr[VTEST_CMD_LEN] == 2);
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_SYNC_READ);

   uint64_t point;
   vtest_read(vtest, &point, sizeof(point));

   return point;
}

static int
vtest_vcmd_sync_wait(struct vtest *vtest,
                     uint32_t flags,
                     int poll_timeout,
                     struct vn_renderer_sync *const *syncs,
                     const uint64_t *points,
                     uint32_t count)
{
   const uint32_t timeout = poll_timeout >= 0 && poll_timeout <= INT32_MAX
                               ? poll_timeout
                               : UINT32_MAX;

   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = VCMD_SYNC_WAIT_SIZE(count);
   vtest_hdr[VTEST_CMD_ID] = VCMD_SYNC_WAIT;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, &flags, sizeof(flags));
   vtest_write(vtest, &timeout, sizeof(timeout));
   for (uint32_t i = 0; i < count; i++) {
      const uint64_t point = points ? points[i] : 1;
      const uint32_t sync[3] = {
         syncs[i]->sync_id,
         (uint32_t)point,
         (uint32_t)(point >> 32),
      };
      vtest_write(vtest, sync, sizeof(sync));
   }

   vtest_read(vtest, vtest_hdr, sizeof(vtest_hdr));
   assert(vtest_hdr[VTEST_CMD_LEN] == 0);
   assert(vtest_hdr[VTEST_CMD_ID] == VCMD_SYNC_WAIT);

   return vtest_receive_fd(vtest);
}

static uint32_t
submit_to_batches(const struct vn_renderer_submit *submit,
                  struct vcmd_submit_cmd2_batch batches[2],
                  uint32_t *batch_count)
{
   uint32_t cmd_size = 0;
   if (submit->cs) {
      const struct vn_cs_iovec *iovs = submit->cs->out.iovs;
      const uint32_t iov_count = submit->cs->out.iov_count;

      size_t iov_len = 0;
      for (uint32_t i = 0; i < iov_count; i++) {
         assert(iovs[i].iov_len % sizeof(uint32_t) == 0);
         iov_len += iovs[i].iov_len;
      }

      cmd_size = iov_len / sizeof(uint32_t);
   }

   *batch_count = 0;
   if (cmd_size || submit->sync_count)
      *batch_count += 1;
   if (submit->wait_cpu)
      *batch_count += 1;

   if (!*batch_count)
      return 0;

   uint32_t data_len =
      (sizeof(*batch_count) + sizeof(batches[0]) * *batch_count) /
      sizeof(uint32_t);

   struct vcmd_submit_cmd2_batch *batch = &batches[0];
   if (cmd_size || submit->sync_count) {
      batch->flags = VCMD_SUBMIT_CMD2_FLAG_SYNC_QUEUE;

      batch->cmd_offset = data_len;
      batch->cmd_size = cmd_size;
      data_len += cmd_size;

      batch->sync_offset = data_len;
      batch->sync_count = submit->sync_count;
      batch->sync_queue_index = submit->sync_queue_index;
      batch->sync_queue_id = submit->sync_queue_id;
      data_len += submit->sync_count * 3;

      batch++;
   }

   if (submit->wait_cpu) {
      memset(batch, 0, sizeof(*batch));
      batch->sync_offset = data_len;
      batch->sync_count = 1;
      data_len += 3;
   }

   return data_len;
}

static uint64_t
vtest_vcmd_submit_cmd2(struct vtest *vtest,
                       const struct vn_renderer_submit *submit)
{
   struct vcmd_submit_cmd2_batch batches[2];
   uint32_t batch_count;
   const uint32_t data_len = submit_to_batches(submit, batches, &batch_count);

   if (!data_len)
      return 0;

   uint32_t vtest_hdr[VTEST_HDR_SIZE];
   vtest_hdr[VTEST_CMD_LEN] = data_len;
   vtest_hdr[VTEST_CMD_ID] = VCMD_SUBMIT_CMD2;

   vtest_write(vtest, vtest_hdr, sizeof(vtest_hdr));
   vtest_write(vtest, &batch_count, sizeof(batch_count));
   vtest_write(vtest, batches, sizeof(batches[0]) * batch_count);

   if (submit->cs) {
      const struct vn_cs_iovec *iovs = submit->cs->out.iovs;
      const uint32_t iov_count = submit->cs->out.iov_count;
      for (uint32_t i = 0; i < iov_count; i++)
         vtest_write(vtest, iovs[i].iov_base, iovs[i].iov_len);
   }

   for (uint32_t i = 0; i < submit->sync_count; i++) {
      const uint64_t point = submit->sync_points ? submit->sync_points[i] : 1;
      const uint32_t sync[3] = {
         submit->syncs[i]->sync_id,
         (uint32_t)point,
         (uint32_t)(point >> 32),
      };
      vtest_write(vtest, sync, sizeof(sync));
   }

   uint64_t cpu_point = 0;
   if (submit->wait_cpu) {
      cpu_point = ++vtest->cpu_point;
      const uint32_t sync[3] = {
         vtest->cpu_sync->sync_id,
         (uint32_t)cpu_point,
         (uint32_t)(cpu_point >> 32),
      };
      vtest_write(vtest, sync, sizeof(sync));
   }

   return cpu_point;
}

/*
 * In virtio-gpu, commands are queued and dispatched in order.  Depending on
 * where the commands are dispatched to, they may overlap or execute out of
 * order.
 *
 * Conventionally, non-fenced commands are retired in dispatch order with
 * respect to each other.  When a non-fenced command is retired, its execution
 * might still be ongoing.  Fenced commands also are retired in dispatch order
 * with respect to each other.  When a fenced command is retired, its
 * execution has completed.
 *
 * A more flexible view is that each command is dispatched to the context
 * specified by its ctx_id.  The command is executed on CPU first in the
 * context, and may trigger GPU execution.  When the command is not fenced, it
 * is retired after CPU execution.  When the command is fenced, it is retired
 * after GPU execution.
 *
 * vtest is similar, except ctx_id is implied.  Depending on the command, it
 * can be 0 or some unique id generated by VCMD_CONTEXT_INIT.
 */
static uint64_t
vtest_vcmd_roundtrip(struct vtest *vtest)
{
   const struct vn_renderer_submit submit = { .wait_cpu = true };
   return vtest_vcmd_submit_cmd2(vtest, &submit);
}

static VkResult
sync_wait_fd_poll(int fd, int poll_timeout)
{
   struct pollfd pollfd = {
      .fd = fd,
      .events = POLLIN,
   };
   const int ret = poll(&pollfd, 1, poll_timeout);

   if (ret < 0)
      return VK_ERROR_DEVICE_LOST;

   return ret == 1 && (pollfd.revents & POLLIN) ? VK_SUCCESS : VK_TIMEOUT;
}

static void
vtest_wait_cpu_point(struct vtest *vtest, uint64_t cpu_point)
{
   mtx_lock(&vtest->sock_mutex);
   const int fd =
      vtest_vcmd_sync_wait(vtest, 0, -1, &vtest->cpu_sync, &cpu_point, 1);
   mtx_unlock(&vtest->sock_mutex);

   sync_wait_fd_poll(fd, -1);
   close(fd);
}

static VkResult
vtest_sync_read(struct vn_renderer_sync *_sync, uint64_t *point)
{
   struct vtest_sync *sync = (struct vtest_sync *)_sync;
   struct vtest *vtest = sync->vtest;

   mtx_lock(&vtest->sock_mutex);
   *point = vtest_vcmd_sync_read(vtest, sync->base.sync_id);
   mtx_unlock(&vtest->sock_mutex);

   return VK_SUCCESS;
}

static VkResult
vtest_sync_write(struct vn_renderer_sync *_sync, uint64_t point)
{
   struct vtest_sync *sync = (struct vtest_sync *)_sync;
   struct vtest *vtest = sync->vtest;

   mtx_lock(&vtest->sock_mutex);
   vtest_vcmd_sync_write(vtest, sync->base.sync_id, point);
   mtx_unlock(&vtest->sock_mutex);

   return VK_SUCCESS;
}

static VkResult
vtest_sync_reset(struct vn_renderer_sync *_sync, uint64_t initial_point)
{
   struct vtest_sync *sync = (struct vtest_sync *)_sync;
   struct vtest *vtest = sync->vtest;

   mtx_lock(&vtest->sock_mutex);
   vtest_vcmd_sync_write(vtest, sync->base.sync_id, initial_point);
   mtx_unlock(&vtest->sock_mutex);

   return VK_SUCCESS;
}

static void
vtest_sync_release(struct vn_renderer_sync *_sync)
{
   struct vtest_sync *sync = (struct vtest_sync *)_sync;
   struct vtest *vtest = sync->vtest;

   mtx_lock(&vtest->sock_mutex);
   vtest_vcmd_sync_unref(vtest, sync->base.sync_id);
   mtx_unlock(&vtest->sock_mutex);

   sync->base.sync_id = 0;
}

static VkResult
vtest_sync_init(struct vn_renderer_sync *_sync,
                uint64_t initial_point,
                bool shareable,
                bool binary)
{
   struct vtest_sync *sync = (struct vtest_sync *)_sync;
   struct vtest *vtest = sync->vtest;

   mtx_lock(&vtest->sock_mutex);
   sync->base.sync_id = vtest_vcmd_sync_create(vtest, initial_point);
   mtx_unlock(&vtest->sock_mutex);

   return VK_SUCCESS;
}

static void
vtest_sync_destroy(struct vn_renderer_sync *_sync,
                   const VkAllocationCallbacks *alloc)
{
   struct vtest_sync *sync = (struct vtest_sync *)_sync;

   if (sync->base.sync_id)
      vtest_sync_release(&sync->base);

   vk_free(alloc, sync);
}

static struct vn_renderer_sync *
vtest_sync_create(struct vn_renderer *renderer,
                  const VkAllocationCallbacks *alloc,
                  VkSystemAllocationScope alloc_scope)
{
   struct vtest *vtest = (struct vtest *)renderer;

   struct vtest_sync *sync =
      vk_zalloc(alloc, sizeof(*sync), VN_DEFAULT_ALIGN, alloc_scope);
   if (!sync)
      return NULL;

   sync->vtest = vtest;

   sync->base.destroy = vtest_sync_destroy;
   sync->base.init = vtest_sync_init;
   sync->base.release = vtest_sync_release;
   sync->base.reset = vtest_sync_reset;
   sync->base.write = vtest_sync_write;
   sync->base.read = vtest_sync_read;

   return &sync->base;
}

static void
vtest_bo_invalidate(struct vn_renderer_bo *_bo,
                    VkDeviceSize offset,
                    VkDeviceSize size)
{
   struct vtest_bo *bo = (struct vtest_bo *)_bo;
   struct vtest *vtest = bo->vtest;

   mtx_lock(&vtest->sock_mutex);
   vtest_vcmd_transfer2(vtest, VCMD_TRANSFER_GET2, bo->base.res_id, offset,
                        size);
   const uint64_t cpu_point = vtest_vcmd_roundtrip(vtest);
   mtx_unlock(&vtest->sock_mutex);

   vtest_wait_cpu_point(vtest, cpu_point);
}

static void
vtest_bo_flush(struct vn_renderer_bo *_bo,
               VkDeviceSize offset,
               VkDeviceSize size)
{
   struct vtest_bo *bo = (struct vtest_bo *)_bo;
   struct vtest *vtest = bo->vtest;

   mtx_lock(&vtest->sock_mutex);
   vtest_vcmd_transfer2(vtest, VCMD_TRANSFER_PUT2, bo->base.res_id, offset,
                        size);
   mtx_unlock(&vtest->sock_mutex);
}

static void *
vtest_bo_map(struct vn_renderer_bo *_bo)
{
   struct vtest_bo *bo = (struct vtest_bo *)_bo;

   if (bo->res_ptr)
      return bo->res_ptr;

   /* XXX
    *
    * This assumes mmap(dmabuf) is equivalent to vkMapMemory(VkDeviceMemory),
    * which is guaranteed by VCMD_PARAM_HOST_COHERENT_DMABUF_BLOB.  But there
    * is no such thing as coherent dmabuf and we know the server is lying.
    *
    * When bo->is_dmabuf is false, this is incorrect when
    * VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is set.
    */
   void *ptr =
      mmap(NULL, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED, bo->res_fd, 0);
   if (ptr == MAP_FAILED)
      return NULL;

   bo->res_ptr = ptr;

   return bo->res_ptr;
}

static int
vtest_bo_export_dmabuf(struct vn_renderer_bo *_bo)
{
   struct vtest_bo *bo = (struct vtest_bo *)_bo;
   return dup(bo->res_fd);
}

static VkResult
vtest_bo_init_memory(struct vn_renderer_bo *_bo,
                     VkDeviceSize size,
                     vn_cs_object_id obj_id,
                     VkMemoryPropertyFlags flags,
                     VkExternalMemoryHandleTypeFlagBits external)
{
   struct vtest_bo *bo = (struct vtest_bo *)_bo;
   struct vtest *vtest = bo->vtest;

   enum vcmd_blob_type blob_type = vtest->coherent_dmabuf_blob
                                      ? VCMD_BLOB_TYPE_HOST3D
                                      : VCMD_BLOB_TYPE_HOST3D_GUEST;

   uint32_t blob_flags = 0;
   if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      blob_flags |= VCMD_BLOB_FLAG_MAPPABLE;
   if (external)
      blob_flags |= VCMD_BLOB_FLAG_SHAREABLE;
   if (external == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)
      blob_flags |= VCMD_BLOB_FLAG_CROSS_DEVICE;

   mtx_lock(&vtest->sock_mutex);
   bo->base.res_id = vtest_vcmd_resource_create_blob(
      vtest, blob_type, blob_flags, size, obj_id, &bo->res_fd);
   mtx_unlock(&vtest->sock_mutex);

   bo->size = size;
   bo->is_dmabuf = vtest->coherent_dmabuf_blob;

   if (blob_flags & VCMD_BLOB_FLAG_MAPPABLE)
      bo->base.map = vtest_bo_map;

   if (bo->is_dmabuf) {
      bo->base.export_dmabuf = vtest_bo_export_dmabuf;
   } else {
      bo->base.flush = vtest_bo_flush;
      bo->base.invalidate = vtest_bo_invalidate;
   }

   return VK_SUCCESS;
}

static VkResult
vtest_bo_init_shm(struct vn_renderer_bo *_bo, VkDeviceSize size)
{
   struct vtest_bo *bo = (struct vtest_bo *)_bo;
   struct vtest *vtest = bo->vtest;

   mtx_lock(&vtest->sock_mutex);
   bo->base.res_id = vtest_vcmd_resource_create_blob(
      vtest, VCMD_BLOB_TYPE_GUEST, VCMD_BLOB_FLAG_MAPPABLE, size, 0,
      &bo->res_fd);
   mtx_unlock(&vtest->sock_mutex);

   bo->size = size;
   bo->base.map = vtest_bo_map;

   return VK_SUCCESS;
}

static void
vtest_bo_destroy(struct vn_renderer_bo *_bo,
                 const VkAllocationCallbacks *alloc)
{
   struct vtest_bo *bo = (struct vtest_bo *)_bo;
   struct vtest *vtest = bo->vtest;

   if (bo->base.res_id) {
      if (bo->res_ptr)
         munmap(bo->res_ptr, bo->size);
      close(bo->res_fd);

      mtx_lock(&vtest->sock_mutex);
      vtest_vcmd_resource_unref(vtest, bo->base.res_id);
      mtx_unlock(&vtest->sock_mutex);
   }

   vk_free(alloc, bo);
}

static struct vn_renderer_bo *
vtest_bo_create(struct vn_renderer *renderer,
                const VkAllocationCallbacks *alloc,
                VkSystemAllocationScope alloc_scope)
{
   struct vtest *vtest = (struct vtest *)renderer;

   struct vtest_bo *bo =
      vk_zalloc(alloc, sizeof(*bo), VN_DEFAULT_ALIGN, alloc_scope);
   if (!bo)
      return NULL;

   bo->vtest = vtest;
   bo->res_fd = -1;

   bo->base.destroy = vtest_bo_destroy;
   bo->base.init_shm = vtest_bo_init_shm;
   bo->base.init_memory = vtest_bo_init_memory;

   return &bo->base;
}

static int
timeout_to_poll_timeout(uint64_t timeout)
{
   const uint64_t ns_per_ms = 1000000;
   const uint64_t ms = (timeout + ns_per_ms - 1) / ns_per_ms;
   if (!ms && timeout)
      return -1;
   return ms <= INT_MAX ? ms : -1;
}

static VkResult
vtest_wait(struct vn_renderer *renderer,
           struct vn_renderer_sync *const *syncs,
           const uint64_t *points,
           uint32_t count,
           bool wait_any,
           uint64_t timeout)
{
   struct vtest *vtest = (struct vtest *)renderer;

   const uint32_t flags = wait_any ? VCMD_SYNC_WAIT_FLAG_ANY : 0;
   const int poll_timeout = timeout_to_poll_timeout(timeout);

   /*
    * vtest_vcmd_sync_wait (and some other sync commands) is executed after
    * all prior commands are dispatched.  That is far from ideal.
    *
    * In virtio-gpu, a drm_syncobj wait ioctl is executed immediately.  It
    * works because it uses virtio-gpu interrupts as a side channel.  vtest
    * needs a side channel to perform well.
    *
    * virtio-gpu or vtest, we should also set up a 1-byte coherent memory that
    * is set to non-zero by GPU after the syncs signal.  That would allow us
    * to do a quick check (or spin a bit) before waiting.
    */
   mtx_lock(&vtest->sock_mutex);
   const int fd =
      vtest_vcmd_sync_wait(vtest, flags, poll_timeout, syncs, points, count);
   mtx_unlock(&vtest->sock_mutex);

   VkResult result = sync_wait_fd_poll(fd, poll_timeout);
   close(fd);

   return result;
}

static VkResult
vtest_submit(struct vn_renderer *renderer,
             const struct vn_renderer_submit *submit)
{
   struct vtest *vtest = (struct vtest *)renderer;

   mtx_lock(&vtest->sock_mutex);
   const uint64_t cpu_point = vtest_vcmd_submit_cmd2(vtest, submit);
   mtx_unlock(&vtest->sock_mutex);

   if (cpu_point)
      vtest_wait_cpu_point(vtest, cpu_point);

   return VK_SUCCESS;
}

static void
vtest_get_info(struct vn_renderer *renderer, struct vn_renderer_info *info)
{
   struct vtest *vtest = (struct vtest *)renderer;

   memset(info, 0, sizeof(*info));

   info->supports_dmabuf = vtest->coherent_dmabuf_blob;
   info->sync_queue_count = vtest->sync_queue_count;

   info->wire_format_version = vtest->capset.wire_format_version;
   info->vk_xml_version = vtest->capset.vk_xml_version;
   info->vk_ext_command_serialization_spec_version =
      vtest->capset.vk_ext_command_serialization_spec_version;
   info->vk_mesa_venus_protocol_spec_version =
      vtest->capset.vk_mesa_venus_protocol_spec_version;
}

static void
vtest_destroy(struct vn_renderer *renderer,
              const VkAllocationCallbacks *alloc)
{
   struct vtest *vtest = (struct vtest *)renderer;

   if (vtest->cpu_sync)
      vtest_sync_destroy(vtest->cpu_sync, alloc);

   if (vtest->sock_fd >= 0) {
      shutdown(vtest->sock_fd, SHUT_RDWR);
      close(vtest->sock_fd);
   }

   mtx_destroy(&vtest->sock_mutex);

   vk_free(alloc, vtest);
}

static VkResult
vtest_init_sync(struct vtest *vtest, const VkAllocationCallbacks *alloc)
{
   uint32_t count;
   if (!vtest_vcmd_get_param(vtest, VCMD_PARAM_SYNC_QUEUE_COUNT, &count) ||
       !count) {
      vn_log(vtest->instance, "no sync support");
      return VK_ERROR_INCOMPATIBLE_DRIVER;
   }

   struct vn_renderer_sync *cpu_sync = vtest_sync_create(
      &vtest->base, alloc, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!cpu_sync)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   VkResult result =
      vtest_sync_init(cpu_sync, vtest->cpu_point, false, false);
   if (result != VK_SUCCESS) {
      vtest_sync_destroy(cpu_sync, alloc);
      return result;
   }

   vtest->sync_queue_count = count;
   vtest->cpu_sync = cpu_sync;

   return VK_SUCCESS;
}

static VkResult
vtest_init_context(struct vtest *vtest)
{
   const enum virgl_renderer_capset id = VIRGL_RENDERER_CAPSET_VENUS;
   const uint32_t version = 1;

   uint32_t val;
   if (vtest_vcmd_get_param(vtest, VCMD_PARAM_HOST_COHERENT_DMABUF_BLOB,
                            &val) &&
       val)
      vtest->coherent_dmabuf_blob = true;
   else
      vn_log(vtest->instance, "no coherent memory support");

   VkResult result = vtest_vcmd_get_capset(vtest, id, version, &vtest->capset,
                                           sizeof(vtest->capset));
   if (result != VK_SUCCESS)
      return result;

   vtest_vcmd_context_init(vtest, id, version);

   return VK_SUCCESS;
}

static VkResult
vtest_init_renderer(struct vtest *vtest)
{
   const char *name = util_get_process_name();
   vtest_vcmd_create_renderer(vtest, name);

   if (vtest_vcmd_ping_protocol_version(vtest))
      vtest->protocol_version = vtest_vcmd_protocol_version(vtest);

   if (vtest->protocol_version < 3) {
      vn_log(vtest->instance, "vtest protocol version (%d) too old",
             vtest->protocol_version);
      return VK_ERROR_INCOMPATIBLE_DRIVER;
   }

   return VK_SUCCESS;
}

VkResult
vn_renderer_create_vtest(struct vn_instance *instance,
                         const VkAllocationCallbacks *alloc,
                         struct vn_renderer **renderer)
{
   struct vtest *vtest = vk_zalloc(alloc, sizeof(*vtest), VN_DEFAULT_ALIGN,
                                   VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!vtest)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   vtest->instance = instance;
   vtest->sock_fd = -1;

   mtx_init(&vtest->sock_mutex, mtx_plain);
   vtest->sock_fd = vtest_connect_socket(instance);
   if (vtest->sock_fd < 0) {
      vtest_destroy(&vtest->base, alloc);
      return VK_ERROR_INCOMPATIBLE_DRIVER;
   }

   VkResult result = vtest_init_renderer(vtest);
   if (result != VK_SUCCESS) {
      vtest_destroy(&vtest->base, alloc);
      return result;
   }

   result = vtest_init_context(vtest);
   if (result != VK_SUCCESS) {
      vtest_destroy(&vtest->base, alloc);
      return result;
   }

   result = vtest_init_sync(vtest, alloc);
   if (result != VK_SUCCESS) {
      vtest_destroy(&vtest->base, alloc);
      return result;
   }

   vtest->base.destroy = vtest_destroy;
   vtest->base.get_info = vtest_get_info;
   vtest->base.bo_create = vtest_bo_create;
   vtest->base.sync_create = vtest_sync_create;
   vtest->base.submit = vtest_submit;
   vtest->base.wait = vtest_wait;

   *renderer = &vtest->base;

   return VK_SUCCESS;
}
