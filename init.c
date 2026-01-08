#include "sys/init.h"

#include "arch/cpu.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/log.h"
#include "dev/acpi.h"
#include "fs/rdsk.h"
#include "fs/tmpfs.h"
#include "fs/vfs.h"
#include "graphics/framebuffer.h"
#include "lib/atomic.h"
#include "lib/barrier.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "memory/earlymem.h"
#include "memory/heap.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/pmm.h"
#include "memory/vm.h"
#include "sched/reaper.h"
#include "sys/cpu.h"
#include "sys/hook.h"
#include "sys/kernel_symbol.h"
#include "sys/module.h"

#include <stddef.h>
#include <tartarus.h>
#include <uacpi/uacpi.h>

framebuffer_t g_framebuffer;
size_t g_cpu_count = 0;

uintptr_t g_hhdm_offset;
size_t g_hhdm_size;

page_t *g_page_db;
size_t g_page_db_size;

static vm_region_t g_hhdm_region, g_page_db_region;

ATOMIC static bool g_init_ap_finished = false;
ATOMIC static uint64_t g_init_ap_cpu_id = 0;

static cpu_t g_early_bsp;

static void thread_init() {
    uacpi_status ret = uacpi_namespace_load();
    if(uacpi_unlikely_error(ret)) log(LOG_LEVEL_WARN, "UACPI", "namespace load failed (%s)", uacpi_status_to_string(ret));
    log(LOG_LEVEL_INFO, "UACPI", "Setup Done");
}

[[gnu::no_instrument_function]] [[noreturn]] static void init_ap() {
    size_t id = ATOMIC_LOAD(&g_init_ap_cpu_id, ATOMIC_SEQ_CST);
    arch_cpu_local_load(&g_cpu_list[id]);
    HOOK_RUN(init_cpu_local);

    log(LOG_LEVEL_INFO, "INIT", "Initializing AP %lu", id);

    init_run_stage(INIT_STAGE_BOOT, true);
    init_run_stage(INIT_STAGE_EARLY, true);

    arch_ptm_load_address_space(g_vm_global_address_space);

    init_run_stage(INIT_STAGE_BEFORE_MAIN, true);
    init_run_stage(INIT_STAGE_MAIN, true);

    init_run_stage(INIT_STAGE_BEFORE_DEV, true);
    init_run_stage(INIT_STAGE_DEV, true);

    init_run_stage(INIT_STAGE_LATE, true);

    log(LOG_LEVEL_DEBUG, "INIT", "AP %lu init exit", id);
    ATOMIC_FETCH_ADD(&g_init_ap_finished, true, ATOMIC_SEQ_CST);

    arch_sched_handoff_cpu();
    ASSERT_UNREACHABLE();
}

