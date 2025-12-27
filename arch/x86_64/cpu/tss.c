#include "x86_64/cpu/tss.h"

#include "arch/cpu.h"
#include "lib/mem.h"
#include "memory/heap.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "sys/init.h"
#include "x86_64/cpu/gdt.h"
#include "x86_64/interrupt.h"

void x86_64_tss_set_rsp0(x86_64_tss_t *tss, uintptr_t stack_pointer) {
    tss->rsp0_lower = (uint32_t) (uint64_t) stack_pointer;
    tss->rsp0_upper = (uint32_t) ((uint64_t) stack_pointer >> 32);
}

void x86_64_tss_set_ist(x86_64_tss_t *tss, int index, uintptr_t stack_pointer) {
    tss->ists[index].addr_lower = (uint32_t) (uint64_t) stack_pointer;
    tss->ists[index].addr_upper = (uint32_t) ((uint64_t) stack_pointer >> 32);
}


INIT_TARGET_PERCORE(tss, INIT_PROVIDES(), INIT_DEPS("memory", "cpu_local", "idt", "gdt")) {
    x86_64_tss_t *tss = heap_alloc(sizeof(x86_64_tss_t));
    mem_clear(tss, sizeof(x86_64_tss_t));
    tss->iomap_base = sizeof(x86_64_tss_t);

    x86_64_tss_set_ist(tss, 0, HHDM(PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(PMM_FLAG_NONE))) + ARCH_PAGE_GRANULARITY));
    x86_64_tss_set_ist(tss, 1, HHDM(PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(PMM_FLAG_NONE))) + ARCH_PAGE_GRANULARITY));
    x86_64_interrupt_set_ist(2, 1); // Non-maskable
    x86_64_interrupt_set_ist(18, 2); // Machine check

    ARCH_CPU_CURRENT_WRITE(arch.tss, tss);

    x86_64_gdt_load_tss(tss);
}
