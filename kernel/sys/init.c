#include "sys/init.h"

#include "arch/cpu.h"
#include "common/assert.h"
#include "common/log.h"
#include "lib/string.h"

#include <stddef.h>

#define TARGET_COUNT (((uintptr_t) ld_init_targets_end - (uintptr_t) ld_init_targets_start) / sizeof(init_target_t))
#define TARGETS ((init_target_t *) ld_init_targets_start)

extern nullptr_t ld_init_targets_start[];
extern nullptr_t ld_init_targets_end[];

static const char *stage_stringify(init_stage_t stage) {
    switch(stage) {
        case INIT_STAGE_BEFORE_EARLY: return "before_early";
        case INIT_STAGE_EARLY:        return "early";
        case INIT_STAGE_BEFORE_MAIN:  return "before_main";
        case INIT_STAGE_MAIN:         return "main";
        case INIT_STAGE_BEFORE_DEV:   return "before_dev";
        case INIT_STAGE_DEV:          return "dev";
        case INIT_STAGE_LATE:         return "late";
    }
    ASSERT_UNREACHABLE();
}

static init_target_t *find_init_target(init_stage_t stage, const char *name) {
    for(size_t i = 0; i < TARGET_COUNT; i++) {
        if(TARGETS[i].stage != stage || !string_eq(TARGETS[i].name, name)) continue;
        return &TARGETS[i];
    }
    return nullptr;
}

static void run_init_target(init_target_t *target, bool is_ap) {
    for(size_t i = 0; i < target->dependency_count; i++) {
        init_target_t *dep = find_init_target(target->stage, target->dependencies[i]);
        if(dep == nullptr) {
            log(LOG_LEVEL_WARN, "INIT", "Init target `%s/%s` has an unknown dependency `%s`", stage_stringify(target->stage), target->name, target->dependencies[i]);
            continue;
        }
        run_init_target(dep, is_ap);
    }

    if((is_ap && !target->per_core) || target->completed) return;

    if(target->per_core) {
        log(LOG_LEVEL_DEBUG, "INIT", "Running per-core init target `%s/%s` for core %lu", stage_stringify(target->stage), target->name, cpu_id());
    } else {
        log(LOG_LEVEL_DEBUG, "INIT", "Running init target `%s/%s`", stage_stringify(target->stage), target->name);
    }

    target->completed = true;
    target->fn();
}

void init_reset_ap() {
    for(size_t i = 0; i < TARGET_COUNT; i++) {
        if(!TARGETS[i].per_core) continue;
        TARGETS[i].completed = false;
    }
}

void init_stage(init_stage_t stage, bool is_ap) {
    for(size_t i = 0; i < TARGET_COUNT; i++) {
        if(TARGETS[i].stage != stage) continue;
        run_init_target(&TARGETS[i], is_ap);
    }
}
