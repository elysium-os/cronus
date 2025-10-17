#pragma once

#include "arch/interrupt.h"

/// Panic stub for unhandled exceptions.
[[noreturn]] void x86_64_exception_unhandled(arch_interrupt_frame_t *frame);
