#pragma once

#include "sys/time.h"

/// Reset the event timer for an event.
void arch_event_timer_arm(time_t delay);
