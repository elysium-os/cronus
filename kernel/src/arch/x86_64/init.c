#include "init.h"

#include "arch/cpu.h"
#include "arch/debug.h"
#include "arch/interrupt.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/log.h"
#include "dev/acpi/acpi.h"
#include "dev/pci.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/framebuffer.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "memory/heap.h"
#include "memory/hhdm.h"
#include "memory/pmm.h"
#include "memory/vm.h"
#include "sched/sched.h"
#include "sys/time.h"
#include "terminal.h"

#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/cpu/cpuid.h"
#include "arch/x86_64/cpu/cr.h"
#include "arch/x86_64/cpu/fpu.h"
#include "arch/x86_64/cpu/gdt.h"
#include "arch/x86_64/cpu/lapic.h"
#include "arch/x86_64/cpu/msr.h"
#include "arch/x86_64/cpu/port.h"
#include "arch/x86_64/dev/hpet.h"
#include "arch/x86_64/dev/ioapic.h"
#include "arch/x86_64/dev/pic8259.h"
#include "arch/x86_64/dev/pit.h"
#include "arch/x86_64/dev/qemu_debug.h"
#include "arch/x86_64/exception.h"
#include "arch/x86_64/interrupt.h"
#include "arch/x86_64/ptm.h"
#include "arch/x86_64/sched.h"

#include <stddef.h>
#include <tartarus.h>
#include <uacpi/uacpi.h>

#define PIT_TIMER_FREQ 1'000
#define LAPIC_CALIBRATION_TICKS 0x1'0000

#define ADJUST_STACK(OFFSET)                                                                                                                                \
    asm volatile("mov %%rsp, %%rax\nadd %0, %%rax\nmov %%rax, %%rsp\nmov %%rbp, %%rax\nadd %0, %%rax\nmov %%rax, %%rbp" : : "rm"(OFFSET) : "rax", "memory")

uintptr_t g_hhdm_offset;
size_t g_hhdm_size;

volatile size_t g_x86_64_cpu_count;
x86_64_cpu_t *g_x86_64_cpus;

framebuffer_t g_framebuffer;

static size_t init_flags = 0;

static vm_region_t g_hhdm_region, g_kernel_region;

static x86_64_cpu_t g_early_bsp;

static void thread_uacpi_setup() {
    uacpi_status ret = uacpi_initialize(0);
    if(uacpi_unlikely_error(ret)) {
        log(LOG_LEVEL_WARN, "UACPI", "initialization failed (%s)", uacpi_status_to_string(ret));
    } else {
        ret = uacpi_namespace_load();
        if(uacpi_unlikely_error(ret)) log(LOG_LEVEL_WARN, "UACPI", "namespace load failed (%s)", uacpi_status_to_string(ret));
    }
    log(LOG_LEVEL_INFO, "UACPI", "done");
}

[[noreturn]] static void init_ap() {
    log(LOG_LEVEL_INFO, "INIT", "Initializing AP %i", x86_64_lapic_id());

    x86_64_gdt_load();

    // Virtual Memory
    uint64_t pat = x86_64_msr_read(X86_64_MSR_PAT);
    pat &= ~(((uint64_t) 0b111 << 48) | ((uint64_t) 0b111 << 40));
    pat |= ((uint64_t) 0x1 << 48) | ((uint64_t) 0x5 << 40);
    x86_64_msr_write(X86_64_MSR_PAT, pat);

    uint64_t cr4 = x86_64_cr4_read();
    cr4 |= 1 << 7; /* CR4.PGE */
    x86_64_cr4_write(cr4);

    ADJUST_STACK(g_hhdm_offset);
    arch_ptm_load_address_space(g_vm_global_address_space);

    // Init CPU local immediately after address space load
    x86_64_cpu_t *cpu = &g_x86_64_cpus[g_x86_64_cpu_count];
    cpu->self = cpu;
    cpu->sequential_id = g_x86_64_cpu_count;
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) cpu);

    // Interrupts
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_APIC));
    x86_64_lapic_initialize();
    x86_64_interrupt_load_idt();
    arch_interrupt_set_ipl(IPL_PREEMPT);

    // CPU Local
    x86_64_tss_t *tss = heap_alloc(sizeof(x86_64_tss_t));
    memclear(tss, sizeof(x86_64_tss_t));
    tss->iomap_base = sizeof(x86_64_tss_t);
    x86_64_gdt_load_tss(tss);

    x86_64_pit_set_reload(UINT16_MAX);
    uint16_t start_count = x86_64_pit_count();
    x86_64_lapic_timer_poll(LAPIC_CALIBRATION_TICKS);
    uint16_t end_count = x86_64_pit_count();

    cpu->lapic_id = x86_64_lapic_id();
    cpu->lapic_timer_frequency = (uint64_t) (LAPIC_CALIBRATION_TICKS / (start_count - end_count)) * X86_64_PIT_BASE_FREQ;
    cpu->tss = tss;
    cpu->tlb_shootdown_check = SPINLOCK_INIT;
    cpu->tlb_shootdown_lock = SPINLOCK_INIT;

    x86_64_fpu_init_cpu();

    log(LOG_LEVEL_DEBUG, "INIT", "AP %i:%i init exit", g_x86_64_cpu_count, x86_64_lapic_id());
    __atomic_add_fetch(&g_x86_64_cpu_count, 1, __ATOMIC_SEQ_CST);

    asm volatile("sti");

    x86_64_sched_init_cpu(cpu, false);
    __builtin_unreachable();
}

