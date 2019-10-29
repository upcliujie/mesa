/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on virgl which is:
 * Copyright 2014, 2015 Red Hat.
 */

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "util/u_process.h"
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

static void
vtest_destroy(struct vn_renderer *renderer,
              const VkAllocationCallbacks *alloc)
{
   struct vtest *vtest = (struct vtest *)renderer;

   if (vtest->sock_fd >= 0) {
      shutdown(vtest->sock_fd, SHUT_RDWR);
      close(vtest->sock_fd);
   }

   mtx_destroy(&vtest->sock_mutex);

   vk_free(alloc, vtest);
}

static VkResult
vtest_init_renderer(struct vtest *vtest)
{
   const char *name = util_get_process_name();
   vtest_vcmd_create_renderer(vtest, name);

   if (vtest_vcmd_ping_protocol_version(vtest))
      vtest->protocol_version = vtest_vcmd_protocol_version(vtest);

   if (vtest->protocol_version < 2) {
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

   vtest->base.destroy = vtest_destroy;

   *renderer = &vtest->base;

   return VK_SUCCESS;
}
