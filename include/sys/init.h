#pragma once

#include "lib/macros.h"

#include <stddef.h>

#define INIT_TARGET_UTIL_NAME(NAME) MACROS_CONCAT(MACROS_CONCAT(g_init_target_, __LINE__), _##NAME)
#define INIT_TARGET_UTIL_FN(NAME) MACROS_CONCAT(INIT_TARGET_UTIL_NAME(NAME), _fn)
#define INIT_TARGET_UTIL_DEPS(NAME) MACROS_CONCAT(INIT_TARGET_UTIL_NAME(NAME), _deps)
#define INIT_TARGET_UTIL_PROVIDES(NAME) MACROS_CONCAT(INIT_TARGET_UTIL_NAME(NAME), _provides)

#define INIT_TARGET_UTIL_FULL_WITH_FN(NAME, PROVIDES, DEPS, TYPE)                \
    static void INIT_TARGET_UTIL_FN(NAME)();                                     \
    INIT_TARGET_UTIL_FULL(NAME, PROVIDES, DEPS, TYPE, INIT_TARGET_UTIL_FN(NAME)) \
    static void INIT_TARGET_UTIL_FN(NAME)()

#define INIT_TARGET_UTIL_FULL(NAME, PROVIDES, DEPS, TYPE, FN)                                                         \
    static const char *INIT_TARGET_UTIL_DEPS(NAME)[] = DEPS;                                                          \
    static const char *INIT_TARGET_UTIL_PROVIDES(NAME)[] = PROVIDES;                                                  \
    [[gnu::used, gnu::section(".init_targets")]] static init_target_t INIT_TARGET_UTIL_NAME(NAME) = (init_target_t) { \
        .name = #NAME,                                                                                                \
        .fn = (FN),                                                                                                   \
        .type = (TYPE),                                                                                               \
        .provides = INIT_TARGET_UTIL_PROVIDES(NAME),                                                                  \
        .provides_count = sizeof(INIT_TARGET_UTIL_PROVIDES(NAME)) / sizeof(const char *),                             \
        .dependencies = INIT_TARGET_UTIL_DEPS(NAME),                                                                  \
        .dependency_count = sizeof(INIT_TARGET_UTIL_DEPS(NAME)) / sizeof(const char *),                               \
    };

#define INIT_PROVIDES(...) ((const char *[]) { __VA_ARGS__ })
#define INIT_DEPS(...) ((const char *[]) { __VA_ARGS__ })
#define INIT_TARGET(NAME, PROVIDES, DEPS) INIT_TARGET_UTIL_FULL_WITH_FN(NAME, PROVIDES, DEPS, INIT_TYPE_BSP_ONLY)
#define INIT_TARGET_PERCORE(NAME, PROVIDES, DEPS) INIT_TARGET_UTIL_FULL_WITH_FN(NAME, PROVIDES, DEPS, INIT_TYPE_ALL)
#define INIT_TARGET_BIND(NAME, PROVIDES, DEPS) INIT_TARGET_UTIL_FULL(NAME, PROVIDES, DEPS, INIT_TYPE_BSP_ONLY, nullptr)
#define INIT_TARGET_BIND_PERCORE(NAME, PROVIDES, DEPS) INIT_TARGET_UTIL_FULL(NAME, PROVIDES, DEPS, INIT_TYPE_ALL, nullptr)

typedef enum {
    INIT_TYPE_ALL,
    INIT_TYPE_BSP_ONLY,
    INIT_TYPE_APS_ONLY,
} init_type_t;

typedef struct [[gnu::packed]] init_target {
    const char *name;
    void (*fn)();
    init_type_t type;

    const char **provides;
    size_t provides_count;

    const char **dependencies;
    size_t dependency_count;

    bool completed;
} init_target_t;

/// Run the init system.
void init_run(bool is_ap);
