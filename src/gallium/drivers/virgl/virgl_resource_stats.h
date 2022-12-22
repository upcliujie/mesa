#ifndef VIRGL_RESOURCE_STATS_H
#define VIRGL_RESOURCE_STATS_H

#include "util/hash_table.h"

struct virgl_resource_stats {
   uint64_t pinned_size;
   uint32_t resource_count;

   struct hash_table *resources;
};

void
virgl_resource_stats_print_report(const struct virgl_resource_stats *stats);

void
virgl_resource_stats_add_alloc(struct virgl_resource_stats *stats, uint64_t alloc_size);

void
virgl_resource_stats_remove_alloc(struct virgl_resource_stats *stats, uint64_t alloc_size);

struct virgl_resource_stats *
virgl_resource_stats_create(void);

void
virgl_resource_stats_destroy(struct virgl_resource_stats *stats);

#endif /* VIRGL_RESOURCE_STATS_H */
