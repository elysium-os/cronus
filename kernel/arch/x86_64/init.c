#include "init.h"

#include "abi/sysv/elf.h"
#include "arch/cpu.h"
#include "arch/event.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "arch/sched.h"
#include "arch/time.h"
#include "common/assert.h"
#include "common/lock/spinlock.h"
#include "common/log.h"
#include "common/panic.h"
#include "dev/acpi.h"
#include "dev/pci.h"
#include "fs/rdsk.h"
#include "fs/tmpfs.h"
#include "fs/vfs.h"
#include "graphics/draw.h"
#include "graphics/framebuffer.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "memory/heap.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/pmm.h"
#include "memory/slab.h"
#include "memory/vm.h"
#include "sched/process.h"
#include "sched/reaper.h"
#include "sched/sched.h"
#include "sys/dw.h"
#include "sys/event.h"
#include "sys/kernel_symbol.h"
#include "sys/module.h"
#include "sys/time.h"
#include "terminal.h"

#include "arch/x86_64/abi/sysv/auxv.h"
#include "arch/x86_64/abi/sysv/sysv.h"
#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/cpu/cpuid.h"
#include "arch/x86_64/cpu/cr.h"
#include "arch/x86_64/cpu/fpu.h"
#include "arch/x86_64/cpu/gdt.h"
#include "arch/x86_64/cpu/lapic.h"
#include "arch/x86_64/cpu/msr.h"
#include "arch/x86_64/cpu/pat.h"
#include "arch/x86_64/dev/hpet.h"
#include "arch/x86_64/dev/ioapic.h"
#include "arch/x86_64/dev/pic8259.h"
#include "arch/x86_64/dev/pit.h"
#include "arch/x86_64/dev/qemu_debug.h"
#include "arch/x86_64/exception.h"
#include "arch/x86_64/interrupt.h"
#include "arch/x86_64/ptm.h"
#include "arch/x86_64/sched.h"
#include "arch/x86_64/syscall.h"
#include "arch/x86_64/tlb.h"

#include <elyboot.h>
#include <stddef.h>
#include <stdint.h>
#include <uacpi/tables.h>
#include <uacpi/uacpi.h>

#define ADJUST_STACK(OFFSET) asm volatile("mov %%rsp, %%rax\nadd %0, %%rax\nmov %%rax, %%rsp\nmov %%rbp, %%rax\nadd %0, %%rax\nmov %%rax, %%rbp" : : "rm"(OFFSET) : "rax", "memory")

uintptr_t g_hhdm_offset;
size_t g_hhdm_size;

size_t g_x86_64_cpu_count;
x86_64_cpu_t *g_x86_64_cpus;

framebuffer_t g_framebuffer;

page_t *g_page_cache;
size_t g_page_cache_size;

static volatile bool g_init_ap_finished = false;
static uint64_t g_init_ap_cpu_id = 0;

static size_t g_init_flags = 0;

static vm_region_t g_hhdm_region, g_kernel_region, g_page_cache_region;

static x86_64_cpu_t g_early_bsp;

static bool g_hpet_initialized = false;

static int g_event_interrupt_vector = -1;

static void thread_init() {
    uacpi_status ret = uacpi_namespace_load();
    if(uacpi_unlikely_error(ret)) log(LOG_LEVEL_WARN, "UACPI", "namespace load failed (%s)", uacpi_status_to_string(ret));
    log(LOG_LEVEL_INFO, "UACPI", "Setup Done");
}

static time_frequency_t calibrate_lapic_timer() {
    uint32_t nominal_freq = 0;
    if(!x86_64_cpuid_register(0x15, X86_64_CPUID_REGISTER_ECX, &nominal_freq) && nominal_freq != 0) {
        return (time_frequency_t) nominal_freq;
    }

    x86_64_lapic_timer_setup(X86_64_LAPIC_TIMER_TYPE_ONESHOT, true, 0xFF, X86_64_LAPIC_TIMER_DIVISOR_16);
    if(!g_hpet_initialized) {
        for(size_t sample_count = 8;; sample_count *= 2) {
            x86_64_pit_set_reload(0xFFF0);
            uint16_t start_count = x86_64_pit_count();
            x86_64_lapic_timer_start(sample_count);
            while(x86_64_lapic_timer_read() != 0) arch_cpu_relax();
            uint64_t delta = start_count - x86_64_pit_count();

            if(delta < 0x4000) continue;

            x86_64_lapic_timer_stop();
            return (sample_count / MATH_MAX(1lu, delta)) * X86_64_PIT_BASE_FREQ;
        }
    } else {
        time_t timeout = (TIME_NANOSECONDS_IN_SECOND / TIME_MILLISECONDS_IN_SECOND) * 100;
        x86_64_lapic_timer_start(UINT32_MAX);
        time_t target = hpet_current_time() + timeout;
        while(hpet_current_time() < target) arch_cpu_relax();
        return ((uint64_t) UINT32_MAX - x86_64_lapic_timer_read()) * (TIME_NANOSECONDS_IN_SECOND / timeout);
    }
    ASSERT_UNREACHABLE();
}

