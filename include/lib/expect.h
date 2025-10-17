#pragma once

#define EXPECT_LIKELY(V) (__builtin_expect(!!(V), 1))
#define EXPECT_UNLIKELY(V) (__builtin_expect(!!(V), 0))
