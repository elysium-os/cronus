#include "sched.h"

#include "arch/cpu.h"
#include "arch/interrupt.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/auxv.h"
#include "lib/math.h"
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
#include "arch/x86_64/thread.h"

#define INTERVAL 100000
#define KERNEL_STACK_SIZE_PG 16
#define USER_STACK_SIZE (8 * ARCH_PAGE_GRANULARITY)
#define INITIAL_RFLAGS (1 << 1)

typedef struct [[gnu::packed]] {
    uint64_t r12, r13, r14, r15, rbp, rbx, flags;
    void (*thread_init)(x86_64_thread_t *prev);
    void (*entry)();
    void (*thread_exit_kernel)();

    struct {
        uint64_t rbp;
        uint64_t rip;
    } invalid_stack_frame;
} init_stack_kernel_t;

typedef struct [[gnu::packed]] {
    uint64_t r12, r13, r14, r15, rbp, rbx, flags;
    void (*thread_init)(x86_64_thread_t *prev);
    void (*thread_init_user)();
    void (*entry)();
    uint64_t user_stack;
} init_stack_user_t;

static_assert(offsetof(x86_64_thread_t, rsp) == 0, "rsp in thread_t changed. Update arch/x86_64/sched.S::THREAD_RSP_OFFSET");

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
[[gnu::no_instrument_function]] static void common_thread_init(x86_64_thread_t *prev) {
    sched_thread_drop(&prev->common);

    arch_interrupt_enable();

    x86_64_lapic_timer_oneshot(g_sched_vector, INTERVAL);
}

[[gnu::no_instrument_function]] static void kernel_thread_exit() {
    arch_sched_thread_current()->state = THREAD_STATE_DESTROY;
    arch_sched_yield();
}

[[noreturn]] static void sched_idle() {
    while(true) {
        __builtin_ia32_pause();
        asm volatile("hlt");
    }
    __builtin_unreachable();
}

[[gnu::no_instrument_function]] static void sched_switch(x86_64_thread_t *this, x86_64_thread_t *next) {
    ASSERT(!arch_interrupt_state());
    ASSERT(next != NULL);

    if(next->common.proc != NULL) {
        arch_ptm_load_address_space(next->common.proc->address_space);
    } else {
        arch_ptm_load_address_space(g_vm_global_address_space);
    }

    X86_64_CPU_LOCAL_MEMBER_SET(current_thread, next);
    x86_64_tss_set_rsp0(X86_64_CPU_LOCAL_MEMBER(tss), next->kernel_stack.base);

    this->state.gs = x86_64_msr_read(X86_64_MSR_KERNEL_GS_BASE);
    this->state.fs = x86_64_msr_read(X86_64_MSR_FS_BASE);

    x86_64_msr_write(X86_64_MSR_KERNEL_GS_BASE, next->state.gs);
    x86_64_msr_write(X86_64_MSR_FS_BASE, next->state.fs);

    if(this->state.fpu_area) g_x86_64_fpu_save(this->state.fpu_area);
    g_x86_64_fpu_restore(next->state.fpu_area);

    x86_64_thread_t *prev = x86_64_sched_context_switch(this, next);
    sched_thread_drop(&prev->common);
}

[[gnu::no_instrument_function]] void arch_sched_yield() {
    interrupt_state_t previous_state = interrupt_state_mask();
    thread_t *current = arch_sched_thread_current();

    thread_t *next = sched_thread_next();
    if(next == NULL) {
        if(current == X86_64_CPU_LOCAL_MEMBER(self)->common.idle_thread) goto oneshot;
        next = X86_64_CPU_LOCAL_MEMBER(self)->common.idle_thread;
    }
    ASSERT(current != next);

    sched_switch(X86_64_THREAD(current), X86_64_THREAD(next));

oneshot:
    x86_64_lapic_timer_oneshot(g_sched_vector, INTERVAL);

    interrupt_state_restore(previous_state);
}

void arch_sched_thread_destroy(thread_t *thread) {
    if(thread->proc != NULL) {
        interrupt_state_t previous_state = spinlock_acquire(&thread->proc->lock);
        list_delete(&thread->list_proc);
        if(list_is_empty(&thread->proc->threads)) {
            sched_process_destroy(thread->proc);
            interrupt_state_restore(previous_state);
        } else {
            spinlock_release(&thread->proc->lock, previous_state);
        }
    }
    heap_free(X86_64_THREAD(thread), sizeof(x86_64_thread_t));
}

