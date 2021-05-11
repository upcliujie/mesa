#ifndef ZINK_INLINES_H
#define ZINK_INLINES_H

/* these go here to avoid include hell */
static inline void
zink_select_draw_vbo(struct zink_context *ctx)
{
   ctx->base.draw_vbo = ctx->draw_vbo[ctx->multidraw][ctx->dynamic_state];
   assert(ctx->base.draw_vbo);
}

static inline void
zink_select_launch_grid(struct zink_context *ctx)
{
   if (!ctx->compute_stage)
      return;
   ctx->base.launch_grid = ctx->launch_grid[BITSET_TEST(ctx->compute_stage->nir->info.system_values_read, SYSTEM_VALUE_WORK_DIM)];
   assert(ctx->base.launch_grid);
}

#endif
