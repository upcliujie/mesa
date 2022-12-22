#include <inttypes.h>

#include "util/u_debug.h"
#include "util/u_hash_table.h"
#include "util/u_memory.h"

#include "virgl_resource_stats.h"

void
virgl_resource_stats_print_report(const struct virgl_resource_stats *stats)
{
   debug_printf("VIRGL: Resource Stats:\n"
                "VIRGL: ===============\n"
                "VIRGL:   - pinned memory:  %" PRIu64 "\n"
                "VIRGL:   - resource count: %" PRIu32 "\n",
                stats->pinned_size,
                stats->resource_count);
}

void
virgl_resource_stats_add_alloc(struct virgl_resource_stats *stats, uint64_t alloc_size)
{
   stats->resource_count += 1;
   stats->pinned_size += alloc_size;
}

void
virgl_resource_stats_remove_alloc(struct virgl_resource_stats *stats, uint64_t alloc_size)
{
   stats->resource_count -= 1;
   stats->pinned_size -= alloc_size;
}

struct virgl_resource_stats *
virgl_resource_stats_create(void)
{
   struct virgl_resource_stats *stats = CALLOC(1, sizeof(*stats));
   if (!stats)
      return NULL;

   stats->resources = util_hash_table_create_ptr_keys();
   if (!stats->resources) {
      FREE(stats);
      return NULL;
   }

   return stats;
}

void
virgl_resource_stats_destroy(struct virgl_resource_stats *stats)
{
   _mesa_hash_table_destroy(stats->resources, NULL);
   FREE(stats);
}
