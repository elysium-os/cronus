#include "sched.h"

#include "arch/cpu.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "common/assert.h"
#include "common/auxv.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "memory/heap.h"
#include "memory/hhdm.h"
#include "memory/pmm.h"
#include "memory/vm.h"
#include "sched/sched.h"
#include "sched/thread.h"

#include "arch/x86_64/cpu/fpu.h"
#include "arch/x86_64/cpu/lapic.h"
#include "arch/x86_64/cpu/msr.h"
#include "arch/x86_64/init.h"
#include "arch/x86_64/interrupt.h"

#define INTERVAL 100000
#define KERNEL_STACK_SIZE_PG 16
#define USER_STACK_SIZE (8 * ARCH_PAGE_GRANULARITY)

#define X86_64_THREAD(THREAD) (CONTAINER_OF((THREAD), x86_64_thread_t, common))

typedef struct {
    uintptr_t base;
    uintptr_t size;
} stack_t;

typedef struct x86_64_thread {
    struct x86_64_thread *this;
    uintptr_t rsp;
    uintptr_t syscall_rsp;
    stack_t kernel_stack;

    struct {
        void *fpu_area;
        uint64_t fs, gs;
    } state;

    thread_t common;
} x86_64_thread_t;

typedef struct {
    uint64_t r12, r13, r14, r15, rbp, rbx;
    void (*thread_init)(x86_64_thread_t *prev);
    void (*thread_init_kernel)();
    void (*entry)();

    struct {
        uint64_t rbp;
        uint64_t rip;
    } invalid_stack_frame;
} __attribute__((packed)) init_stack_kernel_t;

typedef struct {
    uint64_t r12, r13, r14, r15, rbp, rbx;
    void (*thread_init)(x86_64_thread_t *prev);
    void (*thread_init_user)();
    void (*entry)();
    uint64_t user_stack;
} __attribute__((packed)) init_stack_user_t;

static_assert(offsetof(x86_64_thread_t, rsp) == 8, "rsp in thread_t changed. Update arch/x86_64/sched.S::THREAD_RSP_OFFSET");

extern x86_64_thread_t *x86_64_sched_context_switch(x86_64_thread_t *this, x86_64_thread_t *next);
extern void x86_64_sched_userspace_init();

static long g_next_tid = 1;
static int g_sched_vector = 0;

/**
    @warning The prev parameter relies on the fact
    that sched_context_switch takes a thread "this" which
    will stay in RDI throughout the asm routine and will still
    be present upon entry here
*/
static void common_thread_init(x86_64_thread_t *prev) {
    sched_thread_drop(&prev->common);

    x86_64_lapic_timer_oneshot(g_sched_vector, INTERVAL);
}

static void kernel_thread_init() {
    asm volatile("sti");
}

[[noreturn]] static void sched_idle() {
    while(true) asm volatile("hlt");
    ASSERT_COMMENT(false, "Unreachable!");
    __builtin_unreachable();
}

static void sched_switch(x86_64_thread_t *this, x86_64_thread_t *next) {
    if(next->common.proc) {
        arch_ptm_load_address_space(next->common.proc->address_space);
    } else {
        arch_ptm_load_address_space(g_vm_global_address_space);
    }

    next->common.cpu = this->common.cpu;
    ASSERT(next != NULL);
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) next);
    this->common.cpu = 0;

    x86_64_tss_set_rsp0(X86_64_CPU(next->common.cpu)->tss, next->kernel_stack.base);

    this->state.gs = x86_64_msr_read(X86_64_MSR_KERNEL_GS_BASE);
    this->state.fs = x86_64_msr_read(X86_64_MSR_FS_BASE);

    x86_64_msr_write(X86_64_MSR_KERNEL_GS_BASE, next->state.gs);
    x86_64_msr_write(X86_64_MSR_FS_BASE, next->state.fs);

    if(this->state.fpu_area) g_x86_64_fpu_save(this->state.fpu_area);
    g_x86_64_fpu_restore(next->state.fpu_area);

    x86_64_thread_t *prev = x86_64_sched_context_switch(this, next);
    sched_thread_drop(&prev->common);
}