// TODO: cpuid for TSC freq if available :)
static time_frequency_t calibrate_tsc() {
    if(!g_hpet_initialized) {
        for(size_t sample_count = 8;; sample_count *= 2) {
            x86_64_pit_set_reload(0xFFF0);
            uint16_t start_count = x86_64_pit_count();
            uint64_t tsc_target = __rdtsc() + sample_count;
            while(__rdtsc() < tsc_target) arch_cpu_relax();
            uint64_t delta = start_count - x86_64_pit_count();

            if(delta < 0x4000) continue;

            return (sample_count / MATH_MAX(1lu, delta)) * X86_64_PIT_BASE_FREQ;
        }
    } else {
        time_t timeout = (TIME_NANOSECONDS_IN_SECOND / TIME_MILLISECONDS_IN_SECOND) * 100;
        uint64_t tsc_start = __rdtsc();
        time_t target = hpet_current_time() + timeout;
        while(hpet_current_time() < target) arch_cpu_relax();
        return (__rdtsc() - tsc_start) * (TIME_NANOSECONDS_IN_SECOND / timeout);
    }
    ASSERT_UNREACHABLE();
}

[[gnu::no_instrument_function]] [[noreturn]] static void init_ap() {
    x86_64_cpu_t *cpu = &g_x86_64_cpus[g_init_ap_cpu_id];
    cpu->self = cpu;
    cpu->sequential_id = g_init_ap_cpu_id;
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) cpu);

    // Initialize event system
    event_init_cpu();

    log(LOG_LEVEL_INFO, "INIT", "Initializing AP %lu", g_init_ap_cpu_id);

    // GDT
    x86_64_gdt_init();

    // Virtual Memory
    pat_init();

    uint64_t cr4 = x86_64_cr4_read();
    cr4 |= 1 << 7; /* CR4.PGE */
    x86_64_cr4_write(cr4);

    ADJUST_STACK(g_hhdm_offset);
    arch_ptm_load_address_space(g_vm_global_address_space);

    // Interrupts
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_APIC));
    x86_64_lapic_init_cpu();
    x86_64_interrupt_load_idt();

    // CPU Local
    x86_64_tss_t *tss = heap_alloc(sizeof(x86_64_tss_t));
    memclear(tss, sizeof(x86_64_tss_t));
    tss->iomap_base = sizeof(x86_64_tss_t);
    x86_64_gdt_load_tss(tss);

    cpu->lapic_id = x86_64_lapic_id();
    cpu->lapic_timer_frequency = calibrate_lapic_timer();
    cpu->tsc_timer_frequency = calibrate_tsc();
    cpu->tss = tss;
    cpu->current_thread = nullptr;
    cpu->common.dw_items = LIST_INIT;
    cpu->common.flags.deferred_work_status = 0;
    cpu->common.flags.in_interrupt_hard = false;
    cpu->common.flags.in_interrupt_soft = false;

    // Initialize FPU
    x86_64_fpu_init_cpu();

    // Setup ISTs
    x86_64_tss_set_ist(tss, 0, HHDM(PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(PMM_FLAG_NONE))) + ARCH_PAGE_GRANULARITY));
    x86_64_tss_set_ist(tss, 1, HHDM(PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(PMM_FLAG_NONE))) + ARCH_PAGE_GRANULARITY));
    x86_64_interrupt_set_ist(2, 1); // Non-maskable
    x86_64_interrupt_set_ist(18, 2); // Machine check

    // Initialize syscalls
    x86_64_syscall_init_cpu();

    // Initialize event timer
    arch_event_timer_setup(g_event_interrupt_vector);

    // Initialize scheduler
    x86_64_sched_init_cpu(cpu);

    log(LOG_LEVEL_DEBUG, "INIT", "AP %lu (Lapic ID: %i) init exit", g_init_ap_cpu_id, x86_64_lapic_id());
    __atomic_add_fetch(&g_init_ap_finished, true, __ATOMIC_SEQ_CST);

    x86_64_sched_handoff_cpu(cpu, false);
    ASSERT_UNREACHABLE();
}

