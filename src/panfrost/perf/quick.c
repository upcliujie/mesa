#include <stdio.h>
#include <lib/pan_device.h>
#include "pan_perf.h"

enum dump_format {
   /* Human readable dump of all counters */
   HUMAN = 0,

   /* JSON dump of counters */
   JSON = 1,
};

static void
dump_human(const struct panfrost_perf *perf)
{
   for (unsigned i = 0; i < perf->cfg->n_categories; ++i) {
      const struct panfrost_perf_category *cat = &perf->cfg->categories[i];
      printf("%s\n", cat->name);

      for (unsigned j = 0; j < cat->n_counters; ++j) {
         const struct panfrost_perf_counter *ctr = &cat->counters[j];
         uint32_t val = panfrost_perf_counter_read(ctr, perf);
         printf("%s (%s): %u\n", ctr->name, ctr->symbol_name, val);
      }

      printf("\n");
   }
}

static void
dump_json(const struct panfrost_perf *perf)
{
   printf("{\n");

   for (unsigned i = 0; i < perf->cfg->n_categories; ++i) {
      const struct panfrost_perf_category *cat = &perf->cfg->categories[i];

      for (unsigned j = 0; j < cat->n_counters; ++j) {
         const struct panfrost_perf_counter *ctr = &cat->counters[j];
         uint32_t val = panfrost_perf_counter_read(ctr, perf);
         bool last = (i + 1) == perf->cfg->n_categories && (j + 1) == cat->n_counters;
         printf("    \"%s\": %u%s\n", ctr->symbol_name, val, last ? "" : ",");
      }
   }

   printf("}\n");
}

int main(int argc, const char **argv) {
   enum dump_format format = HUMAN;

   if (argc >= 2) {
      if (!strcmp(argv[1], "--json"))
         format = JSON;
      else
         fprintf(stderr, "Invalid option, expected --json\n");
   }

   int fd = drmOpenWithType("panfrost", NULL, DRM_NODE_RENDER);

   if (fd < 0) {
      fprintf(stderr, "No panfrost device\n");
      exit(1);
   }

   void *ctx = ralloc_context(NULL);
   struct panfrost_perf *perf = rzalloc(ctx, struct panfrost_perf);

   struct panfrost_device dev = {};
   panfrost_open_device(ctx, fd, &dev);

   panfrost_perf_init(perf, &dev);
   int ret = panfrost_perf_enable(perf);

   if (ret < 0) {
      fprintf(stderr, "failed to enable counters (%d)\n", ret);
      fprintf(stderr, "try `echo Y | sudo tee /sys/module/panfrost/parameters/unstable_ioctls`\n");

      exit(1);
   }

   sleep(1);

   panfrost_perf_dump(perf);

   if (format == HUMAN)
      dump_human(perf);
   else if (format == JSON)
      dump_json(perf);

   if (panfrost_perf_disable(perf) < 0) {
      fprintf(stderr, "failed to disable counters\n");
      exit(1);
   }

   panfrost_close_device(&dev);
}
