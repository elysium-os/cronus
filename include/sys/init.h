#pragma once

#include "lib/macros.h"

#include <stddef.h>

#define INIT_UTIL_NAME MACROS_CONCAT(init_target_ln, __LINE__)
#define INIT_UTIL_VAR MACROS_CONCAT(g_, INIT_UTIL_NAME)
#define INIT_UTIL_VAR_DEPS MACROS_CONCAT(INIT_UTIL_VAR, _deps)

#define INIT_DEPS(...) ((const char *[]) { __VA_ARGS__ })

#define INIT_TARGET(NAME, STAGE, SCOPE, DEPS)                                                          \
    static void INIT_UTIL_NAME();                                                                      \
    static const char *INIT_UTIL_VAR_DEPS[] = DEPS;                                                    \
    [[gnu::used, gnu::section("init_targets")]] static init_target_t INIT_UTIL_VAR = (init_target_t) { \
        .name = #NAME,                                                                                 \
        .stage = (STAGE),                                                                              \
        .scope = (SCOPE),                                                                              \
        .fn = (INIT_UTIL_NAME),                                                                        \
        .dependencies = INIT_UTIL_VAR_DEPS,                                                            \
        .dependency_count = sizeof(INIT_UTIL_VAR_DEPS) / sizeof(const char *),                         \
        .completed = false,                                                                            \
    };                                                                                                 \
    static void INIT_UTIL_NAME()

typedef enum {
    /**
     * Extremely early init stage with no subsystems guaranteed.
     */
    INIT_STAGE_BOOT,

    /**
     * Early init stage.
     *
     * Guarantees:
     * - Earlymem is initialized
     */
    INIT_STAGE_EARLY,

    INIT_STAGE_BEFORE_MAIN,
    INIT_STAGE_MAIN,

    INIT_STAGE_BEFORE_DEV,
    INIT_STAGE_DEV,

    INIT_STAGE_LATE,
} init_stage_t;

typedef enum {
    INIT_SCOPE_BSP,
    INIT_SCOPE_APS,
    INIT_SCOPE_ALL
} init_scope_t;

typedef struct [[gnu::packed]] {
    const char *name;
    init_stage_t stage;
    init_scope_t scope;
    void (*fn)();
    const char **dependencies;
    size_t dependency_count;

    bool completed;
} init_target_t;

extern init_stage_t g_init_stage_current;

/// Reset per_core targets for a new ap.
void init_reset_ap();

/// Run all the init targets for a stage.
void init_run_stage(init_stage_t stage, bool is_ap);
