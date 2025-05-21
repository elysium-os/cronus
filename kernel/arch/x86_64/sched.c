#include "sched.h"

#include "arch/cpu.h"
#include "arch/interrupt.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "common/log.h"
#include "lib/list.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "memory/heap.h"
#include "memory/hhdm.h"
#include "memory/pmm.h"
#include "memory/vm.h"
#include "sched/process.h"
#include "sched/sched.h"
#include "sched/thread.h"
#include "sys/interrupt.h"

#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/cpu/fpu.h"
#include "arch/x86_64/cpu/lapic.h"
#include "arch/x86_64/cpu/msr.h"
#include "arch/x86_64/init.h"
#include "arch/x86_64/interrupt.h"
#include "arch/x86_64/thread.h"

#include <stddef.h>

#define INTERVAL 100000
#define KERNEL_STACK_SIZE_PG 16

#define IDLE_TID 0
#define BOOTSTRAP_TID 1

typedef struct [[gnu::packed]] {
    uint64_t r12, r13, r14, r15, rbp, rbx;
    void (*thread_init)(x86_64_thread_t *prev);
    void (*entry)();
    void (*thread_exit_kernel)();

    struct {
        uint64_t rbp;
        uint64_t rip;
    } invalid_stack_frame;
} init_stack_kernel_t;

typedef struct [[gnu::packed]] {
    uint64_t r12, r13, r14, r15, rbp, rbx;
    void (*thread_init)(x86_64_thread_t *prev);
    void (*thread_init_user)();
    void (*entry)();
    uint64_t user_stack;
} init_stack_user_t;

extern x86_64_thread_t *x86_64_sched_context_switch(x86_64_thread_t *this, x86_64_thread_t *next);
extern void x86_64_sched_userspace_init();

static long g_next_tid = BOOTSTRAP_TID + 1;
static int g_sched_vector = 0;

/**
    @warning The prev parameter relies on the fact
    that sched_context_switch takes a thread "this" which
    will stay in RDI throughout the asm routine and will still
    be present upon entry here
*/
[[gnu::no_instrument_function]] static void common_thread_init(x86_64_thread_t *prev) {
    log(LOG_LEVEL_DEBUG, "SCHED", "common thread init for %lu", arch_sched_thread_current()->id);
    internal_sched_thread_drop(&prev->common);
    arch_interrupt_enable();
    arch_sched_preempt();
}

[[gnu::no_instrument_function]] static void kernel_thread_exit() {
    sched_yield(THREAD_STATE_DESTROY);
}

[[gnu::no_instrument_function]] static void sched_entry([[maybe_unused]] x86_64_interrupt_frame_t *frame) {
    sched_yield(THREAD_STATE_READY);
}

[[noreturn]] static void sched_idle() {
    while(true) {
        __builtin_ia32_pause();
        arch_cpu_halt();
    }
    ASSERT_UNREACHABLE();
}

[[gnu::no_instrument_function]] static void sched_switch(x86_64_thread_t *this, x86_64_thread_t *next) {
    ASSERT(!arch_interrupt_state());
    ASSERT(next != nullptr);

    if(next->common.proc != nullptr) {
        arch_ptm_load_address_space(next->common.proc->address_space);
    } else {
        arch_ptm_load_address_space(g_vm_global_address_space);
    }

    X86_64_CPU_CURRENT.current_thread = next;
    x86_64_tss_set_rsp0(X86_64_CPU_CURRENT.tss, next->kernel_stack.base);

    this->state.gs = x86_64_msr_read(X86_64_MSR_KERNEL_GS_BASE);
    this->state.fs = x86_64_msr_read(X86_64_MSR_FS_BASE);

    x86_64_msr_write(X86_64_MSR_KERNEL_GS_BASE, next->state.gs);
    x86_64_msr_write(X86_64_MSR_FS_BASE, next->state.fs);

    if(this->state.fpu_area != nullptr) g_x86_64_fpu_save(this->state.fpu_area);
    g_x86_64_fpu_restore(next->state.fpu_area);

    x86_64_thread_t *prev = x86_64_sched_context_switch(this, next);
    internal_sched_thread_drop(&prev->common);
}

static sched_t *pick_next_scheduler() {
    // TODO: this is NOT a good way of doing this
    static size_t current_cpu_id = 0;
    size_t cpu_id = __atomic_fetch_add(&current_cpu_id, 1, __ATOMIC_RELAXED);
    return &g_x86_64_cpus[cpu_id % g_x86_64_cpu_count].common.sched;
}

