#pragma once

#include "lib/container.h"
#include "sched/thread.h"
#include "x86_64/profiler.h"

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

    bool in_interrupt_handler;

#ifdef __ENV_DEVELOPMENT
    struct {
        bool active;
        bool in_profiler;
        size_t current_frame;
        x86_64_profiler_frame_t frames[X86_64_PROFILER_FRAMES];
        rb_tree_t records;
    } profiler;
#endif

    thread_t common;
} x86_64_thread_t;

static_assert(offsetof(x86_64_thread_t, rsp) == 0, "rsp in x86_64_thread_t changed. Update arch/x86_64/sched.asm::THREAD_RSP_OFFSET");
static_assert(offsetof(x86_64_thread_t, syscall_rsp) == 8, "syscall_rsp in x86_64_thread_t changed. Update arch/x86_64/sched/syscall.asm::SYSCALL_RSP_OFFSET");
static_assert(offsetof(x86_64_thread_t, kernel_stack) + offsetof(x86_64_thread_stack_t, base) == 16, "kernel_stack.base in x86_64_thread_t changed. Update arch/x86_64/syscall.asm::KERNEL_STACK_BASE_OFFSET");
