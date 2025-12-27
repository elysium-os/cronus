#include "sys/init.h"

#include "arch/cpu.h"
#include "arch/init.h"
#include "arch/page.h"
#include "arch/ptm.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/log.h"
#include "fs/rdsk.h"
#include "fs/tmpfs.h"
#include "fs/vfs.h"
#include "graphics/framebuffer.h"
#include "lib/math.h"
#include "lib/string.h"
#include "memory/earlymem.h"
#include "memory/hhdm.h"
#include "memory/page.h"
#include "memory/vm.h"
#include "sched/reaper.h"
#include "sys/cpu.h"
#include "sys/init.h"
#include "sys/kernel_symbol.h"
#include "sys/module.h"

#include <tartarus.h>
#include <uacpi/uacpi.h>

tartarus_boot_info_t *g_init_boot_info = nullptr;

cpu_t *g_cpu_list = nullptr;
size_t g_cpu_count = 0;

framebuffer_t g_framebuffer;

static uintptr_t g_framebuffer_paddr;

static vm_region_t g_hhdm_region, g_page_db_region;

static void thread_init() {
    uacpi_status ret = uacpi_namespace_load();
    if(uacpi_unlikely_error(ret)) log(LOG_LEVEL_WARN, "UACPI", "namespace load failed (%s)", uacpi_status_to_string(ret));
    log(LOG_LEVEL_INFO, "UACPI", "Setup Done");
}

[[gnu::no_instrument_function]] [[noreturn]] void init(tartarus_boot_info_t *boot_info) {
    g_init_boot_info = boot_info;

    arch_init_bsp();
    ARCH_CPU_CURRENT_WRITE(sequential_id, boot_info->cpus[boot_info->bsp_index].sequential_id);

    init_run(false);

    vfs_node_t *root_node;
    vfs_result_t res = vfs_root(&root_node);
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

    // SMP init
    log(LOG_LEVEL_DEBUG, "INIT", "Starting APs...");
    arch_init_smp(boot_info);

    // Schedule init threads
    sched_thread_schedule(reaper_create());
    sched_thread_schedule(arch_sched_thread_create_kernel(thread_init));

    // Scheduler handoff
    log(LOG_LEVEL_INFO, "INIT", "Reached scheduler handoff. Bye now!");
    arch_sched_handoff_cpu();
    ASSERT_UNREACHABLE();
}

INIT_TARGET(welcome, INIT_PROVIDES(), INIT_DEPS("log")) {
    log(LOG_LEVEL_INFO, "INIT", "Elysium " MACROS_STRINGIFY(__ARCH) " " MACROS_STRINGIFY(__VERSION) " (" __DATE__ " " __TIME__ ")");
}

INIT_TARGET_PERCORE(cpu_flags, INIT_PROVIDES("cpu_local"), INIT_DEPS()) {
    // TODO: possibly move this to more "proper" targets
    ARCH_CPU_CURRENT_WRITE(flags.threaded, false);
    ARCH_CPU_CURRENT_WRITE(flags.in_interrupt_soft, false);
    ARCH_CPU_CURRENT_WRITE(flags.in_interrupt_hard, false);
    ARCH_CPU_CURRENT_WRITE(flags.deferred_work_status, (unsigned int) 0);
}

INIT_TARGET(hhdm, INIT_PROVIDES("hhdm", "memory"), INIT_DEPS("assert")) {
    ASSERT(g_init_boot_info->hhdm_offset % ARCH_PAGE_GRANULARITY == 0 && g_init_boot_info->hhdm_offset % ARCH_PAGE_GRANULARITY == 0);
    g_hhdm_offset = g_init_boot_info->hhdm_offset;
    g_hhdm_size = g_init_boot_info->hhdm_size;
}

INIT_TARGET(cpu_count, INIT_PROVIDES("cpu_count"), INIT_DEPS("assert", "panic", "log")) {
    uint32_t highest_seqid = 0;
    for(tartarus_size_t i = 0; i < g_init_boot_info->cpu_count; i++) {
        if(g_init_boot_info->cpus[i].init_state == TARTARUS_CPU_STATE_FAIL) continue;
        if(g_init_boot_info->cpus[i].sequential_id > highest_seqid) highest_seqid = g_init_boot_info->cpus[i].sequential_id;
        g_cpu_count++;
    }
    if(g_cpu_count - 1 != highest_seqid) panic("INIT", "Highest sequential id to cpu count mismatch");

    log(LOG_LEVEL_DEBUG, "INIT", "Counted %lu working CPUs", g_cpu_count);
    ASSERT(g_cpu_count > 0);
}