static x86_64_thread_t *create_thread(process_t *proc, size_t id, sched_t *scheduler, x86_64_thread_stack_t kernel_stack, uintptr_t rsp) {
    x86_64_thread_t *thread = heap_alloc(sizeof(x86_64_thread_t));
    thread->common.id = id;
    thread->common.state = THREAD_STATE_READY;
    thread->common.proc = proc;
    thread->common.scheduler = scheduler;
    thread->rsp = rsp;
    thread->kernel_stack = kernel_stack;
    thread->state.fs = 0;
    thread->state.gs = 0;
    thread->state.fpu_area = (void *) HHDM(pmm_alloc_pages(MATH_DIV_CEIL(g_x86_64_fpu_area_size, ARCH_PAGE_GRANULARITY), PMM_FLAG_ZERO)->paddr); // TODO: wasting a page here...
#ifdef __ENV_DEVELOPMENT
    thread->prof_current_call_frame = 0;
#endif

    log(LOG_LEVEL_DEBUG, "SCHED", "created tid %lu", thread->common.id);

    {
        interrupt_state_t previous_state = interrupt_state_mask();
        x86_64_thread_t *current_thread = X86_64_CPU_CURRENT.current_thread;
        if(current_thread != nullptr && current_thread->state.fpu_area != nullptr) g_x86_64_fpu_save(current_thread->state.fpu_area);
        g_x86_64_fpu_restore(thread->state.fpu_area);
        uint16_t x87cw = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (0b11 << 8);
        asm volatile("fldcw %0" : : "m"(x87cw) : "memory");
        uint32_t mxcsr = (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) | (1 << 12);
        asm volatile("ldmxcsr %0" : : "m"(mxcsr) : "memory");
        g_x86_64_fpu_save(thread->state.fpu_area);
        if(current_thread != nullptr && current_thread->state.fpu_area != nullptr) g_x86_64_fpu_restore(current_thread->state.fpu_area);
        interrupt_state_restore(previous_state);
    }

    if(proc != nullptr) {
        interrupt_state_t previous_state = spinlock_acquire(&proc->lock);
        list_append(&proc->threads, &thread->common.list_proc);
        spinlock_release(&proc->lock, previous_state);
    }

    return thread;
}

thread_t *arch_sched_thread_create_kernel(void (*func)()) {
    x86_64_thread_stack_t kernel_stack = { .base = HHDM(pmm_alloc_pages(KERNEL_STACK_SIZE_PG, PMM_FLAG_ZERO)->paddr + KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY), .size = KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY };

    init_stack_kernel_t *init_stack = (init_stack_kernel_t *) (kernel_stack.base - sizeof(init_stack_kernel_t));
    init_stack->entry = func;
    init_stack->thread_init = common_thread_init;
    init_stack->thread_exit_kernel = kernel_thread_exit;

    return &create_thread(nullptr, __atomic_fetch_add(&g_next_tid, 1, __ATOMIC_RELAXED), pick_next_scheduler(), kernel_stack, (uintptr_t) init_stack)->common;
}

thread_t *arch_sched_thread_create_user(process_t *proc, uintptr_t ip, uintptr_t sp) {
    x86_64_thread_stack_t kernel_stack = { .base = HHDM(pmm_alloc_pages(KERNEL_STACK_SIZE_PG, PMM_FLAG_ZERO)->paddr + KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY), .size = KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY };

    init_stack_user_t *init_stack = (init_stack_user_t *) (kernel_stack.base - sizeof(init_stack_user_t));
    init_stack->entry = (void (*)()) ip;
    init_stack->thread_init = common_thread_init;
    init_stack->thread_init_user = x86_64_sched_userspace_init;
    init_stack->user_stack = sp;

    return &create_thread(proc, __atomic_fetch_add(&g_next_tid, 1, __ATOMIC_RELAXED), pick_next_scheduler(), kernel_stack, (uintptr_t) init_stack)->common;
}

thread_t *arch_sched_thread_current() {
    x86_64_thread_t *thread = X86_64_CPU_CURRENT.current_thread;
    ASSERT(thread != nullptr);
    return &thread->common;
}

void arch_sched_preempt() {
    x86_64_lapic_timer_oneshot(g_sched_vector, INTERVAL);
}

void arch_sched_context_switch(thread_t *current, thread_t *next) {
    sched_switch(X86_64_THREAD(current), X86_64_THREAD(next));
}

void x86_64_sched_init_cpu(x86_64_cpu_t *cpu) {
    x86_64_thread_stack_t kernel_stack = { .base = HHDM(pmm_alloc_pages(KERNEL_STACK_SIZE_PG, PMM_FLAG_ZERO)->paddr + KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY), .size = KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY };

    init_stack_kernel_t *init_stack = (init_stack_kernel_t *) (kernel_stack.base - sizeof(init_stack_kernel_t));
    init_stack->entry = sched_idle;
    init_stack->thread_init = common_thread_init;
    init_stack->thread_exit_kernel = kernel_thread_exit;

    x86_64_thread_t *idle_thread = create_thread(nullptr, IDLE_TID, &cpu->common.sched, kernel_stack, (uintptr_t) init_stack);

    cpu->common.sched = (sched_t) { .lock = SPINLOCK_INIT, .thread_queue = LIST_INIT_CIRCULAR(cpu->common.sched.thread_queue), .idle_thread = &idle_thread->common };
}

[[gnu::no_instrument_function]] [[noreturn]] void x86_64_sched_handoff_cpu(x86_64_cpu_t *cpu, bool release) {
    x86_64_thread_t *bootstrap_thread = heap_alloc(sizeof(x86_64_thread_t));
    memclear(bootstrap_thread, sizeof(x86_64_thread_t));
    bootstrap_thread->common.state = THREAD_STATE_DESTROY;
    bootstrap_thread->common.scheduler = &cpu->common.sched;
    bootstrap_thread->common.id = BOOTSTRAP_TID;

    if(release) {
        x86_64_init_flag_set(X86_64_INIT_FLAG_SCHED);
    } else {
        while(!x86_64_init_flag_check(X86_64_INIT_FLAG_SCHED)) arch_cpu_relax();
    }

    sched_switch(bootstrap_thread, X86_64_THREAD(cpu->common.sched.idle_thread));
    ASSERT_UNREACHABLE();
}

void x86_64_sched_init() {
    int sched_vector = x86_64_interrupt_request(INTERRUPT_PRIORITY_LOW, sched_entry);
    if(sched_vector < 0) panic("unable to acquire an interrupt vector for the scheduler");
    g_sched_vector = sched_vector;
}
