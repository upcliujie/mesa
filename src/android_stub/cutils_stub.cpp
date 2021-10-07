#include <cutils/properties.h>
#include <cutils/trace.h>

extern "C" {

atomic_bool atrace_is_ready = ATOMIC_VAR_INIT(true);
int atrace_marker_fd = -1;
uint64_t atrace_enabled_tags = 0;

void
atrace_set_debuggable(bool /*debuggable*/)
{
}

void
atrace_set_tracing_enabled(bool /*enabled*/)
{
}

void
atrace_update_tags()
{
}

void
atrace_setup()
{
}

void
atrace_begin_body(const char * /*name*/)
{
}

void
atrace_end_body()
{
}

void
atrace_async_begin_body(const char * /*name*/, int32_t /*cookie*/)
{
}

void
atrace_async_end_body(const char * /*name*/, int32_t /*cookie*/)
{
}

void
atrace_int_body(const char * /*name*/, int32_t /*value*/)
{
}

void
atrace_int64_body(const char * /*name*/, int64_t /*value*/)
{
}

void
atrace_init()
{
}

uint64_t
atrace_get_enabled_tags()
{
   return ATRACE_TAG_NOT_READY;
}

int
property_get(const char *key, char *value, const char *default_value)
{
   return 0;
}
}
