#include "sys/init.h"

#include "arch/cpu.h"
#include "common/assert.h"
#include "common/log.h"
#include "lib/string.h"

#include <stddef.h>

#define DO_TARGET(TARGET, IS_AP) ((TARGET)->scope == INIT_SCOPE_ALL || ((IS_AP) && (TARGET)->scope == INIT_SCOPE_APS) || (!(IS_AP) && (TARGET)->scope == INIT_SCOPE_BSP))

#define TARGET_COUNT (((uintptr_t) ld_init_targets_end - (uintptr_t) ld_init_targets_start) / sizeof(init_target_t))
#define TARGETS ((init_target_t *) ld_init_targets_start)

extern nullptr_t ld_init_targets_start[];
extern nullptr_t ld_init_targets_end[];

cpu_t *g_cpu_list;

init_stage_t g_init_stage_current = INIT_STAGE_BOOT;

static const char *stage_stringify(init_stage_t stage) {
    switch(stage) {
        case INIT_STAGE_BOOT:        return "boot";
        case INIT_STAGE_EARLY:       return "early";
        case INIT_STAGE_BEFORE_MAIN: return "before_main";
        case INIT_STAGE_MAIN:        return "main";
        case INIT_STAGE_BEFORE_DEV:  return "before_dev";
        case INIT_STAGE_DEV:         return "dev";
        case INIT_STAGE_LATE:        return "late";
    }
    ASSERT_UNREACHABLE();
}

static init_target_t *find_target(init_stage_t stage, const char *name) {
    for(size_t i = 0; i < TARGET_COUNT; i++) {
        if(TARGETS[i].stage != stage || !string_eq(TARGETS[i].name, name)) continue;
        return &TARGETS[i];
    }
    return nullptr;
}

static void run_target(init_target_t *target, bool is_ap) {
    for(size_t i = 0; i < target->dependency_count; i++) {
        init_target_t *dep = find_target(target->stage, target->dependencies[i]);
        if(dep == nullptr) {
            log(LOG_LEVEL_WARN, "INIT", "Target `%s/%s` has an unknown dependency `%s`", stage_stringify(target->stage), target->name, target->dependencies[i]);
            continue;
        }
        run_target(dep, is_ap);
    }

    if(!DO_TARGET(target, is_ap) || target->completed) return;
    target->completed = true;

    log(LOG_LEVEL_DEBUG, "INIT", "Target `%s/%s` (core %lu)", stage_stringify(target->stage), target->name, ARCH_CPU_CURRENT_READ(sequential_id));
    target->fn();
}

void init_reset_ap() {
    g_init_stage_current = INIT_STAGE_BOOT;
    for(size_t i = 0; i < TARGET_COUNT; i++) {
        init_target_t *target = &TARGETS[i];
        if(!DO_TARGET(target, true)) continue;
        target->completed = false;
    }
}

void init_run_stage(init_stage_t stage, bool is_ap) {
    ASSERT(stage >= g_init_stage_current);
    g_init_stage_current = stage;
    for(size_t i = 0; i < TARGET_COUNT; i++) {
        if(TARGETS[i].stage != stage) continue;
        run_target(&TARGETS[i], is_ap);
    }
}
