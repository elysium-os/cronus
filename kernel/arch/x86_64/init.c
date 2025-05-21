#include "init.h"

#include "abi/sysv/elf.h"
#include "arch/debug.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/log.h"
#include "common/panic.h"
#include "dev/acpi/acpi.h"
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
#include "ring_buffer.h"
#include "sched/process.h"
#include "sched/reaper.h"
#include "sched/sched.h"
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

#include <stddef.h>
#include <stdint.h>
#include <tartarus.h>
#include <uacpi/uacpi.h>

#define PIT_TIMER_FREQ 1'000
#define LAPIC_CALIBRATION_TICKS 0x1'0000

#define ADJUST_STACK(OFFSET) asm volatile("mov %%rsp, %%rax\nadd %0, %%rax\nmov %%rax, %%rsp\nmov %%rbp, %%rax\nadd %0, %%rax\nmov %%rax, %%rbp" : : "rm"(OFFSET) : "rax", "memory")

uintptr_t g_hhdm_offset;
size_t g_hhdm_size;

size_t g_x86_64_cpu_count;
x86_64_cpu_t *g_x86_64_cpus;

framebuffer_t g_framebuffer;

page_t *g_page_cache;
size_t g_page_cache_size;

static volatile size_t g_init_cpu_id_counter = 0;

static size_t g_init_flags = 0;

static vm_region_t g_hhdm_region, g_kernel_region, g_page_cache_region;

static x86_64_cpu_t g_early_bsp;

static uintptr_t g_bootmem_base;
static size_t g_bootmem_size;

static uintptr_t bootmem_alloc() {
    if(g_bootmem_size == 0) panic("BOOTMEM", "out of memory");
    uintptr_t address = g_bootmem_base;
    g_bootmem_base += ARCH_PAGE_GRANULARITY;
    g_bootmem_size -= ARCH_PAGE_GRANULARITY;
    memclear((void *) HHDM(address), ARCH_PAGE_GRANULARITY);
    return address;
}

static uintptr_t proper_alloc() {
    return pmm_alloc_page(PMM_FLAG_ZERO)->paddr;
}

static void thread_init() {
    // OPTIMIZE we can mask interrupts for uacpi perf lol

    uacpi_status ret = uacpi_initialize(0);
    if(uacpi_unlikely_error(ret)) {
        log(LOG_LEVEL_WARN, "UACPI", "initialization failed (%s)", uacpi_status_to_string(ret));
    } else {
        ret = uacpi_namespace_load();
        if(uacpi_unlikely_error(ret)) log(LOG_LEVEL_WARN, "UACPI", "namespace load failed (%s)", uacpi_status_to_string(ret));
    }

    log(LOG_LEVEL_INFO, "UACPI", "Setup Done");
}

[[noreturn]] static void init_ap() {
    log(LOG_LEVEL_INFO, "INIT", "Initializing AP %i", x86_64_lapic_id());

    x86_64_gdt_init();

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
    x86_64_cpu_t *cpu = &g_x86_64_cpus[g_init_cpu_id_counter];
    cpu->self = cpu;
    cpu->sequential_id = g_init_cpu_id_counter;
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) cpu);

    // Interrupts
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_APIC));
    x86_64_lapic_initialize();
    x86_64_interrupt_load_idt();

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
    cpu->current_thread = nullptr;

    // Initialize FPU
    x86_64_fpu_init_cpu();

    // Setup ISTs
    x86_64_tss_set_ist(tss, 0, HHDM(pmm_alloc_page(PMM_FLAG_NONE)->paddr + ARCH_PAGE_GRANULARITY));
    x86_64_tss_set_ist(tss, 1, HHDM(pmm_alloc_page(PMM_FLAG_NONE)->paddr + ARCH_PAGE_GRANULARITY));
    x86_64_interrupt_set_ist(2, 1); // Non-maskable
    x86_64_interrupt_set_ist(18, 2); // Machine check

    // Initialize syscalls
    x86_64_syscall_init_cpu();

    // Initialize scheduler
    x86_64_sched_init_cpu(cpu);

    log(LOG_LEVEL_DEBUG, "INIT", "AP %lu:%i init exit", g_init_cpu_id_counter, x86_64_lapic_id());
    __atomic_add_fetch(&g_init_cpu_id_counter, 1, __ATOMIC_SEQ_CST);

    x86_64_sched_handoff_cpu(cpu, false);
    ASSERT_UNREACHABLE();
}

