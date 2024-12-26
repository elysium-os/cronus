#pragma once

#include "arch/x86_64/interrupt.h"

/**
 * @brief Panic stub for unhandled exceptions.
 */
[[noreturn]] void x86_64_exception_unhandled(x86_64_interrupt_frame_t *frame);