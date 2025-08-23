#pragma once

#include "common/panic.h"
#include "lib/macros.h"

#ifdef __ENV_PRODUCTION
#define ASSERT(ASSERTION)
#else
/// Make an assertion and panic on failure.
#define ASSERT(ASSERTION)                                                                                                    \
    ({                                                                                                                       \
        if(!(ASSERTION)) panic("ASSERT", "Assertion \"%s\" failed in " __FILE__ ":" MACROS_STRINGIFY(__LINE__), #ASSERTION); \
    })
#endif

#ifdef __ENV_PRODUCTION
#define ASSERT_COMMENT(ASSERTION, COMMENT)
#else
/// Make an assertion and panic with a comment on failure.
#define ASSERT_COMMENT(ASSERTION, COMMENT)                                                                                                      \
    ({                                                                                                                                          \
        if(!(ASSERTION)) panic("ASSERT", "Assertion \"%s\" failed in " __FILE__ ":" MACROS_STRINGIFY(__LINE__) " \"%s\"", #ASSERTION, COMMENT); \
    })
#endif

#ifdef __ENV_PRODUCTION
#define ASSERT_UNREACHABLE() __builtin_unreachable()
#else
/// Assert is unreachable, if reached panic.
#define ASSERT_UNREACHABLE()                                                     \
    ({                                                                           \
        panic("ASSERT", "Unreachable " __FILE__ ":" MACROS_STRINGIFY(__LINE__)); \
        __builtin_unreachable();                                                 \
    })
#endif


#ifdef __ENV_PRODUCTION
#define ASSERT_UNREACHABLE_COMMENT(COMMENT) __builtin_unreachable()
#else
/// Assert is unreachable, if reached panic with a comment.
#define ASSERT_UNREACHABLE_COMMENT(COMMENT)                                                         \
    ({                                                                                              \
        panic("ASSERT", "Unreachable " __FILE__ ":" MACROS_STRINGIFY(__LINE__) " \"%s\"", COMMENT); \
        __builtin_unreachable();                                                                    \
    })
#endif