[[gnu::no_instrument_function]] [[noreturn]] void init(elyboot_t *boot_info) {
    memclear(&g_early_bsp, sizeof(g_early_bsp));
    g_early_bsp.self = &g_early_bsp;
    g_early_bsp.sequential_id = boot_info->cpus[boot_info->bsp_index].sequential_id;
    g_early_bsp.common.flags.in_interrupt_hard = false;
    g_early_bsp.common.flags.in_interrupt_soft = false;
    g_early_bsp.common.flags.deferred_work_status = 0;
    g_early_bsp.common.sched.status.preempt_counter = 0;
    g_early_bsp.common.sched.status.yield_immediately = false;
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uintptr_t) &g_early_bsp);

    // Initialize event system
    event_init_cpu();

#ifdef __ENV_DEVELOPMENT
    x86_64_qemu_debug_putc('\n');
    log_sink_add(&g_x86_64_qemu_debug_sink);
#endif

    g_hhdm_offset = boot_info->hhdm_base;
    g_hhdm_size = boot_info->hhdm_size;
    ASSERT(g_hhdm_offset % ARCH_PAGE_GRANULARITY == 0 && g_hhdm_size % ARCH_PAGE_GRANULARITY == 0);

    g_page_cache = (page_t *) boot_info->page_cache_base;
    g_page_cache_size = boot_info->page_cache_size;
    ASSERT((uintptr_t) g_page_cache % ARCH_PAGE_GRANULARITY == 0 && g_page_cache_size % ARCH_PAGE_GRANULARITY == 0);

    if(boot_info->framebuffer_count == 0) panic("INIT", "no framebuffer provided");
    elyboot_framebuffer_t *framebuffer = &boot_info->framebuffers[0];
    g_framebuffer.physical_address = framebuffer->paddr;
    g_framebuffer.size = framebuffer->size;
    g_framebuffer.width = framebuffer->width;
    g_framebuffer.height = framebuffer->height;
    g_framebuffer.pitch = framebuffer->pitch;
    // TODO: handle pixel format

    uint32_t highest_seqid = 0;
    g_x86_64_cpu_count = 0;
    for(elyboot_uint_t i = 0; i < boot_info->cpu_count; i++) {
        if(boot_info->cpus[i].init_state == ELYBOOT_CPU_STATE_FAIL) continue;
        if(boot_info->cpus[i].sequential_id > highest_seqid) highest_seqid = boot_info->cpus[i].sequential_id;
        g_x86_64_cpu_count++;
    }
    if(g_x86_64_cpu_count - 1 != highest_seqid) panic("INIT", "Highest sequential id to cpu count mismatch");

    log(LOG_LEVEL_DEBUG, "INIT", "Counted %lu working CPUs", g_x86_64_cpu_count);
    ASSERT(g_x86_64_cpu_count > 0);

    draw_rect(&g_framebuffer, 0, 0, g_framebuffer.width, g_framebuffer.height, draw_color(14, 14, 15));
    log_sink_add(&g_terminal_sink);
    log(LOG_LEVEL_INFO, "INIT", "Elysium " MACROS_STRINGIFY(__ARCH) " " MACROS_STRINGIFY(__VERSION) " (" __DATE__ " " __TIME__ ")");

    char brand1[12];
    x86_64_cpuid_register(0, X86_64_CPUID_REGISTER_EBX, (uint32_t *) &brand1);
    x86_64_cpuid_register(0, X86_64_CPUID_REGISTER_EDX, (uint32_t *) &brand1[4]);
    x86_64_cpuid_register(0, X86_64_CPUID_REGISTER_ECX, (uint32_t *) &brand1[8]);

    char brand2[48];
    for(size_t i = 0; i < 3; i++) {
        x86_64_cpuid_register(0x80000002 + i, X86_64_CPUID_REGISTER_EAX, (uint32_t *) &brand2[i * 16]);
        x86_64_cpuid_register(0x80000002 + i, X86_64_CPUID_REGISTER_EBX, (uint32_t *) &brand2[i * 16 + 4]);
        x86_64_cpuid_register(0x80000002 + i, X86_64_CPUID_REGISTER_ECX, (uint32_t *) &brand2[i * 16 + 8]);
        x86_64_cpuid_register(0x80000002 + i, X86_64_CPUID_REGISTER_EDX, (uint32_t *) &brand2[i * 16 + 12]);
    }
    log(LOG_LEVEL_INFO, "INIT", "Running on %.*s (%.*s)", 12, brand1, 48, brand2);

    log(LOG_LEVEL_DEBUG, "INIT", "Enumerating %lu modules", boot_info->module_count);
    for(uint16_t i = 0; i < boot_info->module_count; i++) {
        elyboot_module_t *module = &boot_info->modules[i];
        log(LOG_LEVEL_DEBUG, "INIT", "| Module found: `%s`", module->name);

        if(!string_eq("kernel.ksym", module->name)) continue;
        kernel_symbols_load((void *) HHDM(module->paddr));
        log(LOG_LEVEL_DEBUG, "INIT", "| -> Kernel symbols loaded");
    }

    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_MSR));

    x86_64_gdt_init();

    // Initialize exception handling
    x86_64_interrupt_init();
    x86_64_interrupt_load_idt();
    for(int i = 0; i < 32; i++) {
        switch(i) {
            default: x86_64_interrupt_set(i, x86_64_exception_unhandled); break;
        }
    }

    // Initialize physical memory
    log(LOG_LEVEL_DEBUG, "INIT", "Physical Memory Map");
    pmm_zone_t *zones[] = { &g_pmm_zone_low, &g_pmm_zone_normal };
    for(size_t i = 0; i < sizeof(zones) / sizeof(pmm_zone_t *); i++) {
        pmm_zone_t *zone = zones[i];
        log(LOG_LEVEL_DEBUG, "INIT", "» %-6s %#-18lx -> %#-18lx %lu/%lu pages", zone->name, zone->start, zone->end, zone->free_page_count, zone->total_page_count);
    }

    for(size_t i = 0; i < boot_info->mm_entry_count; i++) {
        elyboot_mm_entry_t *entry = &boot_info->mm_entries[i];
        bool used = true;
        switch(entry->type) {
            case ELYBOOT_MM_TYPE_FREE:                   used = false; break;
            case ELYBOOT_MM_TYPE_BOOTLOADER_RECLAIMABLE: break;
            case ELYBOOT_MM_TYPE_EFI_RECLAIMABLE:        break;
            case ELYBOOT_MM_TYPE_ACPI_RECLAIMABLE:       break;
            default:                                     continue;
        }
        ASSERT(entry->address % ARCH_PAGE_GRANULARITY == 0 && entry->length % ARCH_PAGE_GRANULARITY == 0);

        log(LOG_LEVEL_DEBUG, "INIT", ">> %#lx -> %#lx", entry->address, entry->address + entry->length);

        pmm_region_add(entry->address, entry->length, used);
    }

    x86_64_init_flag_set(X86_64_INIT_FLAG_MEMORY_PHYS);

    // Initialize Early Virtual Memory
    pat_init();

    uint64_t cr4 = x86_64_cr4_read();
    cr4 |= 1 << 7; /* CR4.PGE */
    x86_64_cr4_write(cr4);

    x86_64_tlb_init_ipis();

    g_vm_global_address_space = x86_64_ptm_init();
    x86_64_interrupt_set(0xE, x86_64_ptm_page_fault_handler);

    // Protect page cache
    ASSERT(boot_info->page_cache_base % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(g_page_cache_size % ARCH_PAGE_GRANULARITY == 0);
    g_page_cache_region.address_space = g_vm_global_address_space;
    g_page_cache_region.base = boot_info->page_cache_base;
    g_page_cache_region.length = g_page_cache_size;
    g_page_cache_region.protection = VM_PROT_RW;
    g_page_cache_region.cache_behavior = VM_CACHE_STANDARD;
    g_page_cache_region.type = VM_REGION_TYPE_ANON;
    g_page_cache_region.dynamically_backed = false;
    g_page_cache_region.type_data.anon.back_zeroed = false;
    rb_insert(&g_vm_global_address_space->regions, &g_page_cache_region.rb_node);
    log(LOG_LEVEL_DEBUG, "INIT", "Global Region: Page Cache (base: %#lx, size: %#lx)", g_page_cache_region.base, g_page_cache_region.length);

    // Map HHDM
    log(LOG_LEVEL_DEBUG, "INIT", "Mapping HHDM (base: %#lx, size: %#lx)", g_hhdm_offset, g_hhdm_size);
    ASSERT(g_hhdm_offset % ARCH_PAGE_GRANULARITY == 0 && g_hhdm_size % ARCH_PAGE_GRANULARITY == 0);

    g_hhdm_region.address_space = g_vm_global_address_space;
    g_hhdm_region.base = g_hhdm_offset;
    g_hhdm_region.length = g_hhdm_size;
    g_hhdm_region.protection = VM_PROT_RW;
    g_hhdm_region.cache_behavior = VM_CACHE_STANDARD;
    g_hhdm_region.type = VM_REGION_TYPE_DIRECT;
    g_hhdm_region.type_data.direct.physical_address = 0;
    rb_insert(&g_vm_global_address_space->regions, &g_hhdm_region.rb_node);
    log(LOG_LEVEL_DEBUG, "INIT", "Global Region: HHDM (base: %#lx, size: %#lx)", g_hhdm_region.base, g_hhdm_region.length);

    // for(size_t i = 0; i < boot_info->memory_map.size; i++) {
    //     tartarus_memory_map_entry_t *entry = &boot_info->memory_map.entries[i];
    //     if(entry->type != TARTARUS_MEMORY_MAP_TYPE_USABLE) continue;

    //     ASSERT(entry->base + entry->length <= g_hhdm_size);
    //     arch_ptm_map(g_vm_global_address_space, entry->base + g_hhdm_offset, entry->base, entry->length, VM_PROT_RW, VM_CACHE_STANDARD, VM_PRIVILEGE_KERNEL, true);
    // }

    // Map the kernel
    log(LOG_LEVEL_DEBUG, "INIT", "Mapping Kernel (base: %#lx, size: %#lx)", g_kernel_region.base, g_kernel_region.length);
    for(uint64_t i = 0; i < boot_info->kernel_segment_count; i++) {
        elyboot_kernel_segment_t *segment = &boot_info->kernel_segments[i];
        log(LOG_LEVEL_DEBUG,
            "INIT",
            "| Mapping Kernel Segment { %#lx -> %#lx [%c%c%c] }",
            segment->vaddr,
            segment->vaddr + segment->size,
            (segment->flags & ELYBOOT_KERNEL_SEGMENT_FLAG_READ) != 0 ? 'R' : ' ',
            (segment->flags & ELYBOOT_KERNEL_SEGMENT_FLAG_WRITE) != 0 ? 'W' : ' ',
            (segment->flags & ELYBOOT_KERNEL_SEGMENT_FLAG_EXECUTE) != 0 ? 'X' : ' ');

        ASSERT(segment->vaddr % ARCH_PAGE_GRANULARITY == 0 && segment->size % ARCH_PAGE_GRANULARITY == 0);

        vm_map_direct(
            g_vm_global_address_space,
            (void *) segment->vaddr,
            segment->size,
            (vm_protection_t) {
                .read = (segment->flags & ELYBOOT_KERNEL_SEGMENT_FLAG_READ) != 0,
                .write = (segment->flags & ELYBOOT_KERNEL_SEGMENT_FLAG_WRITE) != 0,
                .exec = (segment->flags & ELYBOOT_KERNEL_SEGMENT_FLAG_EXECUTE) != 0,
            },
            VM_CACHE_STANDARD,
            segment->paddr,
            VM_FLAG_FIXED
        );
    }

    // Load the new address space
    log(LOG_LEVEL_DEBUG, "INIT", "Loading global address space...");
    arch_ptm_load_address_space(g_vm_global_address_space);
    x86_64_init_flag_set(X86_64_INIT_FLAG_MEMORY_VIRT);

    // Initialize interrupts
    log(LOG_LEVEL_DEBUG, "INIT", "Initializing interrupts");
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_APIC));
    // TODO: fails ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_ARAT));

    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_TSC));
    // ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_TSC_INVARIANT)); // TODO: doesnt work on tcg

    x86_64_pic8259_remap();
    x86_64_pic8259_disable();
    x86_64_lapic_init();
    x86_64_lapic_init_cpu();
    g_x86_64_interrupt_irq_eoi = x86_64_lapic_eoi;
    x86_64_init_flag_set(X86_64_INIT_FLAG_INTERRUPTS);

    // Initialize HEAP
    log(LOG_LEVEL_DEBUG, "INIT", "Initializing slab & heap");
    slab_init();
    heap_initialize();

    // Initialize FPU
    x86_64_fpu_init();
    x86_64_fpu_init_cpu();

    // Initialize ACPI
    log(LOG_LEVEL_DEBUG, "INIT", "RSDP at %#lx", boot_info->acpi_rsdp);
    g_acpi_rsdp = boot_info->acpi_rsdp;

    uacpi_status ret = uacpi_initialize(0);
    if(uacpi_unlikely_error(ret)) panic("INIT", "UACPI initialization failed (%s)", uacpi_status_to_string(ret));

    // Initialize IOAPIC
    uacpi_table madt;
    ret = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt);
    if(uacpi_likely_success(ret)) {
        x86_64_ioapic_init((struct acpi_madt *) madt.hdr);
        uacpi_table_unref(&madt);
    }

    // Initialize PCI
    uacpi_table mcfg;
    ret = uacpi_table_find_by_signature(ACPI_MCFG_SIGNATURE, &mcfg);
    if(uacpi_likely_success(ret)) {
        pci_enumerate((struct acpi_mcfg *) mcfg.hdr);
        uacpi_table_unref(&mcfg);
    }

    // Initialize timer
    uacpi_table hpet;
    ret = uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &hpet);
    if(uacpi_likely_success(ret)) {
        x86_64_hpet_init((struct acpi_hpet *) hpet.hdr);
        uacpi_table_unref(&hpet);
        g_hpet_initialized = true;
    }

    time_frequency_t lapic_timer_freq = calibrate_lapic_timer();
    time_frequency_t tsc_timer_freq = calibrate_tsc();

    log(LOG_LEVEL_DEBUG, "INIT", "BSP Local Apic Timer calibrated, freq: %lu", lapic_timer_freq);
    log(LOG_LEVEL_DEBUG, "INIT", "BSP TSC calibrated, freq: %lu", tsc_timer_freq);

    // Initialize event system
    g_event_interrupt_vector = arch_interrupt_request(INTERRUPT_PRIORITY_EVENT, events_process);
    if(g_event_interrupt_vector < 0) panic("INIT", "Failed to acquire interrupt vector for event handler");

    event_init();

    // SMP init
    g_x86_64_cpus = heap_alloc(sizeof(x86_64_cpu_t) * boot_info->cpu_count);
    memclear(g_x86_64_cpus, sizeof(x86_64_cpu_t) * boot_info->cpu_count);

    x86_64_tss_t *tss = heap_alloc(sizeof(x86_64_tss_t));
    memclear(tss, sizeof(x86_64_tss_t));
    tss->iomap_base = sizeof(x86_64_tss_t);
    x86_64_gdt_load_tss(tss);

    log(LOG_LEVEL_DEBUG, "INIT", "> BSP(%lu)", boot_info->bsp_index);

    x86_64_cpu_t *cpu = nullptr;
    for(size_t i = 0; i < boot_info->cpu_count; i++) {
        if(boot_info->cpus[i].init_state == ELYBOOT_CPU_STATE_FAIL) continue;

        if(i == boot_info->bsp_index) {
            cpu = &g_x86_64_cpus[boot_info->cpus[i].sequential_id];
            cpu->self = cpu;
            cpu->sequential_id = boot_info->cpus[i].sequential_id;
            cpu->lapic_id = x86_64_lapic_id();
            cpu->lapic_timer_frequency = lapic_timer_freq;
            cpu->tsc_timer_frequency = tsc_timer_freq;
            cpu->tss = tss;
            cpu->current_thread = nullptr;
            cpu->common.dw_items = LIST_INIT;
            cpu->common.flags.deferred_work_status = 0;
            cpu->common.flags.in_interrupt_hard = false;
            cpu->common.flags.in_interrupt_soft = false;
            x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) cpu);

            event_init_cpu();
            continue;
        }

        g_init_ap_cpu_id = boot_info->cpus[i].sequential_id;
        g_init_ap_finished = false;

        __atomic_store_n(boot_info->cpus[i].wake_on_write, (uintptr_t) init_ap, __ATOMIC_RELEASE);
        while(!g_init_ap_finished);
    }
    ASSERT(cpu != nullptr);

    log(LOG_LEVEL_DEBUG, "INIT", "SMP init done (%lu/%lu cpus initialized)", g_x86_64_cpu_count, boot_info->cpu_count);
    log(LOG_LEVEL_DEBUG, "INIT", "BSP seqid is %lu", X86_64_CPU_CURRENT.sequential_id);
    x86_64_init_flag_set(X86_64_INIT_FLAG_SMP);

    // Initialize ISTs
    x86_64_tss_set_ist(tss, 0, HHDM(PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(PMM_FLAG_NONE))) + ARCH_PAGE_GRANULARITY));
    x86_64_tss_set_ist(tss, 1, HHDM(PAGE_PADDR(PAGE_FROM_BLOCK(pmm_alloc_page(PMM_FLAG_NONE))) + ARCH_PAGE_GRANULARITY));
    x86_64_interrupt_set_ist(2, 1); // Non-maskable
    x86_64_interrupt_set_ist(18, 2); // Machine check

    x86_64_init_flag_set(X86_64_INIT_FLAG_TIME);

    // Initialize Deferred Work
    dw_init();

    // Initialize syscalls
    x86_64_syscall_init_cpu();

    // Initialize event timer
    arch_event_timer_setup(g_event_interrupt_vector);

    // Initialize VFS
    elyboot_module_t *sysroot_module = nullptr;
    for(uint16_t i = 0; i < boot_info->module_count; i++) {
        elyboot_module_t *module = &boot_info->modules[i];
        if(!string_eq(module->name, "root.rdk")) continue;
        sysroot_module = module;
        break;
    }
    if(sysroot_module == nullptr) panic("INIT", "could not locate root.rdk");

    vfs_result_t res = vfs_mount(&g_rdsk_ops, nullptr, (void *) HHDM(sysroot_module->paddr));
    if(res != VFS_RESULT_OK) panic("INIT", "failed to mount rdsk (%i)", res);

    vfs_node_t *root_node;
    res = vfs_root(&root_node);
    ASSERT(res == VFS_RESULT_OK);

    log(LOG_LEVEL_DEBUG, "INIT", "| FS Root Listing");
    for(size_t i = 0;;) {
        const char *filename;
        res = root_node->ops->readdir(root_node, &i, &filename);
        ASSERT(res == VFS_RESULT_OK);
        if(filename == nullptr) break;
        log(LOG_LEVEL_DEBUG, "INIT", "| - %s", filename);
    }

    res = vfs_mount(&g_tmpfs_ops, "/tmp", nullptr);
    if(res != VFS_RESULT_OK) panic("INIT", "failed to mount /tmp (%i)", res);

    x86_64_init_flag_set(X86_64_INIT_FLAG_VFS);

    // Initialize sched
    x86_64_sched_init_cpu(X86_64_CPU_CURRENT.self);

