/*
 * Copyright (c) 2017-2019 Lima Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

/**
 * Expose Mali4xx HW perf counters.
 *
 * We also have code to fake support for occlusion queries.
 * Since we expose support for GL 2.0, we have to expose occlusion queries,
 * but the spec allows you to expose 0 query counter bits, so we just return 0
 * as the result of all our queries.
 */

#include "util/u_debug.h"

#include "lima_context.h"
#include "lima_event_list.h"
#include "lima_job.h"
#include "lima_screen.h"

#include <drm-uapi/lima_drm.h>
#include <xf86drm.h>

struct lima_query
{
   unsigned num_queries;
   struct lima_hwperfmon *hwperfmon;
};

int
lima_get_driver_query_group_info(struct pipe_screen *pscreen, unsigned index,
                                 struct pipe_driver_query_group_info *info)
{
   struct lima_screen *screen = lima_screen(pscreen);

   if (!screen->has_perfmon_ioctl)
      return 0;

   if (!info)
      return lima_query_groups_num;

   if (index >= lima_query_groups_num)
      return 0;

   info->name = lima_group_data[index].name;
   info->max_active_queries = lima_group_data[index].max_active_queries;
   info->num_queries = lima_group_data[index].num_queries;
   return 1;
}

int lima_get_driver_query_info(struct pipe_screen *pscreen, unsigned index,
                               struct pipe_driver_query_info *info)
{
   struct lima_screen *screen = lima_screen(pscreen);

   if (!screen->has_perfmon_ioctl)
      return 0;

   if (!info)
      return lima_queries_num;

   if (index >= lima_queries_num)
      return 0;

   info->group_id = lima_query_data[index].group_id;
   info->name = lima_query_data[index].name;
   info->query_type = PIPE_QUERY_DRIVER_SPECIFIC + index;
   info->result_type = PIPE_DRIVER_QUERY_RESULT_TYPE_CUMULATIVE;
   info->type = PIPE_DRIVER_QUERY_TYPE_UINT64;
   info->flags = PIPE_DRIVER_QUERY_FLAG_BATCH;
   return 1;
}

static struct pipe_query *
lima_create_batch_query(struct pipe_context *ctx, unsigned num_queries,
                        unsigned *query_types)
{
   struct lima_query *query = calloc(1, sizeof(*query));
   struct lima_hwperfmon *hwperfmon;
   unsigned nhwqueries = 0;

   if (!query)
      return NULL;

   for (int i = 0; i < num_queries; i++) {
      if (query_types[i] >= PIPE_QUERY_DRIVER_SPECIFIC)
         nhwqueries++;
   }

   /* We can't mix HW and non-HW queries. */
   if (nhwqueries && nhwqueries != num_queries)
      goto err_free_query;

   if (!nhwqueries)
      return (struct pipe_query *)query;

   hwperfmon = calloc(1, sizeof(*hwperfmon));
   if (!hwperfmon)
      goto err_free_query;

   /* To validate that we are not adding more events per
    * group than what is supported */
   int count_groups[lima_query_groups_num] = { 0 };

   for (int i = 0; i < num_queries; i++) {
      unsigned index = query_types[i] - PIPE_QUERY_DRIVER_SPECIFIC;

      /* events that have "event_delta" need to have it OR'ed with
       * the event number for the final event. */
      uint8_t event = (lima_query_data[index].event_delta |
                       lima_query_data[index].event);
      enum lima_query_groups group = lima_query_data[index].group_id;

      hwperfmon->groups[i] = group;
      hwperfmon->events[i] = event;

      count_groups[group]++;
      if (count_groups[group] > 2)
         goto err_free_query;
   }
   query->hwperfmon = hwperfmon;
   query->num_queries = num_queries;

   /* Note that struct pipe_query isn't actually defined anywhere. */
   return (struct pipe_query *)query;

err_free_query:
   free(query);

   return NULL;
}

static struct pipe_query *
lima_create_query(struct pipe_context *ctx, unsigned query_type, unsigned index)
{
   return lima_create_batch_query(ctx, 1, &query_type);
}

