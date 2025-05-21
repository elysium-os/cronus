#include "time.h"

#include "common/log.h"

#include <stddef.h>

static time_source_t *g_main_source = nullptr;

void time_source_register(time_source_t *source) {
    g_main_source = source;
}

time_t time_monotonic() {
    if(g_main_source == nullptr) {
        log(LOG_LEVEL_WARN, "TIME", "time requested. no available source");
        return 0;
    }
    return g_main_source->current();
}