#ifdef __ENV_DEVELOPMENT
    // Find modules dir
    vfs_node_t *modules_dir = nullptr;
    res = vfs_lookup(&VFS_ABSOLUTE_PATH("/sys/modules"), &modules_dir);
    if(res != VFS_RESULT_OK) panic("INIT", "failed to lookup modules directory (%i)", res);
    if(modules_dir == nullptr) panic("INIT", "no modules directory found");

    // Run Test Modules
    static const char *test_modules[] = { "test_pmm.cronmod", "test_vm.cronmod" };

    for(size_t i = 0; i < sizeof(test_modules) / sizeof(char *); i++) {
        vfs_node_t *test_module_file = nullptr;
        res = vfs_lookup(&(vfs_path_t) { .root = modules_dir, .relative_path = test_modules[i] }, &test_module_file);
        if(res != VFS_RESULT_OK) panic("INIT", "failed to lookup `%s` module (%i)", test_modules[i], res);
        if(test_module_file == nullptr) panic("INIT", "no `%s` module found", test_modules[i]);

        module_t *module;
        module_result_t mres = module_load(test_module_file, &module);
        if(mres != MODULE_RESULT_OK) panic("INIT", "failed to load `%s` module `%s`", test_modules[i], module_result_stringify(mres));

        if(module->initialize != nullptr) module->initialize();
        if(module->uninitialize != nullptr) module->uninitialize();
    }