/** @warning Thread should not be on the scheduler queue when this is called */
void arch_sched_thread_destroy(thread_t *thread) {
    if(thread->proc) {
        spinlock_acquire(&thread->proc->lock);
        list_delete(&thread->list_proc);
        if(list_is_empty(&thread->proc->threads)) {
            sched_process_destroy(thread->proc);
        } else {
            spinlock_release(&thread->proc->lock);
        }
    }
    heap_free(X86_64_THREAD(thread));
}

static x86_64_thread_t *create_thread(process_t *proc, stack_t kernel_stack, uintptr_t rsp) {
    x86_64_thread_t *thread = heap_alloc(sizeof(x86_64_thread_t));
    memset(thread, 0, sizeof(x86_64_thread_t));
    thread->this = thread;
    thread->common.id = __atomic_fetch_add(&g_next_tid, 1, __ATOMIC_RELAXED);
    thread->common.state = THREAD_STATE_READY;
    thread->common.proc = proc;
    thread->rsp = rsp;
    thread->kernel_stack = kernel_stack;
    thread->state.fs = 0;
    thread->state.gs = 0;
    thread->state.fpu_area = heap_alloc_align(g_x86_64_fpu_area_size, 64);
    memset(thread->state.fpu_area, 0, g_x86_64_fpu_area_size);

    g_x86_64_fpu_restore(thread->state.fpu_area);
    uint16_t x87cw = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (0b11 << 8);
    asm volatile("fldcw %0" : : "m"(x87cw) : "memory");
    uint32_t mxcsr = (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) | (1 << 12);
    asm volatile("ldmxcsr %0" : : "m"(mxcsr) : "memory");
    g_x86_64_fpu_save(thread->state.fpu_area);

    return thread;
}

thread_t *arch_sched_thread_create_kernel(void (*func)()) {
    pmm_page_t *kernel_stack_page = pmm_alloc_pages(PMM_ZONE_NORMAL, KERNEL_STACK_SIZE_PG, PMM_FLAG_ZERO);
    stack_t kernel_stack = {.base = HHDM(kernel_stack_page->paddr + KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY), .size = KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY};

    init_stack_kernel_t *init_stack = (init_stack_kernel_t *) (kernel_stack.base - sizeof(init_stack_kernel_t));
    init_stack->entry = func;
    init_stack->thread_init = common_thread_init;
    init_stack->thread_init_kernel = kernel_thread_init;
    return &create_thread(NULL, kernel_stack, (uintptr_t) init_stack)->common;
}

thread_t *arch_sched_thread_create_user(process_t *proc, uintptr_t ip, uintptr_t sp) {
    pmm_page_t *kernel_stack_page = pmm_alloc_pages(PMM_ZONE_NORMAL, KERNEL_STACK_SIZE_PG, PMM_FLAG_ZERO);
    stack_t kernel_stack = {.base = HHDM(kernel_stack_page->paddr + KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY), .size = KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY};

    init_stack_user_t *init_stack = (init_stack_user_t *) (kernel_stack.base - sizeof(init_stack_user_t));
    init_stack->entry = (void (*)()) ip;
    init_stack->thread_init = common_thread_init;
    init_stack->thread_init_user = x86_64_sched_userspace_init;
    init_stack->user_stack = sp;

    x86_64_thread_t *thread = create_thread(proc, kernel_stack, (uintptr_t) init_stack);
    spinlock_acquire(&proc->lock);
    list_append(&proc->threads, &thread->common.list_proc);
    spinlock_release(&proc->lock);
    return &thread->common;
}

