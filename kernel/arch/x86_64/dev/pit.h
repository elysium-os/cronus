#pragma once

#include <stdint.h>

#define X86_64_PIT_BASE_FREQ 1193182

/// Set the reload value.
void x86_64_pit_set_reload(uint16_t reload_value);

/// Set the frequency.
void x86_64_pit_match_frequency(uint64_t frequency);

/// Retrieve current PIT count.
uint16_t x86_64_pit_count();