[[gnu::no_instrument_function]] [[noreturn]] void init(tartarus_boot_info_t *boot_info) {
    g_early_bsp.self = &g_early_bsp;
    g_early_bsp.sequential_id = boot_info->cpus[boot_info->bsp_index].sequential_id;
    arch_cpu_local_load(&g_early_bsp);
    HOOK_RUN(init_cpu_local);

    init_run_stage(INIT_STAGE_BOOT, false);

    log(LOG_LEVEL_INFO, "INIT", "Elysium " MACROS_STRINGIFY(__ARCH) " " MACROS_STRINGIFY(__VERSION) " (" __DATE__ " " __TIME__ ")");

    // Initialize HHDM
    ASSERT(boot_info->hhdm_offset % ARCH_PAGE_GRANULARITY == 0 && boot_info->hhdm_offset % ARCH_PAGE_GRANULARITY == 0);
    g_hhdm_offset = boot_info->hhdm_offset;
    g_hhdm_size = boot_info->hhdm_size;

    // Count CPUs and ensure seqids make sense
    uint32_t highest_seqid = 0;
    for(tartarus_size_t i = 0; i < boot_info->cpu_count; i++) {
        if(boot_info->cpus[i].init_state == TARTARUS_CPU_STATE_FAIL) continue;
        if(boot_info->cpus[i].sequential_id > highest_seqid) highest_seqid = boot_info->cpus[i].sequential_id;
        g_cpu_count++;
    }
    if(g_cpu_count - 1 != highest_seqid) panic("INIT", "Highest sequential id to cpu count mismatch");

    log(LOG_LEVEL_DEBUG, "INIT", "Counted %lu working CPUs", g_cpu_count);
    ASSERT(g_cpu_count > 0);

    // Initialize Early Memory
    for(size_t i = 0; i < boot_info->mm_entry_count; i++) {
        tartarus_mm_entry_t *entry = &boot_info->mm_entries[i];

        for(size_t j = i + 1; j < boot_info->mm_entry_count; j++) { ASSERT((entry->base >= (boot_info->mm_entries[j].base + boot_info->mm_entries[j].length)) || (boot_info->mm_entries[j].base >= (entry->base + entry->length))); }

        switch(entry->type) {
            case TARTARUS_MM_TYPE_USABLE: break;

            case TARTARUS_MM_TYPE_BOOTLOADER_RECLAIMABLE:
            case TARTARUS_MM_TYPE_EFI_RECLAIMABLE:
            case TARTARUS_MM_TYPE_ACPI_RECLAIMABLE:
            case TARTARUS_MM_TYPE_ACPI_NVS:
            case TARTARUS_MM_TYPE_RESERVED:
            case TARTARUS_MM_TYPE_BAD:                    continue;
        }

        ASSERT(entry->base % ARCH_PAGE_GRANULARITY == 0 && entry->length % ARCH_PAGE_GRANULARITY == 0);

        earlymem_region_add(entry->base, entry->length); // TODO: coalesce these regions so that we dont end up with weird max_orders in the buddy
    }

    // Load modules
    log(LOG_LEVEL_DEBUG, "INIT", "Enumerating %lu modules", boot_info->module_count);
    for(uint16_t i = 0; i < boot_info->module_count; i++) {
        tartarus_module_t *module = &boot_info->modules[i];
        log(LOG_LEVEL_DEBUG, "INIT", "| found `%s`", module->name);

        if(!string_eq("kernel.ksym", module->name)) continue;
        kernel_symbols_load((void *) HHDM(module->paddr));
        log(LOG_LEVEL_DEBUG, "INIT", "| » Kernel symbols loaded");
    }

    // Run early init stage
    init_run_stage(INIT_STAGE_EARLY, false);

    // Set RSDP
    g_acpi_rsdp = boot_info->acpi_rsdp_address;
    log(LOG_LEVEL_DEBUG, "INIT", "RSDP at %#lx", g_acpi_rsdp);

    // Initialize framebuffer
    if(boot_info->framebuffer_count == 0) panic("INIT", "no framebuffer provided");
    // TODO: handle pixel format... and the entire way we deal with framebuffers in general
    tartarus_framebuffer_t *framebuffer = &boot_info->framebuffers[0];
    g_framebuffer.address = framebuffer->vaddr;
    g_framebuffer.size = framebuffer->size;
    g_framebuffer.width = framebuffer->width;
    g_framebuffer.height = framebuffer->height;
    g_framebuffer.pitch = framebuffer->pitch;

    // Map HHDM
    log(LOG_LEVEL_DEBUG, "INIT", "Mapping HHDM (base: %#lx, size: %#lx)", g_hhdm_offset, g_hhdm_size);
    for(size_t i = 0; i < boot_info->mm_entry_count; i++) {
        tartarus_mm_entry_t *entry = &boot_info->mm_entries[i];
        switch(entry->type) {
            case TARTARUS_MM_TYPE_USABLE:
            case TARTARUS_MM_TYPE_BOOTLOADER_RECLAIMABLE:
            case TARTARUS_MM_TYPE_EFI_RECLAIMABLE:
            case TARTARUS_MM_TYPE_ACPI_RECLAIMABLE:       break;
            default:                                      continue;
        }

        ASSERT(entry->base + entry->length <= g_hhdm_size);
        arch_ptm_map(g_vm_global_address_space, g_hhdm_offset + entry->base, entry->base, entry->length, VM_PROT_RW, VM_CACHE_STANDARD, VM_PRIVILEGE_KERNEL, true);
    }

    g_hhdm_region.address_space = g_vm_global_address_space;
    g_hhdm_region.base = g_hhdm_offset;
    g_hhdm_region.length = g_hhdm_size;
    g_hhdm_region.protection = VM_PROT_RW;
    g_hhdm_region.cache_behavior = VM_CACHE_STANDARD;
    g_hhdm_region.dynamically_backed = false;
    g_hhdm_region.type = VM_REGION_TYPE_DIRECT;
    g_hhdm_region.type_data.direct.physical_address = 0;
    rb_insert(&g_vm_global_address_space->regions, &g_hhdm_region.rb_node);
    log(LOG_LEVEL_DEBUG, "INIT", "Global Region: HHDM (base: %#lx, size: %#lx)", g_hhdm_region.base, g_hhdm_region.length);

    // Setup page cache
    uintptr_t page_db_start = g_hhdm_offset + g_hhdm_size; // TODO: change this to some bump allocator we also use for hhdm?
    uintptr_t page_db_end = page_db_start;
    for(size_t i = 0; i < boot_info->mm_entry_count; i++) {
        tartarus_mm_entry_t *entry = &boot_info->mm_entries[i];
        switch(entry->type) {
            case TARTARUS_MM_TYPE_USABLE:
            case TARTARUS_MM_TYPE_BOOTLOADER_RECLAIMABLE:
            case TARTARUS_MM_TYPE_EFI_RECLAIMABLE:
            case TARTARUS_MM_TYPE_ACPI_RECLAIMABLE:       break;
            default:                                      continue;
        }

        uintptr_t start = page_db_start + MATH_FLOOR((entry->base / ARCH_PAGE_GRANULARITY) * sizeof(page_t), ARCH_PAGE_GRANULARITY);
        uintptr_t end = page_db_start + MATH_CEIL(((entry->base + entry->length) / ARCH_PAGE_GRANULARITY) * sizeof(page_t), ARCH_PAGE_GRANULARITY);
        log(LOG_LEVEL_DEBUG, "INIT", "Mapping page cache segment %#lx -> %#lx [%#lx]", start, end, end - start);

        for(page_db_end = start; page_db_end < end; page_db_end += ARCH_PAGE_GRANULARITY) {
            arch_ptm_map(g_vm_global_address_space, page_db_end, earlymem_alloc_page(), ARCH_PAGE_GRANULARITY, VM_PROT_RW, VM_CACHE_STANDARD, VM_PRIVILEGE_KERNEL, true);
        }
    }
    size_t page_db_size = page_db_end - page_db_start;

    // TODO: collect the physical pages in the page cache and put them into the region :)
    //  once anon regions have the rbtree to thread them on obviously

    g_page_db_region.address_space = g_vm_global_address_space;
    g_page_db_region.base = page_db_start;
    g_page_db_region.length = page_db_size;
    g_page_db_region.protection = VM_PROT_RW;
    g_page_db_region.cache_behavior = VM_CACHE_STANDARD;
    g_page_db_region.type = VM_REGION_TYPE_ANON;
    g_page_db_region.dynamically_backed = false;
    g_page_db_region.type_data.anon.back_zeroed = false;
    rb_insert(&g_vm_global_address_space->regions, &g_page_db_region.rb_node);
    log(LOG_LEVEL_DEBUG, "INIT", "Global Region: Page Cache (base: %#lx, size: %#lx)", g_page_db_region.base, g_page_db_region.length);

    g_page_db = (page_t *) page_db_start;
    g_page_db_size = page_db_size;

    // Map the kernel
    if(boot_info->kernel_segment_count == 0) panic("INIT", "Kernel has zero segments");
    for(uint64_t i = 0; i < boot_info->kernel_segment_count; i++) {
        tartarus_kernel_segment_t *segment = &boot_info->kernel_segments[i];
        log(LOG_LEVEL_DEBUG,
            "INIT",
            "| Mapping Kernel Segment { %#lx -> %#lx [%c%c%c] }",
            segment->vaddr,
            segment->vaddr + segment->size,
            (segment->flags & TARTARUS_KERNEL_SEGMENT_FLAG_READ) != 0 ? 'R' : ' ',
            (segment->flags & TARTARUS_KERNEL_SEGMENT_FLAG_WRITE) != 0 ? 'W' : ' ',
            (segment->flags & TARTARUS_KERNEL_SEGMENT_FLAG_EXECUTE) != 0 ? 'X' : ' ');

        ASSERT(segment->vaddr % ARCH_PAGE_GRANULARITY == 0 && segment->size % ARCH_PAGE_GRANULARITY == 0);

        arch_ptm_map(
            g_vm_global_address_space,
            segment->vaddr,
            segment->paddr,
            segment->size,
            (vm_protection_t) {
                .read = (segment->flags & TARTARUS_KERNEL_SEGMENT_FLAG_READ) != 0,
                .write = (segment->flags & TARTARUS_KERNEL_SEGMENT_FLAG_WRITE) != 0,
                .exec = (segment->flags & TARTARUS_KERNEL_SEGMENT_FLAG_EXECUTE) != 0,
            },
            VM_CACHE_STANDARD,
            VM_PRIVILEGE_KERNEL,
            true
        );

        vm_region_t *region = (void *) HHDM(earlymem_alloc(sizeof(vm_region_t)));
        region->address_space = g_vm_global_address_space;
        region->base = segment->vaddr;
        region->length = segment->size;
        region->type = VM_REGION_TYPE_DIRECT;
        region->type_data.direct.physical_address = segment->paddr;
        region->cache_behavior = VM_CACHE_STANDARD;
        region->dynamically_backed = false;
        region->protection = VM_PROT_RW;
        rb_insert(&g_vm_global_address_space->regions, &region->rb_node);
    }

    // Map the framebuffer
    arch_ptm_map(g_vm_global_address_space, (uintptr_t) g_framebuffer.address, framebuffer->paddr, MATH_CEIL(g_framebuffer.size, ARCH_PAGE_GRANULARITY), VM_PROT_RW, VM_CACHE_NONE, VM_PRIVILEGE_KERNEL, true);

    // Load the new address space
    log(LOG_LEVEL_DEBUG, "INIT", "Loading global address space...");
    arch_ptm_load_address_space(g_vm_global_address_space);

    // Initialize physical memory
    log(LOG_LEVEL_DEBUG, "INIT", "Initializing physical memory proper");
    for(size_t i = 0; i < boot_info->mm_entry_count; i++) {
        tartarus_mm_entry_t *entry = &boot_info->mm_entries[i];

        switch(entry->type) {
            case TARTARUS_MM_TYPE_USABLE:
            case TARTARUS_MM_TYPE_BOOTLOADER_RECLAIMABLE:
            case TARTARUS_MM_TYPE_EFI_RECLAIMABLE:
            case TARTARUS_MM_TYPE_ACPI_RECLAIMABLE:       break;
            default:                                      continue;
        }

        log(LOG_LEVEL_DEBUG, "INIT", "| %#lx -> %#lx", entry->base, entry->base + entry->length);

        pmm_region_add(entry->base, entry->length);
    }

    // TODO: release reclaimable regions into pmm as well as
    //       merging with free ones for max order to settle properly.
    LIST_ITERATE(&g_earlymem_regions, node) {
        earlymem_region_t *region = CONTAINER_OF(node, earlymem_region_t, list_node);
        for(size_t offset = 0; offset < region->length; offset += ARCH_PAGE_GRANULARITY) {
            // TODO: we can hand out the metadata pages of the earlymem bitmap as free
            if(!earlymem_region_isfree(region, offset)) continue;
            pmm_free(&PAGE(region->base + offset)->block);
        }
    }

    log(LOG_LEVEL_DEBUG, "INIT", "Physical Memory Map");
    pmm_zone_t *zones[] = { &g_pmm_zone_low, &g_pmm_zone_normal };
    for(size_t i = 0; i < sizeof(zones) / sizeof(pmm_zone_t *); i++) {
        pmm_zone_t *zone = zones[i];
        log(LOG_LEVEL_DEBUG, "INIT", "» %-6s %#-18lx -> %#-18lx %lu/%lu pages", zone->name, zone->start, zone->end, zone->free_page_count, zone->total_page_count);
    }

    // Main init
    init_run_stage(INIT_STAGE_BEFORE_MAIN, false);
    init_run_stage(INIT_STAGE_MAIN, false);

    // Dev init
    init_run_stage(INIT_STAGE_BEFORE_DEV, false);
    init_run_stage(INIT_STAGE_DEV, false);

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

    // Run module tests
#ifdef __ENV_DEVELOPMENT
    vfs_node_t *modules_dir = nullptr;
    res = vfs_lookup(&VFS_ABSOLUTE_PATH("/sys/modules"), &modules_dir);
    if(res != VFS_RESULT_OK) panic("INIT", "failed to lookup modules directory (%i)", res);
    if(modules_dir == nullptr) panic("INIT", "no modules directory found");

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

    // Late init
    init_run_stage(INIT_STAGE_LATE, false);

    // Schedule init threads
    sched_thread_schedule(reaper_create());

    // CPU local proper
    log(LOG_LEVEL_DEBUG, "INIT", "Setting up proper CPU locals (%lu locals)", g_cpu_count);
    g_cpu_list = heap_alloc(sizeof(cpu_t) * g_cpu_count);

    for(size_t i = 0; i < boot_info->cpu_count; i++) {
        if(boot_info->cpus[i].init_state == TARTARUS_CPU_STATE_FAIL) continue;

        cpu_t *cpu = &g_cpu_list[i];
        if(i == boot_info->bsp_index) {
            mem_copy(cpu, &g_early_bsp, sizeof(cpu_t));
            arch_cpu_local_load(cpu);

            ASSERT(cpu->sequential_id == i);
        }
        cpu->sequential_id = i;
        cpu->self = cpu;
    }

    // SMP init
    log(LOG_LEVEL_DEBUG, "INIT", "Starting APs...");
    for(size_t i = 0; i < boot_info->cpu_count; i++) {
        if(i == boot_info->bsp_index) continue;
        if(boot_info->cpus[i].init_state == TARTARUS_CPU_STATE_FAIL) continue;

        init_reset_ap();

        ATOMIC_STORE(&g_init_ap_cpu_id, boot_info->cpus[i].sequential_id, ATOMIC_SEQ_CST);
        ATOMIC_STORE(&g_init_ap_finished, false, ATOMIC_SEQ_CST);

        ATOMIC_STORE(boot_info->cpus[i].wake_on_write, (uintptr_t) init_ap, ATOMIC_SEQ_CST);
        while(!ATOMIC_LOAD(&g_init_ap_finished, ATOMIC_SEQ_CST)) arch_cpu_relax();
    }

    log(LOG_LEVEL_DEBUG, "INIT", "SMP init done (%lu/%lu cpus initialized)", g_cpu_count, g_cpu_count);
    log(LOG_LEVEL_DEBUG, "INIT", "BSP seqid is %lu", ARCH_CPU_CURRENT_READ(sequential_id));

    sched_thread_schedule(arch_sched_thread_create_kernel(thread_init));

    // Scheduler handoff
    log(LOG_LEVEL_INFO, "INIT", "BSP reached scheduler handoff. Bye now!");
    arch_sched_handoff_cpu();
    ASSERT_UNREACHABLE();
}
