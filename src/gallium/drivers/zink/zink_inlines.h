#ifndef ZINK_INLINES_H
#define ZINK_INLINES_H

/* these go here to avoid include hell */
static inline void
zink_select_draw_vbo(struct zink_context *ctx)
{
   if (!ctx->gfx_stages[PIPE_SHADER_VERTEX])
      return;
   ctx->base.draw_vbo = ctx->draw_vbo[ctx->multidraw][ctx->dynamic_state]
                                     [ctx->pipeline_changed[0]][ctx->num_so_targets > 0]
                                     [BITSET_TEST(ctx->gfx_stages[PIPE_SHADER_VERTEX]->nir->info.system_values_read, SYSTEM_VALUE_DRAW_ID)];
   assert(ctx->base.draw_vbo);
}

static inline void
zink_select_launch_grid(struct zink_context *ctx)
{
   if (!ctx->compute_stage)
      return;
   ctx->base.launch_grid = ctx->launch_grid[BITSET_TEST(ctx->compute_stage->nir->info.system_values_read, SYSTEM_VALUE_WORK_DIM)]
                                           [ctx->pipeline_changed[1]];
   assert(ctx->base.launch_grid);
}

#endif
