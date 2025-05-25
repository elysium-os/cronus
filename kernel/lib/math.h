#pragma once

/// Divide and round up.
#define MATH_DIV_CEIL(DIVIDEND, DIVISOR) (((DIVIDEND) + (DIVISOR) - 1) / (DIVISOR))

/// Round up.
#define MATH_CEIL(VALUE, PRECISION) (MATH_DIV_CEIL((VALUE), (PRECISION)) * (PRECISION))

/// Round down.
#define MATH_FLOOR(VALUE, PRECISION) (((VALUE) / (PRECISION)) * (PRECISION))

/// Find the minimum number.
static inline int math_min(int a, int b) {
    return a < b ? a : b;
}

/// Find the maximum number.
static inline int math_max(int a, int b) {
    return a > b ? a : b;
}
