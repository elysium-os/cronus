#include "time.h"

#include "common/spinlock.h"

time_t g_time_monotonic = {};

static spinlock_t g_lock = SPINLOCK_INIT;

void time_advance(time_t length) {
    ipl_t ipl_previous = spinlock_acquire(&g_lock);

    g_time_monotonic = time_add(g_time_monotonic, length);

    spinlock_release(&g_lock, ipl_previous);
}
