#pragma once

#include "sys/time.h"

/// Setup the event timer for a given vector.
void arch_event_timer_setup(int vector);

/// Reset the event timer for an event.
void arch_event_timer_arm(time_t delay);
