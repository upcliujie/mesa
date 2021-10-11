#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "virgl_context.h"
#include "virgl_encode.h"
#include "virtio-gpu/virgl_protocol.h"
#include "virgl_resource.h"
#include "virgl_screen.h"


struct virgl_host_reset_state {
   uint32_t result;
};

struct virgl_reset_status_query_object {
   uint32_t handle;
   struct virgl_resource *buf;
   enum pipe_reset_status result;
};


static void
virgl_create_device_reset_status_obj(struct virgl_context *vctx)
{

   vctx->reset_status = CALLOC_STRUCT(virgl_reset_status_query_object);
   if (!vctx->reset_status)
      return;

   vctx->reset_status->buf = (struct virgl_resource *)
                             pipe_buffer_create(vctx->base.screen, PIPE_BIND_CUSTOM,
                                                PIPE_USAGE_STAGING,
                                                sizeof(struct virgl_host_reset_state));

   if (!vctx->reset_status->buf) {
      FREE(vctx->reset_status);
      return;
   }

   vctx->reset_status->handle = virgl_object_assign_handle();

   util_range_add(&vctx->reset_status->buf->b, &vctx->reset_status->buf->valid_buffer_range, 0,
                  sizeof(struct virgl_host_reset_state));
   virgl_resource_dirty(vctx->reset_status->buf, 0);

   virgl_encoder_create_reset_status_obj(vctx, vctx->reset_status->handle,
                                         vctx->reset_status->buf);

}


static enum pipe_reset_status
virgl_get_device_reset_status(struct pipe_context *ctx)
{
   struct virgl_context *vctx = virgl_context(ctx);
   struct virgl_reset_status_query_object *rsq = vctx->reset_status;

   if (!rsq)
      return PIPE_NO_RESET;

   struct virgl_screen *vs = virgl_screen(ctx->screen);
   volatile struct virgl_host_reset_state *host_state;

   virgl_encoder_query_host_status(vctx, rsq->handle,
                                   virgl_host_query_reset_status);
   vs->vws->emit_res(vs->vws, vctx->cbuf,
                     vctx->reset_status->buf->hw_res, false);

   ctx->flush(ctx, NULL, 0);
   vs->vws->resource_wait(vs->vws, rsq->buf->hw_res);

   host_state = vs->vws->resource_map(vs->vws, rsq->buf->hw_res);

   rsq->result = host_state->result;

   return rsq->result;
}



void virgl_init_reset_status_functions(struct virgl_context *vctx)
{
   virgl_create_device_reset_status_obj(vctx);

   vctx->base.get_device_reset_status = virgl_get_device_reset_status;
}