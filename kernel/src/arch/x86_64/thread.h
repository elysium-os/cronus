#pragma once

#include "lib/container.h"
#include "sched/thread.h"

#include "arch/x86_64/debug.h"

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

#ifdef __ENV_DEVELOPMENT
    x86_64_debug_prof_call_frame_t prof_call_frames[X86_64_DEBUG_PROF_MAX_FRAMES];
    size_t prof_current_call_frame;
#endif

    thread_t common;
} x86_64_thread_t;

static_assert(offsetof(x86_64_thread_t, rsp) == 0, "rsp in thread_t changed. Update arch/x86_64/sched.asm::THREAD_RSP_OFFSET");
static_assert(offsetof(x86_64_thread_t, syscall_rsp) == 8, "syscall_rsp in thread_t changed. Update arch/amd64/sched/syscall.asm::SYSCALL_RSP_OFFSET");
static_assert(
    offsetof(x86_64_thread_t, kernel_stack) + offsetof(x86_64_thread_stack_t, base) == 16,
    "kernel_stack::base in thread_t changed. Update arch/x86_64/syscall.asm::KERNEL_STACK_BASE_OFFSET"
);
