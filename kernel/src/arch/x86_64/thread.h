#pragma once

#include "lib/container.h"
#include "sched/thread.h"

#include <stddef.h>

#define X86_64_THREAD(THREAD) (CONTAINER_OF((THREAD), x86_64_thread_t, common))

typedef struct {
    uintptr_t base;
    uintptr_t size;
} x86_64_thread_stack_t;

typedef struct {
    uintptr_t rsp;
    uintptr_t syscall_rsp;
    x86_64_thread_stack_t kernel_stack;

    struct {
        void *fpu_area;
        uint64_t fs, gs;
    } state;

    thread_t common;
} x86_64_thread_t;
