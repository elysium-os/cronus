#pragma once

/// Divide and round up.
#define MATH_DIV_CEIL(DIVIDEND, DIVISOR)      \
    ({                                        \
        auto divisor = (DIVISOR);             \
        ((DIVIDEND) + divisor - 1) / divisor; \
    })

/// Round up.
#define MATH_CEIL(VALUE, PRECISION)                    \
    ({                                                 \
        auto precision = (PRECISION);                  \
        MATH_DIV_CEIL((VALUE), precision) * precision; \
    })

/// Round down.
#define MATH_FLOOR(VALUE, PRECISION)       \
    ({                                     \
        auto precision = (PRECISION);      \
        ((VALUE) / precision) * precision; \
    })

/// Find the minimum number.
#define MATH_MIN(A, B) \
    ({                 \
        auto a = (A);  \
        auto b = (B);  \
        a < b ? a : b; \
    })

/// Find the maximum number.
#define MATH_MAX(A, B) \
    ({                 \
        auto a = (A);  \
        auto b = (B);  \
        a > b ? a : b; \
    })

// Clamp value between min and max.
#define MATH_CLAMP(VALUE, MIN, MAX) MATH_MAX(MATH_MIN((VALUE), (MAX)), (MIN))
