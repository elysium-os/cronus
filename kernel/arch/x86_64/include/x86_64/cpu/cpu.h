#pragma once

#include "lib/container.h"
#include "sys/cpu.h"
#include "sys/time.h"
#include "x86_64/cpu/tss.h"
#include "x86_64/thread.h"

#include <stddef.h>
#include <stdint.h>

#define X86_64_CPU(CPU) (CONTAINER_OF((CPU), x86_64_cpu_t, common))

#define X86_64_CPU_CURRENT (*(__seg_gs x86_64_cpu_t *) nullptr)

typedef struct x86_64_cpu {
    struct x86_64_cpu *self;

    size_t sequential_id;

    uint32_t lapic_id;

    time_frequency_t lapic_timer_frequency;
    time_frequency_t tsc_timer_frequency;

    x86_64_tss_t *tss;

    x86_64_thread_t *current_thread;

    cpu_t common;
} x86_64_cpu_t;

static_assert(offsetof(x86_64_cpu_t, current_thread) == 48, "current_thread in x86_64_cpu_t changed. Update arch/x86_64/syscall.asm::CURRENT_THREAD_OFFSET");

extern x86_64_cpu_t *g_x86_64_cpus;
