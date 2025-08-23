#pragma once

#include "sys/time.h"

/// Reset the event timer for an event.
void event_timer_arm(time_t delay);
