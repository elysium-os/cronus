#pragma once

#include <stddef.h>

#define INIT_TARGET_FULL(NAME, STAGE, FN, PERCORE, ...)                                                        \
    static const char *g_init_target_##NAME##_deps[] = { __VA_ARGS__ };                                        \
    [[gnu::used, gnu::section(".init_targets")]] static init_target_t g_init_target_##NAME = (init_target_t) { \
        .name = #NAME,                                                                                         \
        .stage = (STAGE),                                                                                      \
        .dependencies = g_init_target_##NAME##_deps,                                                           \
        .dependency_count = sizeof(g_init_target_##NAME##_deps) / sizeof(const char *),                        \
        .fn = (FN),                                                                                            \
        .per_core = (PERCORE),                                                                                 \
        .completed = false,                                                                                    \
    };


#define INIT_TARGET(NAME, STAGE, FN, ...) INIT_TARGET_FULL(NAME, STAGE, FN, false, __VA_ARGS__)
#define INIT_TARGET_PERCORE(NAME, STAGE, FN, ...) INIT_TARGET_FULL(NAME, STAGE, FN, true, __VA_ARGS__)

typedef enum {
    /**
     * Extremely early init stage with no subsystems guaranteed.
     */
    INIT_STAGE_BOOT,

    /**
     * Early init stage.
     *
     * Guarantees:
     * - Earlymem is initialized.
     */
    INIT_STAGE_EARLY,

    INIT_STAGE_BEFORE_MAIN,
    INIT_STAGE_MAIN,

    INIT_STAGE_BEFORE_DEV,
    INIT_STAGE_DEV,

    INIT_STAGE_LATE,
} init_stage_t;

typedef struct [[gnu::packed]] {
    const char *name;
    init_stage_t stage;
    const char **dependencies;
    size_t dependency_count;
    void (*fn)();
    bool per_core;
    bool completed;
} init_target_t;

extern init_stage_t g_init_stage_current;

/// Reset per_core targets for a new ap.
void init_reset_ap();

/// Run all the init targets for a stage.
void init_stage(init_stage_t stage, bool is_ap);