INIT_TARGET(modules, INIT_PROVIDES("boot_module"), INIT_DEPS("hhdm", "panic", "log")) {
    log(LOG_LEVEL_DEBUG, "INIT", "Enumerating %lu modules", g_init_boot_info->module_count);
    for(uint16_t i = 0; i < g_init_boot_info->module_count; i++) {
        tartarus_module_t *module = &g_init_boot_info->modules[i];
        log(LOG_LEVEL_DEBUG, "INIT", "| found `%s`", module->name);

        if(!string_eq("kernel.ksym", module->name)) continue;
        kernel_symbols_load((void *) HHDM(module->paddr));
        log(LOG_LEVEL_DEBUG, "INIT", "| » Kernel symbols loaded");
    }
}

INIT_TARGET(framebuffer, INIT_PROVIDES("framebuffer"), INIT_DEPS()) {
    // TODO: handle having no framebuffers

    // TODO: handle pixel format... and the entire way we deal with framebuffers in general
    tartarus_framebuffer_t *framebuffer = &g_init_boot_info->framebuffers[0];
    g_framebuffer.address = framebuffer->vaddr;
    g_framebuffer.size = framebuffer->size;
    g_framebuffer.width = framebuffer->width;
    g_framebuffer.height = framebuffer->height;
    g_framebuffer.pitch = framebuffer->pitch;

    g_framebuffer_paddr = framebuffer->paddr;
}

INIT_TARGET(map_hhdm, INIT_PROVIDES("map_global_as"), INIT_DEPS("hhdm", "ptm", "log")) {
    log(LOG_LEVEL_DEBUG, "INIT", "Mapping HHDM (base: %#lx, size: %#lx)", g_hhdm_offset, g_hhdm_size);
    for(size_t i = 0; i < g_init_boot_info->mm_entry_count; i++) {
        tartarus_mm_entry_t *entry = &g_init_boot_info->mm_entries[i];
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
}

INIT_TARGET(map_page_db, INIT_PROVIDES("map_global_as"), INIT_DEPS("ptm", "earlymem", "log")) {
    uintptr_t page_db_start = g_hhdm_offset + g_hhdm_size; // TODO: change this to some bump allocator we also use for hhdm?
    uintptr_t page_db_end = page_db_start;
    for(size_t i = 0; i < g_init_boot_info->mm_entry_count; i++) {
        tartarus_mm_entry_t *entry = &g_init_boot_info->mm_entries[i];
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
}

INIT_TARGET(map_kernel, INIT_PROVIDES("map_global_as"), INIT_DEPS("ptm", "earlymem", "hhdm", "log")) {
    if(g_init_boot_info->kernel_segment_count == 0) panic("INIT", "Kernel has zero segments");
    for(uint64_t i = 0; i < g_init_boot_info->kernel_segment_count; i++) {
        tartarus_kernel_segment_t *segment = &g_init_boot_info->kernel_segments[i];
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
}

INIT_TARGET(map_fb, INIT_PROVIDES("map_global_as"), INIT_DEPS("ptm", "framebuffer")) {
    arch_ptm_map(g_vm_global_address_space, (uintptr_t) g_framebuffer.address, g_framebuffer_paddr, MATH_CEIL(g_framebuffer.size, ARCH_PAGE_GRANULARITY), VM_PROT_RW, VM_CACHE_NONE, VM_PRIVILEGE_KERNEL, true);
}

INIT_TARGET_PERCORE(load_global_map, INIT_PROVIDES("global_as", "memory"), INIT_DEPS("map_global_as")) {
    log(LOG_LEVEL_DEBUG, "INIT", "Loading global address space...");
    arch_ptm_load_address_space(g_vm_global_address_space);
}

INIT_TARGET(sysroot, INIT_PROVIDES("fs"), INIT_DEPS("memory")) {
    tartarus_module_t *sysroot_module = nullptr;
    for(uint16_t i = 0; i < g_init_boot_info->module_count; i++) {
        tartarus_module_t *module = &g_init_boot_info->modules[i];
        if(!string_eq(module->name, "root.rdk")) continue;
        sysroot_module = module;
        break;
    }
    if(sysroot_module == nullptr) panic("INIT", "could not locate root.rdk");

    vfs_result_t res = vfs_mount(&g_rdsk_ops, nullptr, (void *) HHDM(sysroot_module->paddr));
    if(res != VFS_RESULT_OK) panic("INIT", "failed to mount rdsk (%i)", res);
}

INIT_TARGET_BIND(assert, INIT_PROVIDES("assert"), INIT_DEPS("panic"));
