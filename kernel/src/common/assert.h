#pragma once

#include "common/panic.h"

/**
 * @brief Make an assertion and panic on failure.
 */
#ifdef __ENV_PRODUCTION
#define ASSERT(ASSERTION) ({ ASSERTION; })
#else
#define ASSERT(ASSERTION)                                          \
    if(!(ASSERTION)) panic("Assertion \"%s\" failed", #ASSERTION);
#endif

/**
 * @brief Make an assertion and panic with a comment on failure.
 */
#ifdef __ENV_PRODUCTION
#define ASSERT_COMMENT(ASSERTION, COMMENT) ({ ASSERTION; })
#else
#define ASSERT_COMMENT(ASSERTION, COMMENT)                                     \
    if(!(ASSERTION)) panic("Assertion \"%s\" failed: %s", #ASSERTION, COMMENT)
#endif

/**
 * @brief Assert is unreachable, if reached panic with a comment.
 */
#ifdef __ENV_PRODUCTION
#define ASSERT_UNREACHABLE(COMMENT) __builtin_unreachable()
#else
#define ASSERT_UNREACHABLE(COMMENT)                     \
    panic("Assertion unreachable failed: %s", COMMENT); \
    __builtin_unreachable()
#endif