#endif

    // Schedule init threads
    sched_thread_schedule(reaper_create());
    sched_thread_schedule(arch_sched_thread_create_kernel(thread_init));

    {
        log(LOG_LEVEL_DEBUG, "INIT", "loading /usr/bin/init");
        vm_address_space_t *as = arch_ptm_address_space_create();

        vfs_node_t *init_exec;
        res = vfs_lookup(&VFS_ABSOLUTE_PATH("/usr/bin/init"), &init_exec);
        if(res != VFS_RESULT_OK) panic("INIT", "failed to lookup init executable (%i)", res);
        if(init_exec == nullptr) panic("INIT", "no init executable found");

        elf_file_t *init_elf;
        elf_result_t elf_res = elf_read(init_exec, &init_elf);
        if(elf_res != ELF_RESULT_OK) panic("INIT", "could not read init (%i)", elf_res);

        elf_res = elf_load(init_elf, as);
        if(elf_res != ELF_RESULT_OK) panic("INIT", "could not load init (%i)", elf_res);

        uintptr_t entry;
        char *interpreter;
        elf_res = elf_lookup_interpreter(init_elf, &interpreter);
        switch(elf_res) {
            case ELF_RESULT_OK:
                log(LOG_LEVEL_DEBUG, "INIT", "found init interpreter `%s`", interpreter);

                vfs_node_t *interpreter_exec;
                res = vfs_lookup(&VFS_ABSOLUTE_PATH(interpreter), &interpreter_exec);
                if(res != VFS_RESULT_OK) panic("INIT", "failed to lookup interpreter for init (%i)", res);
                if(interpreter_exec == nullptr) panic("INIT", "no interpreter found for init executable");

                elf_file_t *interpreter_elf;
                elf_res = elf_read(interpreter_exec, &interpreter_elf);
                if(elf_res != ELF_RESULT_OK) panic("INIT", "could not read interpreter for init (%i)", elf_res);

                elf_res = elf_load(interpreter_elf, as);
                if(elf_res != ELF_RESULT_OK) panic("INIT", "could not load interpreter for init (%i)", elf_res);

                entry = interpreter_elf->entry;
                heap_free(interpreter_elf, sizeof(elf_file_t));
                break;
            case ELF_RESULT_ERR_NOT_FOUND: entry = init_elf->entry; break;
            default:                       panic("INIT", "elf interpreter lookup failed (%i)", elf_res);
        }

        x86_64_auxv_t auxv = { .entry = init_elf->entry };

        uintptr_t phdr_addr;
        elf_res = elf_lookup_phdr_address(init_elf, &phdr_addr);
        if(elf_res == ELF_RESULT_OK) {
            auxv.phdr = phdr_addr;
            auxv.phnum = init_elf->program_headers.count;
            auxv.phent = init_elf->program_headers.entry_size;
        }

        heap_free(init_elf, sizeof(elf_file_t));

        log(LOG_LEVEL_DEBUG, "INIT", "auxv { entry: %#lx; phdr: %#lx; phent: %#lx; phnum: %#lx; }", auxv.entry, auxv.phdr, auxv.phent, auxv.phnum);

        char *argv[] = { "/usr/bin/init", nullptr };
        char *envp[] = { nullptr };

        process_t *proc = process_create(as);
        uintptr_t thread_stack = x86_64_sysv_stack_setup(proc->address_space, ARCH_PAGE_GRANULARITY * 8, argv, envp, &auxv);
        thread_t *thread = arch_sched_thread_create_user(proc, entry, thread_stack);

        log(LOG_LEVEL_DEBUG, "INIT", "init thread >> entry: %#lx, stack: %#lx", entry, thread_stack);

        sched_thread_schedule(thread);
    }

    // Scheduler handoff
    log(LOG_LEVEL_INFO, "INIT", "Reached scheduler handoff. Bye for now!");
    x86_64_sched_handoff_cpu(cpu, true);
    ASSERT_UNREACHABLE();
}

bool x86_64_init_flag_check(size_t flags) {
    return (flags & g_init_flags) == flags;
}

void x86_64_init_flag_set(size_t flags) {
    g_init_flags |= flags;
}
