#pragma once

#include "common/panic.h"

#ifdef __ENV_PRODUCTION
#define ASSERT(ASSERTION)
#else
/**
 * @brief Make an assertion and panic on failure.
 */
#define ASSERT(ASSERTION)                                                       \
    if(!(ASSERTION)) panic("Assertion \"%s\" failed in " __FILE__, #ASSERTION);
#endif


#ifdef __ENV_PRODUCTION
#define ASSERT_UNREACHABLE() __builtin_unreachable()
#else
/**
 * @brief Assert is unreachable, if reached panic.
 */
#define ASSERT_UNREACHABLE()                            \
    panic("Unreachable assertion failed in " __FILE__); \
    __builtin_unreachable()
#endif


#ifdef __ENV_PRODUCTION
#define ASSERT_UNREACHABLE_COMMENT(COMMENT) __builtin_unreachable()
#else
/**
 * @brief Assert is unreachable, if reached panic with a comment.
 */
#define ASSERT_UNREACHABLE_COMMENT(COMMENT)                                \
    panic("Unreachable assertion failed in " __FILE__ " \"%s\"", COMMENT); \
    __builtin_unreachable()
#endif
