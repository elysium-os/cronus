#include "sys/init.h"

#include "arch/init.h"
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
#include "lib/math.h"
#include "lib/string.h"
#include "memory/earlymem.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/pmm.h"
#include "memory/vm.h"
#include "sched/reaper.h"
#include "sys/event.h"
#include "sys/kernel_symbol.h"
#include "sys/module.h"

#include <tartarus.h>
#include <uacpi/uacpi.h>

framebuffer_t g_framebuffer;
size_t g_cpu_count;

static vm_region_t g_hhdm_region, g_page_cache_region;

static void thread_init() {
    uacpi_status ret = uacpi_namespace_load();
    if(uacpi_unlikely_error(ret)) log(LOG_LEVEL_WARN, "UACPI", "namespace load failed (%s)", uacpi_status_to_string(ret));
    log(LOG_LEVEL_INFO, "UACPI", "Setup Done");
}

[[gnu::no_instrument_function]] [[noreturn]] void init(tartarus_boot_info_t *boot_info) {
    init_bsp_local(boot_info->cpus[boot_info->bsp_index].sequential_id);
    event_init_cpu_local();
    init_stage(INIT_STAGE_BEFORE_EARLY, false);
    init_bsp();

    // Initialize HHDM
    ASSERT(boot_info->hhdm_offset % PAGE_GRANULARITY == 0 && boot_info->hhdm_offset % PAGE_GRANULARITY == 0);
    g_hhdm_offset = boot_info->hhdm_offset;
    g_hhdm_size = boot_info->hhdm_size;

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

    log(LOG_LEVEL_INFO, "INIT", "Elysium " MACROS_STRINGIFY(__ARCH) " " MACROS_STRINGIFY(__VERSION) " (" __DATE__ " " __TIME__ ")");

    uint32_t highest_seqid = 0;
    g_cpu_count = 0;
    for(tartarus_size_t i = 0; i < boot_info->cpu_count; i++) {
        if(boot_info->cpus[i].init_state == TARTARUS_CPU_STATE_FAIL) continue;
        if(boot_info->cpus[i].sequential_id > highest_seqid) highest_seqid = boot_info->cpus[i].sequential_id;
        g_cpu_count++;
    }
    if(g_cpu_count - 1 != highest_seqid) panic("INIT", "Highest sequential id to cpu count mismatch");

    log(LOG_LEVEL_DEBUG, "INIT", "Counted %lu working CPUs", g_cpu_count);
    ASSERT(g_cpu_count > 0);

    // Load modules
    log(LOG_LEVEL_DEBUG, "INIT", "Enumerating %lu modules", boot_info->module_count);
    for(uint16_t i = 0; i < boot_info->module_count; i++) {
        tartarus_module_t *module = &boot_info->modules[i];
        log(LOG_LEVEL_DEBUG, "INIT", "| found `%s`", module->name);

        if(!string_eq("kernel.ksym", module->name)) continue;
        kernel_symbols_load((void *) HHDM(module->paddr));
        log(LOG_LEVEL_DEBUG, "INIT", "| » Kernel symbols loaded");
    }

    // Initialize Early Memory
    for(size_t i = 0; i < boot_info->mm_entry_count; i++) {
        tartarus_mm_entry_t *entry = &boot_info->mm_entries[i];

        for(size_t j = i + 1; j < boot_info->mm_entry_count; j++) {
            ASSERT((entry->base >= (boot_info->mm_entries[j].base + boot_info->mm_entries[j].length)) || (boot_info->mm_entries[j].base >= (entry->base + entry->length)));
        }

        switch(entry->type) {
            case TARTARUS_MM_TYPE_USABLE: break;

            case TARTARUS_MM_TYPE_BOOTLOADER_RECLAIMABLE:
            case TARTARUS_MM_TYPE_EFI_RECLAIMABLE:
            case TARTARUS_MM_TYPE_ACPI_RECLAIMABLE:
            case TARTARUS_MM_TYPE_ACPI_NVS:
            case TARTARUS_MM_TYPE_RESERVED:
            case TARTARUS_MM_TYPE_BAD:                    continue;
        }

        ASSERT(entry->base % PAGE_GRANULARITY == 0 && entry->length % PAGE_GRANULARITY == 0);

        earlymem_region_add(entry->base, entry->length); // TODO: coalesce these regions so that we dont end up with weird max_orders in the buddy
    }

    init_stage(INIT_STAGE_EARLY, false);

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
        ptm_map(g_vm_global_address_space, g_hhdm_offset + entry->base, entry->base, entry->length, VM_PROT_RW, VM_CACHE_STANDARD, VM_PRIVILEGE_KERNEL, true);
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
    uintptr_t page_cache_start = g_hhdm_offset + g_hhdm_size; // TODO: change this to some bump allocator we also use for hhdm?
    uintptr_t page_cache_end = page_cache_start;
    for(size_t i = 0; i < boot_info->mm_entry_count; i++) {
        tartarus_mm_entry_t *entry = &boot_info->mm_entries[i];
        switch(entry->type) {
            case TARTARUS_MM_TYPE_USABLE:
            case TARTARUS_MM_TYPE_BOOTLOADER_RECLAIMABLE:
            case TARTARUS_MM_TYPE_EFI_RECLAIMABLE:
            case TARTARUS_MM_TYPE_ACPI_RECLAIMABLE:       break;
            default:                                      continue;
        }

        uintptr_t start = page_cache_start + MATH_FLOOR((entry->base / PAGE_GRANULARITY) * sizeof(page_t), PAGE_GRANULARITY);
        uintptr_t end = page_cache_start + MATH_CEIL(((entry->base + entry->length) / PAGE_GRANULARITY) * sizeof(page_t), PAGE_GRANULARITY);
        log(LOG_LEVEL_DEBUG, "INIT", "Mapping page cache segment %#lx -> %#lx [%#lx]", start, end, end - start);

        for(page_cache_end = start; page_cache_end < end; page_cache_end += PAGE_GRANULARITY) {
            ptm_map(g_vm_global_address_space, page_cache_end, earlymem_alloc_page(), PAGE_GRANULARITY, VM_PROT_RW, VM_CACHE_STANDARD, VM_PRIVILEGE_KERNEL, true);
        }
    }
    size_t page_cache_size = page_cache_end - page_cache_start;

    // TODO: collect the physical pages in the page cache and put them into the region :)
    //  once anon regions have the rbtree to thread them on obviously

    g_page_cache_region.address_space = g_vm_global_address_space;
    g_page_cache_region.base = page_cache_start;
    g_page_cache_region.length = page_cache_size;
    g_page_cache_region.protection = VM_PROT_RW;
    g_page_cache_region.cache_behavior = VM_CACHE_STANDARD;
    g_page_cache_region.type = VM_REGION_TYPE_ANON;
    g_page_cache_region.dynamically_backed = false;
    g_page_cache_region.type_data.anon.back_zeroed = false;
    rb_insert(&g_vm_global_address_space->regions, &g_page_cache_region.rb_node);
    log(LOG_LEVEL_DEBUG, "INIT", "Global Region: Page Cache (base: %#lx, size: %#lx)", g_page_cache_region.base, g_page_cache_region.length);

    g_page_cache = (page_t *) page_cache_start;
    g_page_cache_size = page_cache_size;

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

        ASSERT(segment->vaddr % PAGE_GRANULARITY == 0 && segment->size % PAGE_GRANULARITY == 0);

        ptm_map(
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
    ptm_map(g_vm_global_address_space, (uintptr_t) g_framebuffer.address, framebuffer->paddr, MATH_CEIL(g_framebuffer.size, PAGE_GRANULARITY), VM_PROT_RW, VM_CACHE_NONE, VM_PRIVILEGE_KERNEL, true);

    // Load the new address space
    log(LOG_LEVEL_DEBUG, "INIT", "Loading global address space...");
    ptm_load_address_space(g_vm_global_address_space);

    // Initialize physical memory
    log(LOG_LEVEL_DEBUG, "INIT", "Initializing physical memory proper");
    for(size_t i = 0; i < boot_info->mm_entry_count; i++) {
        tartarus_mm_entry_t *entry = &boot_info->mm_entries[i];

        bool is_free = false;
        switch(entry->type) {
            case TARTARUS_MM_TYPE_USABLE:                 break;
            case TARTARUS_MM_TYPE_BOOTLOADER_RECLAIMABLE: break;
            case TARTARUS_MM_TYPE_EFI_RECLAIMABLE:        break;
            case TARTARUS_MM_TYPE_ACPI_RECLAIMABLE:       break;
            default:                                      continue;
        }

        log(LOG_LEVEL_DEBUG, "INIT", "| %#lx -> %#lx (free: %u)", entry->base, entry->base + entry->length, is_free);

        pmm_region_add(entry->base, entry->length, is_free);
    }

    // TODO: release reclaimable regions into pmm as well as
    //       merging with free ones for max order to settle properly.
    LIST_ITERATE(&g_earlymem_regions, node) {
        earlymem_region_t *region = CONTAINER_OF(node, earlymem_region_t, list_node);
        for(size_t offset = 0; offset < region->length; offset += PAGE_GRANULARITY) {
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
    init_stage(INIT_STAGE_BEFORE_MAIN, false);
    init_cpu_locals(boot_info);
    init_stage(INIT_STAGE_MAIN, false);

    // Dev init
    init_stage(INIT_STAGE_BEFORE_DEV, false);
    init_stage(INIT_STAGE_DEV, false);

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
    init_stage(INIT_STAGE_LATE, false);

    // SMP init
    log(LOG_LEVEL_DEBUG, "INIT", "Starting APs...");
    init_smp(boot_info);

    // Schedule init threads
    sched_thread_schedule(reaper_create());
    sched_thread_schedule(sched_thread_create_kernel(thread_init));

    // Scheduler handoff
    log(LOG_LEVEL_INFO, "INIT", "Reached scheduler handoff. Bye now!");
    sched_handoff_cpu();
    ASSERT_UNREACHABLE();
}
