#include <stdint.h>
#include <stdbool.h>

#ifndef U_LOWER_PARALLEL_COPY_H
#define U_LOWER_PARALLEL_COPY_H

struct u_copy {
   /* Base register destination of the copy */
   unsigned dst;

   /* Base register source of the copy. If negative, the source is not a
    * register but some opaque caller-defined handle. */
   signed src;

   /* Number of consecutive registers of source/destination copied */
   unsigned size;

   /* Whether the copy has been handled. Callers must leave to false. */
   bool done;

   /* Extra fields for caller use */
   uint64_t user;
};

struct lower_parallel_copy_options {
   /* Number of physical registers modeled */
   unsigned num_regs;

   /* Callbacks to generate code */
   void (*copy)(const struct u_copy *entry, void *data);
   void (*swap)(const struct u_copy *entry, void *data);

   /* Data to pass to callbacks */
   void *data;
};

void u_lower_parallel_copy(struct lower_parallel_copy_options *options,
                           struct u_copy *copies,
                           unsigned num_copies);

#endif