static void
lima_destroy_query(struct pipe_context *pctx, struct pipe_query *pquery)
{
   struct lima_screen *screen = lima_screen(pctx->screen);
   struct lima_query *query = (struct lima_query *)pquery;

   if (query->hwperfmon && query->hwperfmon->id) {
      if (query->hwperfmon->id) {
         struct drm_lima_perfmon_destroy req = { };

         req.id = query->hwperfmon->id;
         drmIoctl(screen->fd, DRM_IOCTL_LIMA_PERFMON_DESTROY, &req);
      }

      free(query->hwperfmon);
   }

   free(query);
}

static bool
lima_begin_query(struct pipe_context *pctx, struct pipe_query *pquery)
{
   struct lima_query *query = (struct lima_query *)pquery;
   struct lima_context *ctx = lima_context(pctx);
   struct lima_screen *screen = lima_screen(pctx->screen);
   struct drm_lima_perfmon_create req = { };
   int ret;

   if (!query->hwperfmon)
      return true;

   /* Only one perfmon can be activated per context. */
   if (ctx->perfmon)
      return false;

   /* Reset the counters by destroying the previously allocated perfmon */
   if (query->hwperfmon->id) {
      struct drm_lima_perfmon_destroy destroyreq = { };

      destroyreq.id = query->hwperfmon->id;
      drmIoctl(screen->fd, DRM_IOCTL_LIMA_PERFMON_DESTROY, &destroyreq);
   }

   for (int i = 0; i < query->num_queries; i++) {
      req.groups[i] = query->hwperfmon->groups[i];
      req.events[i] = query->hwperfmon->events[i];
   }

   req.ncounters = query->num_queries;
   ret = drmIoctl(screen->fd, DRM_IOCTL_LIMA_PERFMON_CREATE, &req);
   if (ret)
      return false;

   query->hwperfmon->id = req.id;

   /* Make sure all pendings jobs are flushed before activating the
    * perfmon.
    */
   lima_flush(ctx);
   ctx->perfmon = query->hwperfmon;
   return true;
}

static bool
lima_end_query(struct pipe_context *pctx, struct pipe_query *pquery)
{
   struct lima_query *query = (struct lima_query *)pquery;
   struct lima_context *ctx = lima_context(pctx);

   if (!query->hwperfmon)
      return true;

   if (ctx->perfmon != query->hwperfmon)
      return false;

   /* Make sure all pendings jobs are flushed before deactivating the
    * perfmon.
    */
   lima_flush(ctx);
   ctx->perfmon = NULL;
   return true;
}

static bool
lima_get_query_result(struct pipe_context *pctx, struct pipe_query *pquery,
                      bool wait, union pipe_query_result *vresult)
{
   struct lima_screen *screen = lima_screen(pctx->screen);
   struct lima_context *ctx = lima_context(pctx);
   struct lima_job *job = lima_job_get(ctx);
   struct lima_query *query = (struct lima_query *)pquery;
   struct drm_lima_perfmon_get_values req;
   int ret;

   if (!query->hwperfmon) {
      vresult->u64 = 0;
      return true;
   }

   if (!lima_job_wait(job, LIMA_PIPE_GP, PIPE_TIMEOUT_INFINITE))
      return false;
   if (!lima_job_wait(job, LIMA_PIPE_PP, PIPE_TIMEOUT_INFINITE))
      return false;

   req.id = query->hwperfmon->id;
   req.values_ptr = (uintptr_t)query->hwperfmon->counters;
   ret = drmIoctl(screen->fd, DRM_IOCTL_LIMA_PERFMON_GET_VALUES, &req);
   if (ret)
      return false;

   for (int i = 0; i < query->num_queries; i++)
      vresult->batch[i].u64 = query->hwperfmon->counters[i];

   return true;
}

static void
lima_set_active_query_state(struct pipe_context *pipe, bool enable)
{

}

void
lima_query_init(struct lima_context *ctx)
{
   ctx->base.create_query = lima_create_query;
   ctx->base.create_batch_query = lima_create_batch_query;
   ctx->base.destroy_query = lima_destroy_query;
   ctx->base.begin_query = lima_begin_query;
   ctx->base.end_query = lima_end_query;
   ctx->base.get_query_result = lima_get_query_result;
   ctx->base.set_active_query_state = lima_set_active_query_state;
}