[[noreturn]] void init(tartarus_boot_info_t *boot_info) {
    g_hhdm_offset = boot_info->hhdm.offset;
    g_hhdm_size = boot_info->hhdm.size;

    if(boot_info->framebuffer_count == 0) panic("INIT", "no framebuffer provided");
    tartarus_framebuffer_t *framebuffer = &boot_info->framebuffers[0];
    g_framebuffer.physical_address = HHDM_TO_PHYS(framebuffer->address);
    g_framebuffer.size = framebuffer->size;
    g_framebuffer.width = framebuffer->width;
    g_framebuffer.height = framebuffer->height;
    g_framebuffer.pitch = framebuffer->pitch;
    // TODO handle pixel format

    size_t bsp_seq_id = 0;
    for(size_t i = 0; i < boot_info->cpu_count; i++) {
        if(boot_info->cpus[i].initialization_failed) continue;
        if(i == boot_info->bsp_index) bsp_seq_id = g_x86_64_cpu_count;
        g_x86_64_cpu_count++;
    }

    memclear(&g_early_bsp, sizeof(g_early_bsp));
    g_early_bsp.self = &g_early_bsp;
    g_early_bsp.sequential_id = bsp_seq_id;
    x86_64_msr_write(X86_64_MSR_GS_BASE, (uintptr_t) &g_early_bsp);

#ifdef __ENV_DEVELOPMENT
    x86_64_qemu_debug_putc('\n');
    log_sink_add(&g_x86_64_qemu_debug_sink);
#endif

    log_sink_add(&g_ring_buffer_sink);
    log_sink_add(&g_terminal_sink);

    log(LOG_LEVEL_DEBUG, "INIT", "Counted %lu working CPUs", g_x86_64_cpu_count);
    ASSERT(g_x86_64_cpu_count > 0);

    draw_rect(&g_framebuffer, 0, 0, g_framebuffer.width, g_framebuffer.height, draw_color(14, 14, 15));
    log(LOG_LEVEL_INFO, "INIT", "Elysium " STRING_MACRO_STRINGIFY(__ARCH) " " STRING_MACRO_STRINGIFY(__VERSION) " (" __DATE__ " " __TIME__ ")");

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

    log(LOG_LEVEL_DEBUG, "INIT", "Enumerating %u modules", boot_info->module_count);
    for(uint16_t i = 0; i < boot_info->module_count; i++) {
        tartarus_module_t *module = &boot_info->modules[i];
        log(LOG_LEVEL_DEBUG, "INIT", "| Module found: `%s`", module->name);

        if(!string_eq("kernel.ksym", module->name)) continue;
        kernel_symbols_load((void *) HHDM(module->paddr));
        log(LOG_LEVEL_DEBUG, "INIT", "| -> Kernel symbols loaded");
    }

    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_MSR));

    x86_64_gdt_init();

    // Initialize interrupt & exception handling
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_APIC));
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
    x86_64_init_flag_set(X86_64_INIT_FLAG_INTERRUPTS);

    // Initialize early physical memory
    size_t early_mem_index = ({
        size_t largest_index = 0;
        size_t largest_size = 0;
        for(int i = 0; i < boot_info->memory_map.size; i++) {
            tartarus_memory_map_entry_t entry = boot_info->memory_map.entries[i];
            if(entry.type != TARTARUS_MEMORY_MAP_TYPE_USABLE) continue;
            if(entry.length < largest_size) continue;
            largest_index = i;
            largest_size = entry.length;
        }
        largest_index;
    });
    g_bootmem_base = boot_info->memory_map.entries[early_mem_index].base;
    g_bootmem_size = boot_info->memory_map.entries[early_mem_index].length;
    log(LOG_LEVEL_DEBUG, "INIT", "bootmem region (base: %#lx, size: %#lx)", g_bootmem_base, g_bootmem_size);

    g_x86_64_ptm_phys_allocator = bootmem_alloc;
    x86_64_init_flag_set(X86_64_INIT_FLAG_MEMORY_PHYS_EARLY);

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
    g_hhdm_region.protection = (vm_protection_t) { .read = true, .write = true };
    g_hhdm_region.cache_behavior = VM_CACHE_STANDARD;
    g_hhdm_region.type = VM_REGION_TYPE_DIRECT;
    g_hhdm_region.type_data.direct.physical_address = 0;
    list_push(&g_vm_global_address_space->regions, &g_hhdm_region.list_node);

    g_kernel_region.address_space = g_vm_global_address_space;
    g_kernel_region.base = MATH_FLOOR(boot_info->kernel.vaddr, ARCH_PAGE_GRANULARITY);
    g_kernel_region.length = MATH_CEIL(boot_info->kernel.size, ARCH_PAGE_GRANULARITY);
    g_kernel_region.protection = (vm_protection_t) { .read = true, .write = true };
    g_kernel_region.cache_behavior = VM_CACHE_STANDARD;
    g_kernel_region.type = VM_REGION_TYPE_ANON;
    list_push(&g_vm_global_address_space->regions, &g_kernel_region.list_node);

    ADJUST_STACK(g_hhdm_offset);
    arch_ptm_load_address_space(g_vm_global_address_space);
    x86_64_init_flag_set(X86_64_INIT_FLAG_MEMORY_VIRT);

    // Initialize physical memory
    uintptr_t early_mem_base = g_bootmem_base;
    size_t early_mem_size = g_bootmem_size;

    uintptr_t pagecache_start = boot_info->hhdm.offset + boot_info->hhdm.size;
    size_t pagecache_end = pagecache_start;
    g_page_cache = (page_t *) pagecache_start;

    for(size_t i = 0; i < boot_info->memory_map.size; i++) {
        tartarus_memory_map_entry_t entry = boot_info->memory_map.entries[i];
        if(entry.type != TARTARUS_MEMORY_MAP_TYPE_USABLE) continue;

        g_bootmem_base = early_mem_index == i ? early_mem_base : entry.base;
        g_bootmem_size = early_mem_index == i ? early_mem_size : entry.length;

        uintptr_t start = pagecache_start + MATH_FLOOR((entry.base / ARCH_PAGE_GRANULARITY) * sizeof(page_t), ARCH_PAGE_GRANULARITY);
        uintptr_t end = pagecache_start + MATH_CEIL(((entry.base + entry.length) / ARCH_PAGE_GRANULARITY) * sizeof(page_t), ARCH_PAGE_GRANULARITY);
        if(start > pagecache_end) pagecache_end = start;
        for(; pagecache_end < end; pagecache_end += ARCH_PAGE_GRANULARITY) {
            arch_ptm_map(g_vm_global_address_space, pagecache_end, bootmem_alloc(), (vm_protection_t) { .read = true, .write = true }, VM_CACHE_STANDARD, VM_PRIVILEGE_KERNEL, true);
        }

        pmm_region_add(entry.base, entry.length, (g_bootmem_base / ARCH_PAGE_GRANULARITY) - (entry.base / ARCH_PAGE_GRANULARITY));
    }
    g_page_cache_size = pagecache_end - pagecache_start;

    ASSERT(pagecache_start % ARCH_PAGE_GRANULARITY == 0);
    ASSERT(g_page_cache_size % ARCH_PAGE_GRANULARITY == 0);
    g_page_cache_region.address_space = g_vm_global_address_space;
    g_page_cache_region.base = pagecache_start;
    g_page_cache_region.length = g_page_cache_size;
    g_page_cache_region.protection = (vm_protection_t) { .read = true, .write = true };
    g_page_cache_region.cache_behavior = VM_CACHE_STANDARD;
    g_page_cache_region.type = VM_REGION_TYPE_ANON; // TODO: this is a lie
    g_page_cache_region.type_data.anon.back_zeroed = false;
    list_push(&g_vm_global_address_space->regions, &g_page_cache_region.list_node);
    log(LOG_LEVEL_DEBUG, "INIT", "Page Cache (base: %#lx, size: %#lx)", pagecache_start, g_page_cache_size);

    g_x86_64_ptm_phys_allocator = proper_alloc;
    x86_64_init_flag_set(X86_64_INIT_FLAG_MEMORY_PHYS);

    log(LOG_LEVEL_DEBUG, "INIT", "Physical Memory Map");
    pmm_zone_t *zones[] = { &g_pmm_zone_low, &g_pmm_zone_normal };
    for(size_t i = 0; i < sizeof(zones) / sizeof(pmm_zone_t *); i++) {
        pmm_zone_t *zone = zones[i];
        log(LOG_LEVEL_DEBUG, "INIT", "» %-6s %#-18lx -> %#-18lx %lu/%lu pages", zone->name, zone->start, zone->end, zone->free_page_count, zone->total_page_count);
    }

    // Initialize HEAP
    slab_init();
    heap_initialize();

    // Initialize FPU
    x86_64_fpu_init();
    x86_64_fpu_init_cpu();

    // Initialize ACPI
    log(LOG_LEVEL_DEBUG, "INIT", "RSDP at %#lx", boot_info->acpi_rsdp_address);
    acpi_init(boot_info->acpi_rsdp_address);

    // Initialize IOAPIC
    acpi_sdt_header_t *madt = acpi_find_table((uint8_t *) "APIC");
    if(madt != nullptr) x86_64_ioapic_init(madt);

    // Initialize PCI
    acpi_sdt_header_t *mcfg = acpi_find_table((uint8_t *) "MCFG");
    pci_enumerate(mcfg);

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

    log(LOG_LEVEL_DEBUG, "INIT", "> BSP(%u)", boot_info->bsp_index);

    x86_64_cpu_t *cpu = nullptr;
    for(size_t i = 0; i < boot_info->cpu_count; i++) {
        if(boot_info->cpus[i].initialization_failed) continue;

        if(i == boot_info->bsp_index) {
            cpu = &g_x86_64_cpus[g_init_cpu_id_counter];
            *cpu = g_early_bsp;
            cpu->self = cpu;
            cpu->sequential_id = g_init_cpu_id_counter;
            ASSERT(g_init_cpu_id_counter == bsp_seq_id);
            cpu->lapic_id = x86_64_lapic_id();
            cpu->lapic_timer_frequency = (uint64_t) (LAPIC_CALIBRATION_TICKS / (start_count - end_count)) * X86_64_PIT_BASE_FREQ;
            cpu->tss = tss;
            cpu->current_thread = nullptr;
            x86_64_msr_write(X86_64_MSR_GS_BASE, (uint64_t) cpu);
            g_init_cpu_id_counter++;
            continue;
        }

        size_t previous_count = g_init_cpu_id_counter;
        __atomic_store_n(boot_info->cpus[i].wake_on_write, (uintptr_t) init_ap, __ATOMIC_RELEASE);
        while(previous_count >= g_init_cpu_id_counter);
    }
    ASSERT(cpu != nullptr);
    ASSERT(g_init_cpu_id_counter == g_x86_64_cpu_count);

    log(LOG_LEVEL_DEBUG, "INIT", "SMP init done (%lu/%i cpus initialized)", g_init_cpu_id_counter, boot_info->cpu_count);
    log(LOG_LEVEL_DEBUG, "INIT", "BSP seqid is %lu", X86_64_CPU_CURRENT.sequential_id);
    x86_64_init_flag_set(X86_64_INIT_FLAG_SMP);

    // Initialize ISTs
    x86_64_tss_set_ist(tss, 0, HHDM(pmm_alloc_page(PMM_FLAG_NONE)->paddr + ARCH_PAGE_GRANULARITY));
    x86_64_tss_set_ist(tss, 1, HHDM(pmm_alloc_page(PMM_FLAG_NONE)->paddr + ARCH_PAGE_GRANULARITY));
    x86_64_interrupt_set_ist(2, 1); // Non-maskable
    x86_64_interrupt_set_ist(18, 2); // Machine check

    // Initialize timer
    acpi_sdt_header_t *hpet_header = acpi_find_table((uint8_t *) "HPET");
    if(hpet_header != nullptr) {
        x86_64_hpet_init(hpet_header);
        time_source_register(&g_hpet_time_source);
        //     int timer = x86_64_hpet_find_timer(true);
        //     if(timer == -1) goto use_pit;
        //     int gsi_vector = x86_64_hpet_configure_timer(timer, HPET_TIMER_FREQ, false);

        //     int hpet_time_vector = x86_64_interrupt_request(IPL_CRITICAL, hpet_time_handler);
        //     ASSERT(hpet_time_vector != -1);
        //     x86_64_ioapic_map_gsi(gsi_vector, x86_64_lapic_id(), false, true, hpet_time_vector);
    } else {
        // use_pit:
        // x86_64_pit_match_frequency(PIT_TIMER_FREQ);
        // int pit_time_vector = x86_64_interrupt_request(IPL_CRITICAL, pit_time_handler);
        // ASSERT(pit_time_vector != -1);
        // x86_64_ioapic_map_legacy_irq(0, x86_64_lapic_id(), false, true, pit_time_vector);
    }

    x86_64_init_flag_set(X86_64_INIT_FLAG_TIME);

    // Initialize syscalls
    x86_64_syscall_init_cpu();

    // Initialize VFS
    tartarus_module_t *sysroot_module = nullptr;
    for(uint16_t i = 0; i < boot_info->module_count; i++) {
        tartarus_module_t *module = &boot_info->modules[i];
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

    // Flush ring buffer
    vfs_node_t *log_node;
    res = vfs_mkfile(&VFS_ABSOLUTE_PATH("/tmp"), "kernel.log", &log_node);
    if(res != VFS_RESULT_OK) panic("INIT", "failed to make log file (%i)", res);

    size_t count;
    res = log_node->ops->rw(log_node, &(vfs_rw_t) { .rw = VFS_RW_WRITE, .size = g_ring_buffer.size - g_ring_buffer.index, .offset = 0, .buffer = &g_ring_buffer.buffer[g_ring_buffer.index] }, &count);
    if(res != VFS_RESULT_OK || count != g_ring_buffer.size - g_ring_buffer.index) panic("INIT", "failed to flush ring buffer");
    if(g_ring_buffer.full) {
        res = log_node->ops->rw(log_node, &(vfs_rw_t) { .rw = VFS_RW_WRITE, .size = g_ring_buffer.index, .offset = g_ring_buffer.size - g_ring_buffer.index, .buffer = &g_ring_buffer.buffer }, &count);
        if(res != VFS_RESULT_OK || count != g_ring_buffer.index) panic("INIT", "failed to flush ring buffer");
    }

    // Initialize sched
    x86_64_sched_init();
    x86_64_sched_init_cpu(X86_64_CPU_CURRENT.self);

    // Load modules
    vfs_node_t *test_module_file = nullptr;
    vfs_lookup(&VFS_ABSOLUTE_PATH("/sys/modules/test.cronmod"), &test_module_file);
    if(res != VFS_RESULT_OK) panic("INIT", "failed to lookup test module (%i)", res);
    if(test_module_file == nullptr) panic("INIT", "no test module found");

    module_t *module;
    module_result_t mres = module_load(test_module_file, &module);
    if(mres != MODULE_RESULT_OK) panic("INIT", "failed to load test module `%s`", module_result_stringify(mres));

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
