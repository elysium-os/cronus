#pragma once

#include "lib/macros.h"

typedef void (*hook_fn_t)();

#define HOOK_UTIL_NAME MACROS_CONCAT(hook_ln, __LINE__)

#define HOOK_RUN(NAME)                                                                                                                          \
    extern const nullptr_t __start_hook_##NAME[];                                                                                               \
    extern const nullptr_t __stop_hook_##NAME[];                                                                                                \
    for(const hook_fn_t *it = (hook_fn_t *) (uintptr_t) __start_hook_##NAME; it != (hook_fn_t *) (uintptr_t) __stop_hook_##NAME; ++it) (*it)();

#define HOOK(NAME)                                                                                                    \
    static void HOOK_UTIL_NAME();                                                                                     \
    [[gnu::used, gnu::section("hook_" #NAME)]] static void (*MACROS_CONCAT(g_, HOOK_UTIL_NAME))() = (HOOK_UTIL_NAME); \
    static void HOOK_UTIL_NAME()
