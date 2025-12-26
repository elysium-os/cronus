#include "sys/init.h"

#include "arch/cpu.h"
#include "common/log.h"
#include "lib/string.h"

#include <stddef.h>

#define TARGET_COUNT (((uintptr_t) ld_init_targets_end - (uintptr_t) ld_init_targets_start) / sizeof(init_target_t))
#define TARGETS ((init_target_t *) ld_init_targets_start)

extern nullptr_t ld_init_targets_start[];
extern nullptr_t ld_init_targets_end[];

static bool target_provides(init_target_t *target, const char *name) {
    for(size_t i = 0; i < target->provides_count; i++) {
        if(!string_eq(target->provides[i], name)) continue;
        return true;
    }
    return false;
}

static void run_target(init_target_t *target, bool is_ap) {
    if(target->completed) return;

    for(size_t i = 0; i < target->dependency_count; i++) {
        size_t provided_count = 0;
        for(size_t j = 0; j < TARGET_COUNT; j++) {
            init_target_t *dep = &TARGETS[j];
            if(!target_provides(dep, target->dependencies[i])) continue;
            run_target(dep, is_ap);
            provided_count++;
        }

        if(provided_count == 0) {
            log(LOG_LEVEL_WARN, "INIT", "`%s` has no providers", target->dependencies[i]);
            continue;
        }
    }

    switch(target->type) {
        case INIT_TYPE_ALL: break;
        case INIT_TYPE_BSP_ONLY:
            if(is_ap) return;
            break;
        case INIT_TYPE_APS_ONLY:
            if(!is_ap) return;
            break;
    }

    target->completed = true;

    if(target->fn == nullptr) return;

    if(is_ap) {
        log(LOG_LEVEL_DEBUG, "INIT", "Target `%s` (AP %lu)", target->name, ARCH_CPU_CURRENT_READ(sequential_id));
    } else {
        log(LOG_LEVEL_DEBUG, "INIT", "Target `%s` (BSP)", target->name);
    }

    target->fn();
}

void init_run(bool is_ap) {
    // Reset targets
    for(size_t i = 0; i < TARGET_COUNT; i++) {
        switch(TARGETS[i].type) {
            case INIT_TYPE_ALL: TARGETS[i].completed = false; break;
            case INIT_TYPE_BSP_ONLY:
                if(!is_ap) TARGETS[i].completed = false;
                break;
            case INIT_TYPE_APS_ONLY:
                if(is_ap) TARGETS[i].completed = false;
                break;
        }
    }

    // Run all targets
    for(size_t i = 0; i < TARGET_COUNT; i++) run_target(&TARGETS[i], is_ap);
}
