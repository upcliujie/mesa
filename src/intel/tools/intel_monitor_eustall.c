/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "intel_monitor_eustall.h"

#include <poll.h>

#include "drm-uapi/xe_drm.h"
#include "util/u_qsort.h"

static bool
oa_stream_ready(int fd) {
   struct pollfd pfd;

   pfd.fd = fd;
   pfd.events = POLLIN;
   pfd.revents = 0;

   if (poll(&pfd, 1, 0) < 0) {
      fprintf(stderr, "IMON: Error polling OA stream\n");
      return false;
   }

   if (!(pfd.revents & POLLIN))
      return false;

   return true;
}

/* Initialize eustall_cfg and enable eu stall profiling by
 * opening stream with KMD. Return eustall_cfg.
 */
struct eustall_config*
eustall_setup(int drm_fd, struct intel_device_info *devinfo,
              uint64_t poll_period_ns)
{
   struct eustall_config *eustall_cfg = malloc(sizeof(struct eustall_config));
   eustall_cfg->gpu_buf_size = 128 * 1024, /* min buffer supported: 128 KB */
   eustall_cfg->sample_rate = 1,           /* fastest GPU sampling: 251 cycles/sample */
   eustall_cfg->min_event_count = 1,       /* min records to trigger data flush */
   eustall_cfg->drm_fd = drm_fd;
   eustall_cfg->fd = -1;
   eustall_cfg->devinfo = devinfo;
   eustall_cfg->poll_period_ns = poll_period_ns,

   eustall_cfg->result.accumulator =
      _mesa_hash_table_create(NULL,
                              _mesa_hash_uint,
                              _mesa_key_uint_equal);

   /* Size buffer for minimum HW buffer size (128 KB) + several headers */
   eustall_cfg->buf_len = 128 * 1024 +
                          4 * sizeof(struct drm_xe_eu_stall_data_header);
   eustall_cfg->buf = malloc(eustall_cfg->buf_len);

   return eustall_cfg;
}

/* Sample all eustall data via KMD stream. Open stream on first call.
 */
bool
eustall_sample(struct eustall_config *eustall_cfg)
{
   int bytes_read, bytes_parsed;

   if (eustall_cfg->fd < 0) {
      eustall_cfg->fd =
         intel_perf_eustall_stream_open(eustall_cfg->devinfo,
                                        eustall_cfg->drm_fd,
                                        eustall_cfg->gpu_buf_size,
                                        eustall_cfg->poll_period_ns,
                                        eustall_cfg->sample_rate,
                                        eustall_cfg->min_event_count,
                                        true /* enable stream immediately */);
      if (eustall_cfg->fd < 0) {
         fprintf(stderr, "IMON: ERROR encountered while opening "
                         "eustall stream. err=%i\n", eustall_cfg->fd);
         return false;
      }
      fprintf(stderr, "IMON: intel_perf_eustall_stream_open = %i\n",
              eustall_cfg->fd);
      return true;
   }

   while (oa_stream_ready(eustall_cfg->fd)) {
      bytes_read =
         intel_perf_eustall_stream_read_samples(eustall_cfg->devinfo,
                                                eustall_cfg->fd,
                                                eustall_cfg->buf,
                                                eustall_cfg->buf_len);
      if (bytes_read <= 0) {
         if (bytes_read < 0)
            fprintf(stderr, "IMON: read_samples returned err=%i", bytes_read);
         break;
      }

      bytes_parsed =
         intel_perf_query_result_eustall_accumulate(&eustall_cfg->result,
                                                    eustall_cfg->buf,
                                                    eustall_cfg->buf + bytes_read);

      if (bytes_parsed != bytes_read) {
         fprintf(stderr, "IMON: ERROR while parsing eu stall stream data. "
                         "Not able to parse all data.\n");
         return false;
      }
   }
   
   return true;
}

static int
compare_ip_addr(const void *a, const void *b)
{
   const uint32_t *val_a = a;
   const uint32_t *val_b = b;
   return (int)(*val_a - *val_b);
}

/* Write all previously collected results to fd. Clear results.
 */
void
eustall_dump_results(struct eustall_config *eustall_cfg, FILE *file)
{
   struct hash_table *accumulator = eustall_cfg->result.accumulator;
   uint32_t *ip_addr_keys = malloc(accumulator->size * sizeof(uint32_t));
   int i = 0;

   /* Sort keys so ip_addr appear in order */
   hash_table_foreach(accumulator, entry) {
      struct intel_perf_query_eustall_event* data =
         (struct intel_perf_query_eustall_event*)entry->data;
      ip_addr_keys[i++] = data->ip_addr;
   }
   qsort(ip_addr_keys, accumulator->entries, sizeof(uint32_t), compare_ip_addr);

   fprintf(file, "offset,tdr_count,other_count,control_count,pipestall_count,"
                 "send_count,dist_acc_count,sbid_count,sync_count,"
                 "inst_fetch_count,active_count,sum\n");

   for (i = 0; i < accumulator->entries; i++) {
      struct hash_entry *entry =
         _mesa_hash_table_search(accumulator, ip_addr_keys + i);
      struct intel_perf_query_eustall_event *data =
         (struct intel_perf_query_eustall_event*)entry->data;

      uint32_t ip_addr = data->ip_addr << 3;
      uint64_t sum = data->tdr_count + data->other_count +
         data->control_count + data->pipestall_count + data->send_count +
         data->dist_acc_count + data->sbid_count + data->sync_count +
         data->inst_fetch_count + data->active_count;

      fprintf(file, "0x%08x,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
         ip_addr, data->tdr_count, data->other_count, data->control_count,
         data->pipestall_count, data->send_count, data->dist_acc_count,
         data->sbid_count, data->sync_count, data->inst_fetch_count,
         data->active_count, sum);

         free(data);
         _mesa_hash_table_remove(accumulator, entry);
   }
   free(ip_addr_keys);
}

static void
delete_entry(struct hash_entry *entry)
{
   free(entry->data);
}

/* Close eustall stream and deconstruct eustall cfg.
 */
void
eustall_close(struct eustall_config *eustall_cfg)
{
   _mesa_hash_table_destroy(eustall_cfg->result.accumulator, delete_entry);
   eustall_cfg->result.accumulator = NULL;

   close(eustall_cfg->fd);
   eustall_cfg->fd = -1;

   free(eustall_cfg->buf);
   eustall_cfg->buf = NULL;

   free(eustall_cfg);
}
