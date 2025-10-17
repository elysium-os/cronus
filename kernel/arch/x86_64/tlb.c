#include "x86_64/tlb.h"

#include "arch/cpu.h"
#include "arch/page.h"
#include "arch/time.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "common/log.h"
#include "lib/mem.h"
#include "memory/heap.h"
#include "sys/cpu.h"
#include "sys/init.h"
#include "x86_64/cpu/cpu.h"
#include "x86_64/cpu/lapic.h"
#include "x86_64/interrupt.h"

#define RETRY_AFTER_NS 1'000

static spinlock_t g_shootdown_lock = SPINLOCK_INIT;
static uint8_t g_shootdown_vector;

static spinlock_t g_status_lock = SPINLOCK_INIT;
static bool *g_shootdown_status = nullptr;
static size_t g_shootdown_complete_count = 0;

static uintptr_t g_shootdown_address;
static size_t g_shootdown_length;

static void invalidate(uintptr_t addr, size_t length) {
    LOG_TRACE("PTM", "invalidating on CPU(%lu) for %#lx - %#lx", X86_64_CPU_CURRENT_READ(sequential_id), addr, addr + length);
    for(; length > 0; length -= ARCH_PAGE_GRANULARITY, addr += ARCH_PAGE_GRANULARITY) asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static void tlb_shootdown_handler(arch_interrupt_frame_t *) {
    interrupt_state_t prev_state = spinlock_acquire_noint(&g_status_lock);

    if(g_shootdown_status[X86_64_CPU_CURRENT_READ(sequential_id)]) {
        spinlock_release_noint(&g_status_lock, prev_state);
        return;
    }

    uintptr_t address = g_shootdown_address;
    size_t length = g_shootdown_length;

    __atomic_store_n(&g_shootdown_status[X86_64_CPU_CURRENT_READ(sequential_id)], true, __ATOMIC_RELEASE);
    spinlock_release_noint(&g_status_lock, prev_state);

    invalidate(address, length);

    __atomic_add_fetch(&g_shootdown_complete_count, 1, __ATOMIC_RELEASE);
}

void x86_64_tlb_shootdown(uintptr_t addr, size_t length) {
    LOG_TRACE("PTM", "shootdown from CPU(%lu) for %#lx - %#lx [threaded: %u]", X86_64_CPU_CURRENT_READ(sequential_id), addr, addr + length, ARCH_CPU_CURRENT_READ(flags.threaded));
    if(!ARCH_CPU_CURRENT_READ(flags.threaded)) {
        invalidate(addr, length);
        return;
    }

    ASSERT(arch_interrupt_state());

    spinlock_acquire_nodw(&g_shootdown_lock);
    interrupt_state_t prev_state = spinlock_acquire_noint(&g_status_lock);
    g_shootdown_address = addr;
    g_shootdown_length = length;

    g_shootdown_complete_count = 0;
    memset(g_shootdown_status, false, sizeof(bool) * g_cpu_count);
    spinlock_release_noint(&g_status_lock, prev_state);

    invalidate(addr, length);
    __atomic_add_fetch(&g_shootdown_complete_count, 1, __ATOMIC_RELEASE);

    size_t last = 0;
    while(__atomic_load_n(&g_shootdown_complete_count, __ATOMIC_ACQUIRE) != g_cpu_count) {
        time_t time = arch_time_monotonic();
        if(time - last > RETRY_AFTER_NS) {
            last = time;
            for(size_t i = 0; i < g_cpu_count; i++) {
                x86_64_cpu_t *cpu = &g_x86_64_cpus[i];
                if(cpu == X86_64_CPU_CURRENT_READ(self)) continue;

                if(__atomic_load_n(&g_shootdown_status[cpu->sequential_id], __ATOMIC_ACQUIRE)) continue;

                x86_64_lapic_ipi(cpu->lapic_id, g_shootdown_vector | X86_64_LAPIC_IPI_ASSERT);
            }
        };

        asm volatile("pause");
    }

    spinlock_release_nodw(&g_shootdown_lock);
    LOG_TRACE("PTM", "shootdown finished for CPU(%lu). cpus (%lu/%lu)", X86_64_CPU_CURRENT_READ(sequential_id), g_shootdown_complete_count, g_cpu_count);
}

static void init_tlb() {
    int vector = x86_64_interrupt_request(INTERRUPT_PRIORITY_CRITICAL, tlb_shootdown_handler);
    ASSERT(vector != -1);
    g_shootdown_vector = (uint8_t) vector;
    g_shootdown_status = heap_alloc(sizeof(bool) * g_cpu_count);
}

INIT_TARGET(tlb, INIT_STAGE_MAIN, init_tlb);