static x86_64_thread_t *create_thread(process_t *proc, x86_64_thread_stack_t kernel_stack, uintptr_t rsp) {
    x86_64_thread_t *thread = heap_alloc(sizeof(x86_64_thread_t));
    memclear(thread, sizeof(x86_64_thread_t));
    thread->common.id = __atomic_fetch_add(&g_next_tid, 1, __ATOMIC_RELAXED);
    thread->common.state = THREAD_STATE_READY;
    thread->common.proc = proc;
    thread->rsp = rsp;
    thread->kernel_stack = kernel_stack;
    thread->state.fs = 0;
    thread->state.gs = 0;
    thread->state.fpu_area = (void *) HHDM(pmm_alloc_pages(MATH_DIV_CEIL(g_x86_64_fpu_area_size, ARCH_PAGE_GRANULARITY), PMM_FLAG_ZERO)->paddr);
#ifdef __ENV_DEVELOPMENT
    thread->prof_current_call_frame = 0;
#endif

    g_x86_64_fpu_restore(thread->state.fpu_area);
    uint16_t x87cw = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (0b11 << 8);
    asm volatile("fldcw %0" : : "m"(x87cw) : "memory");
    uint32_t mxcsr = (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) | (1 << 12);
    asm volatile("ldmxcsr %0" : : "m"(mxcsr) : "memory");
    g_x86_64_fpu_save(thread->state.fpu_area);

    return thread;
}

[[gnu::no_instrument_function]] static void sched_entry([[maybe_unused]] x86_64_interrupt_frame_t *frame) {
    arch_sched_yield();
}

thread_t *arch_sched_thread_create_kernel(void (*func)()) {
    pmm_block_t *kernel_stack_page = pmm_alloc_pages(KERNEL_STACK_SIZE_PG, PMM_FLAG_ZERO);
    x86_64_thread_stack_t kernel_stack = {
        .base = HHDM(kernel_stack_page->paddr + KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY),
        .size = KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY
    };

    init_stack_kernel_t *init_stack = (init_stack_kernel_t *) (kernel_stack.base - sizeof(init_stack_kernel_t));
    init_stack->flags = INITIAL_RFLAGS;
    init_stack->entry = func;
    init_stack->thread_init = common_thread_init;
    init_stack->thread_exit_kernel = kernel_thread_exit;
    return &create_thread(NULL, kernel_stack, (uintptr_t) init_stack)->common;
}

thread_t *arch_sched_thread_create_user(process_t *proc, uintptr_t ip, uintptr_t sp) {
    pmm_block_t *kernel_stack_page = pmm_alloc_pages(KERNEL_STACK_SIZE_PG, PMM_FLAG_ZERO);
    x86_64_thread_stack_t kernel_stack = {
        .base = HHDM(kernel_stack_page->paddr + KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY),
        .size = KERNEL_STACK_SIZE_PG * ARCH_PAGE_GRANULARITY
    };

    init_stack_user_t *init_stack = (init_stack_user_t *) (kernel_stack.base - sizeof(init_stack_user_t));
    init_stack->flags = INITIAL_RFLAGS;
    init_stack->entry = (void (*)()) ip;
    init_stack->thread_init = common_thread_init;
    init_stack->thread_init_user = x86_64_sched_userspace_init;
    init_stack->user_stack = sp;

    x86_64_thread_t *thread = create_thread(proc, kernel_stack, (uintptr_t) init_stack);
    interrupt_state_t previous_state = spinlock_acquire(&proc->lock);
    list_append(&proc->threads, &thread->common.list_proc);
    spinlock_release(&proc->lock, previous_state);
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
    x86_64_thread_t *thread = X86_64_CPU_LOCAL_MEMBER(current_thread);
    ASSERT(thread != NULL);
    return &thread->common;
}

[[gnu::no_instrument_function]] [[noreturn]] void x86_64_sched_init_cpu(x86_64_cpu_t *cpu, bool release) {
    x86_64_thread_t *idle_thread = X86_64_THREAD(arch_sched_thread_create_kernel(sched_idle));
    idle_thread->common.id = 0;
    cpu->common.idle_thread = &idle_thread->common;

    x86_64_thread_t *bootstrap_thread = heap_alloc(sizeof(x86_64_thread_t));
    memclear(bootstrap_thread, sizeof(x86_64_thread_t));
    bootstrap_thread->common.state = THREAD_STATE_DESTROY;

    if(release) {
        x86_64_init_flag_set(X86_64_INIT_FLAG_SCHED);
    } else {
        while(!x86_64_init_flag_check(X86_64_INIT_FLAG_SCHED)) arch_cpu_relax();
    }

    sched_switch(bootstrap_thread, idle_thread);
    __builtin_unreachable();
}

void x86_64_sched_init() {
    int sched_vector = x86_64_interrupt_request(INTERRUPT_PRIORITY_LOW, sched_entry);
    ASSERT_COMMENT(sched_vector >= 0, "Unable to acquire an interrupt vector for the scheduler");
    g_sched_vector = sched_vector;
}
