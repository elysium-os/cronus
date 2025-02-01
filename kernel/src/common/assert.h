#pragma once

#include "common/panic.h"

/**
 * @brief Make an assertion and panic on failure.
 */
#define ASSERT(ASSERTION)                                          \
    if(!(ASSERTION)) panic("Assertion \"%s\" failed", #ASSERTION);

/**
 * @brief Make an assertion and panic with a comment on failure.
 */
#define ASSERT_COMMENT(ASSERTION, COMMENT)                                     \
    if(!(ASSERTION)) panic("Assertion \"%s\" failed: %s", #ASSERTION, COMMENT)

/**
 * @brief Assert is unreachable, if reached panic with a comment.
 */
#define ASSERT_UNREACHABLE(COMMENT)                     \
    panic("Assertion unreachable failed: %s", COMMENT); \
    __builtin_unreachable()
