#include "tlb.h"

#include "arch/page.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "common/log.h"

#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/cpu/lapic.h"
#include "arch/x86_64/init.h"
#include "arch/x86_64/interrupt.h"

static uint8_t g_tlb_shootdown_vector;
static spinlock_t g_tlb_shootdown_lock;
static size_t g_tlb_shootdown_complete;
static uintptr_t g_tlb_shootdown_address;
static size_t g_tlb_shootdown_length;

static void invalidate(uintptr_t addr, size_t length) {
    LOG_DEVELOPMENT("PTM", "invalidating on CPU(%lu) for %#lx - %#lx", X86_64_CPU_CURRENT.sequential_id, addr, addr + length);
    for(; length > 0; length -= ARCH_PAGE_GRANULARITY, addr += ARCH_PAGE_GRANULARITY) asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static void tlb_shootdown_handler([[maybe_unused]] x86_64_interrupt_frame_t *frame) {
    ASSERT(x86_64_init_flag_check(X86_64_INIT_FLAG_SMP | X86_64_INIT_FLAG_SCHED));

    if(spinlock_try_acquire(&g_tlb_shootdown_lock)) {
        spinlock_release_raw(&g_tlb_shootdown_lock);
        log(LOG_LEVEL_WARN, "PTM", "Spurious TLB shootdown");
        return;
    }

    invalidate(g_tlb_shootdown_address, g_tlb_shootdown_length);
    __atomic_add_fetch(&g_tlb_shootdown_complete, 1, __ATOMIC_RELEASE);
}

void x86_64_tlb_shootdown(uintptr_t addr, size_t length) {
    LOG_DEVELOPMENT("PTM", "shootdown from CPU(%lu) for %#lx - %#lx", X86_64_CPU_CURRENT.sequential_id, addr, addr + length);
    if(!x86_64_init_flag_check(X86_64_INIT_FLAG_SMP | X86_64_INIT_FLAG_SCHED)) {
        invalidate(addr, length);
        return;
    }

    spinlock_acquire_nodw(&g_tlb_shootdown_lock);
    g_tlb_shootdown_address = addr;
    g_tlb_shootdown_length = length;
    g_tlb_shootdown_complete = 0;

    for(size_t i = 0; i < g_x86_64_cpu_count; i++) {
        x86_64_cpu_t *cpu = &g_x86_64_cpus[i];
        if(cpu == X86_64_CPU_CURRENT.self) {
            invalidate(addr, length);
            __atomic_add_fetch(&g_tlb_shootdown_complete, 1, __ATOMIC_RELEASE);
            continue;
        }

        x86_64_lapic_ipi(cpu->lapic_id, g_tlb_shootdown_vector | X86_64_LAPIC_IPI_ASSERT);
    }

    ASSERT(arch_interrupt_state());

    while(__atomic_load_n(&g_tlb_shootdown_complete, __ATOMIC_ACQUIRE) != g_x86_64_cpu_count) asm volatile("pause");

    spinlock_release_nodw(&g_tlb_shootdown_lock);
}

void x86_64_tlb_init_ipis() {
    int vector = x86_64_interrupt_request(INTERRUPT_PRIORITY_CRITICAL, tlb_shootdown_handler);
    ASSERT(vector != -1);
    g_tlb_shootdown_vector = (uint8_t) vector;
}