uintptr_t arch_sched_stack_setup(process_t *proc, char **argv, char **envp, auxv_t *auxv) {
#define WRITE_QWORD(VALUE)                                            \
    {                                                                 \
        stack -= sizeof(uint64_t);                                    \
        uint64_t tmp = (VALUE);                                       \
        ASSERT(vm_copy_to(proc->address_space, stack, &tmp, 4) == 4); \
    }

    void *stack_ptr = vm_map_anon(proc->address_space, NULL, USER_STACK_SIZE, (vm_protection_t) {.read = true, .write = true}, VM_CACHE_STANDARD, VM_FLAG_NONE);
    ASSERT(stack_ptr != NULL);
    uintptr_t stack = (uintptr_t) stack_ptr + USER_STACK_SIZE - 1;
    stack &= ~0xF;

    int argc = 0;
    for(; argv[argc]; argc++) stack -= string_length(argv[argc]) + 1;
    uintptr_t arg_data = stack;

    int envc = 0;
    for(; envp[envc]; envc++) stack -= string_length(envp[envc]) + 1;
    uintptr_t env_data = stack;

    stack -= (stack - (12 + 1 + envc + 1 + argc + 1) * sizeof(uint64_t)) % 0x10;

#define WRITE_AUX(ID, VALUE) \
    {                        \
        WRITE_QWORD(VALUE);  \
        WRITE_QWORD(ID);     \
    }
    WRITE_AUX(0, 0);
    WRITE_AUX(AUXV_SECURE, 0);
    WRITE_AUX(AUXV_ENTRY, auxv->entry);
    WRITE_AUX(AUXV_PHDR, auxv->phdr);
    WRITE_AUX(AUXV_PHENT, auxv->phent);
    WRITE_AUX(AUXV_PHNUM, auxv->phnum);
#undef WRITE_AUX

    WRITE_QWORD(0);
    for(int i = 0; i < envc; i++) {
        WRITE_QWORD(env_data);
        size_t str_sz = string_length(envp[i]) + 1;
        ASSERT(vm_copy_to(proc->address_space, env_data, envp[i], str_sz) == str_sz);
        env_data += str_sz;
    }

    WRITE_QWORD(0);
    for(int i = 0; i < argc; i++) {
        WRITE_QWORD(arg_data);
        size_t str_sz = string_length(argv[i]) + 1;
        ASSERT(vm_copy_to(proc->address_space, arg_data, argv[i], str_sz) == str_sz);
        arg_data += str_sz;
    }
    WRITE_QWORD(argc);

    return stack;
#undef WRITE_QWORD
}

thread_t *arch_sched_thread_current() {
    x86_64_thread_t *thread = NULL;
    asm volatile("mov %%gs:0, %0" : "=r"(thread));
    ASSERT(thread != NULL);
    return &thread->common;
}

void x86_64_sched_next() {
    thread_t *current = arch_sched_thread_current();

    thread_t *next = sched_thread_next();
    if(!next) {
        if(current == current->cpu->idle_thread) goto oneshot;
        next = current->cpu->idle_thread;
    }
    ASSERT(current != next);

    sched_switch(X86_64_THREAD(current), X86_64_THREAD(next));

oneshot:
    x86_64_lapic_timer_oneshot(g_sched_vector, INTERVAL);
}

static void sched_entry([[maybe_unused]] x86_64_interrupt_frame_t *frame) {
    x86_64_sched_next();
}

[[noreturn]] void x86_64_sched_init_cpu(x86_64_cpu_t *cpu, bool release) {
    x86_64_thread_t *idle_thread = X86_64_THREAD(arch_sched_thread_create_kernel(sched_idle));
    idle_thread->common.id = 0;
    cpu->common.idle_thread = &idle_thread->common;

    x86_64_thread_t *bootstrap_thread = heap_alloc(sizeof(x86_64_thread_t));
    memset(bootstrap_thread, 0, sizeof(x86_64_thread_t));
    bootstrap_thread->this = bootstrap_thread;
    bootstrap_thread->common.state = THREAD_STATE_DESTROY;
    bootstrap_thread->common.cpu = &cpu->common;

    if(release) {
        x86_64_init_flag_set(X86_64_INIT_FLAG_SCHED);
    } else {
        while(!x86_64_init_flag_check(X86_64_INIT_FLAG_SCHED)) arch_cpu_relax();
    }

    sched_switch(bootstrap_thread, idle_thread);
    __builtin_unreachable();
}

void x86_64_sched_init() {
    int sched_vector = x86_64_interrupt_request(X86_64_INTERRUPT_PRIORITY_PREEMPT, sched_entry);
    ASSERT_COMMENT(sched_vector >= 0, "Unable to acquire an interrupt vector for the scheduler");
    g_sched_vector = sched_vector;
}