[[noreturn]] void init(tartarus_boot_info_t *boot_info) {
    g_hhdm_offset = boot_info->hhdm.offset;
    g_hhdm_size = boot_info->hhdm.size;

    if(boot_info->framebuffer_count == 0) panic("no framebuffer provided");
    tartarus_framebuffer_t *framebuffer = &boot_info->framebuffers[0];
    g_framebuffer.physical_address = HHDM_TO_PHYS(framebuffer->address);
    g_framebuffer.size = framebuffer->size;
    g_framebuffer.width = framebuffer->width;
    g_framebuffer.height = framebuffer->height;
    g_framebuffer.pitch = framebuffer->pitch;
    // TODO handle pixel format

    memclear(&g_early_bsp, sizeof(g_early_bsp));
    g_early_bsp.self = &g_early_bsp;
    g_early_bsp.sequential_id = boot_info->bsp_index;
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uintptr_t) &g_early_bsp);

#ifdef __ENV_DEVELOPMENT
    x86_64_qemu_debug_putc('\n');
    log_sink_add(&g_x86_64_qemu_debug_sink);
#endif

    log_sink_add(&g_terminal_sink);
    draw_rect(&g_framebuffer, 0, 0, g_framebuffer.width, g_framebuffer.height, draw_color(14, 14, 15));
    log(LOG_LEVEL_INFO, "INIT", "Elysium alpha.6 (" __DATE__ " " __TIME__ ")");

    log(LOG_LEVEL_DEBUG, "INIT", "Enumerating modules");
    for(uint16_t i = 0; i < boot_info->module_count; i++) {
        tartarus_module_t *module = &boot_info->modules[i];
        log(LOG_LEVEL_DEBUG, "INIT", "Module found: %s", module->name);
        if(!string_eq("kernel_symbols.txt", module->name)) continue;
        g_arch_debug_symbols = (char *) HHDM(module->paddr);
        g_arch_debug_symbols_length = module->size;
        log(LOG_LEVEL_DEBUG, "INIT", "Kernel symbols loaded");
        break;
    }

    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_MSR));

    x86_64_gdt_load();

    // Initialize interrupt & exception handling
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_APIC))
    x86_64_pic8259_remap();
    x86_64_pic8259_disable();
    x86_64_lapic_initialize();
    g_x86_64_interrupt_irq_eoi = x86_64_lapic_eoi;
    x86_64_interrupt_init();
    x86_64_interrupt_load_idt();
    for(int i = 0; i < 32; i++) {
        switch(i) {
            default: x86_64_interrupt_set(i, x86_64_exception_unhandled); break;
        }
    }
    arch_interrupt_set_ipl(IPL_PREEMPT);
    x86_64_init_flag_set(X86_64_INIT_FLAG_INTERRUPTS);

    // Initialize physical memory
    pmm_zone_register(PMM_ZONE_LOW, "LOW", 0, 0x100'0000);
    pmm_zone_register(PMM_ZONE_NORMAL, "Normal", 0x100'0000, UINTPTR_MAX);
    for(int i = 0; i < boot_info->memory_map.size; i++) {
        tartarus_memory_map_entry_t entry = boot_info->memory_map.entries[i];
        if(entry.type != TARTARUS_MEMORY_MAP_TYPE_USABLE) continue;
        pmm_region_add(entry.base, entry.length);
    }
    x86_64_init_flag_set(X86_64_INIT_FLAG_MEMORY_PHYS);

    log(LOG_LEVEL_DEBUG, "INIT", "Physical Memory Map");
    for(size_t i = 0; i <= PMM_ZONE_COUNT; i++) {
        if(!PMM_ZONE_PRESENT(i)) continue;
        pmm_zone_t *zone = &g_pmm_zones[i];
        log(LOG_LEVEL_DEBUG, "INIT", "- %s", zone->name);
        LIST_FOREACH(&zone->regions, elem) {
            pmm_region_t *region = LIST_CONTAINER_GET(elem, pmm_region_t, list_elem);
            log(LOG_LEVEL_DEBUG, "INIT", "  - %#-12lx %lu/%lu pages", region->base, region->free_count, region->page_count);
        }
    }

    // Initialize Virtual Memory
    uint64_t pat = x86_64_msr_read(X86_64_MSR_PAT);
    pat &= ~(((uint64_t) 0b111 << 48) | ((uint64_t) 0b111 << 40));
    pat |= ((uint64_t) 0x1 << 48) | ((uint64_t) 0x5 << 40);
    x86_64_msr_write(X86_64_MSR_PAT, pat);

    uint64_t cr4 = x86_64_cr4_read();
    cr4 |= 1 << 7; /* CR4.PGE */
    x86_64_cr4_write(cr4);

    g_vm_global_address_space = x86_64_ptm_init();
    x86_64_interrupt_set(0xE, x86_64_ptm_page_fault_handler);

    g_hhdm_region.address_space = g_vm_global_address_space;
    g_hhdm_region.base = MATH_FLOOR(g_hhdm_offset, ARCH_PAGE_GRANULARITY);
    g_hhdm_region.length = MATH_CEIL(g_hhdm_size, ARCH_PAGE_GRANULARITY);
    g_hhdm_region.protection = (vm_protection_t) {.read = true, .write = true};
    g_hhdm_region.cache_behavior = VM_CACHE_STANDARD;
    g_hhdm_region.type = VM_REGION_TYPE_DIRECT;
    g_hhdm_region.type_data.direct.physical_address = 0;
    list_append(&g_vm_global_address_space->regions, &g_hhdm_region.list_elem);

    g_kernel_region.address_space = g_vm_global_address_space;
    g_kernel_region.base = MATH_FLOOR(boot_info->kernel.vaddr, ARCH_PAGE_GRANULARITY);
    g_kernel_region.length = MATH_CEIL(boot_info->kernel.size, ARCH_PAGE_GRANULARITY);
    g_kernel_region.protection = (vm_protection_t) {.read = true, .write = true};
    g_kernel_region.cache_behavior = VM_CACHE_STANDARD;
    g_kernel_region.type = VM_REGION_TYPE_ANON;
    list_append(&g_vm_global_address_space->regions, &g_kernel_region.list_elem);

    ADJUST_STACK(g_hhdm_offset);
    arch_ptm_load_address_space(g_vm_global_address_space);
    x86_64_init_flag_set(X86_64_INIT_FLAG_MEMORY_VIRT);

    // Initialize HEAP
    heap_initialize(g_vm_global_address_space, 0x1'000'000);

    // Initialize FPU
    x86_64_fpu_init();
    x86_64_fpu_init_cpu();

    // Initialize ACPI
    log(LOG_LEVEL_DEBUG, "INIT", "RSDP at %#lx", boot_info->acpi_rsdp_address);
    acpi_init(boot_info->acpi_rsdp_address);

    // Initialize IOAPIC
    acpi_sdt_header_t *madt = acpi_find_table((uint8_t *) "APIC");
    if(madt != NULL) x86_64_ioapic_init(madt);

    // Initialize PCI
    acpi_sdt_header_t *mcfg = acpi_find_table((uint8_t *) "MCFG");
    pci_enumerate(mcfg);

    // Initialize sched
    x86_64_sched_init();

    // SMP init
    g_x86_64_cpus = heap_alloc(sizeof(x86_64_cpu_t) * boot_info->cpu_count);
    memclear(g_x86_64_cpus, sizeof(x86_64_cpu_t) * boot_info->cpu_count);

    x86_64_tss_t *tss = heap_alloc(sizeof(x86_64_tss_t));
    memclear(tss, sizeof(x86_64_tss_t));
    tss->iomap_base = sizeof(x86_64_tss_t);
    x86_64_gdt_load_tss(tss);

    x86_64_pit_set_reload(UINT16_MAX);
    uint16_t start_count = x86_64_pit_count();
    x86_64_lapic_timer_poll(LAPIC_CALIBRATION_TICKS);
    uint16_t end_count = x86_64_pit_count();

    x86_64_cpu_t *cpu = NULL;
    g_x86_64_cpu_count = 0;
    for(size_t i = 0; i < boot_info->cpu_count; i++) {
        if(boot_info->cpus[i].initialization_failed) continue;

        if(i == boot_info->bsp_index) {
            cpu = &g_x86_64_cpus[g_x86_64_cpu_count];
            *cpu = g_early_bsp;
            cpu->self = cpu;
            cpu->sequential_id = g_x86_64_cpu_count;
            cpu->lapic_id = x86_64_lapic_id();
            cpu->lapic_timer_frequency = (uint64_t) (LAPIC_CALIBRATION_TICKS / (start_count - end_count)) * X86_64_PIT_BASE_FREQ;
            cpu->tss = tss;
            cpu->tlb_shootdown_check = SPINLOCK_INIT;
            cpu->tlb_shootdown_lock = SPINLOCK_INIT;
            x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) cpu);
            g_x86_64_cpu_count++;
            continue;
        }

        size_t previous_count = g_x86_64_cpu_count;
        *boot_info->cpus[i].wake_on_write = (uint64_t) init_ap;
        while(previous_count >= g_x86_64_cpu_count);
    }
    ASSERT(cpu != NULL);

    log(LOG_LEVEL_DEBUG, "INIT", "SMP init done (%i/%i cpus initialized)", g_x86_64_cpu_count, boot_info->cpu_count);
    x86_64_init_flag_set(X86_64_INIT_FLAG_SMP);

    // Initialize timer
    acpi_sdt_header_t *hpet_header = acpi_find_table((uint8_t *) "HPET");
    if(hpet_header != NULL) {
        x86_64_hpet_init(hpet_header);
        time_source_register(&g_hpet_time_source);
        //     int timer = x86_64_hpet_find_timer(true);
        //     if(timer == -1) goto use_pit;
        //     int gsi_vector = x86_64_hpet_configure_timer(timer, HPET_TIMER_FREQ, false);

        //     int hpet_time_vector = x86_64_interrupt_request(X86_64_INTERRUPT_PRIORITY_CRITICAL, hpet_time_handler);
        //     ASSERT(hpet_time_vector != -1);
        //     x86_64_ioapic_map_gsi(gsi_vector, x86_64_lapic_id(), false, true, hpet_time_vector);
    } else {
        // use_pit:
        // x86_64_pit_match_frequency(PIT_TIMER_FREQ);
        // int pit_time_vector = x86_64_interrupt_request(X86_64_INTERRUPT_PRIORITY_CRITICAL, pit_time_handler);
        // ASSERT(pit_time_vector != -1);
        // x86_64_ioapic_map_legacy_irq(0, x86_64_lapic_id(), false, true, pit_time_vector);
    }

    x86_64_init_flag_set(X86_64_INIT_FLAG_TIME);

    asm volatile("sti");

    sched_thread_schedule(arch_sched_thread_create_kernel(thread_uacpi_setup));

    log(LOG_LEVEL_INFO, "INIT", "Reached scheduler handoff. Bye for now!");
    x86_64_sched_init_cpu(cpu, true);
    __builtin_unreachable();
}

bool x86_64_init_flag_check(size_t flags) {
    return (flags & init_flags) == flags;
}

void x86_64_init_flag_set(size_t flags) {
    init_flags |= flags;
}
