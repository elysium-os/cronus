#pragma once

#include <stddef.h>

/// Fill memory with a value.
/// @param ch Value to fill memory with
/// @param count Amount of memory to fill
void memset(void *dest, int ch, size_t count);

/// Copy memory. `dest` and `src` are not allowed to overlap
/// @param count Amount of memory to copy
void memcpy(void *dest, const void *src, size_t count);

/// Copy memory. `dest` and `src` are allowed to overlap.
/// @param count Amount of memory to move
void memmove(void *dest, const void *src, size_t count);

/// Compare two regions of memory. Results in a negative value if the LHS is greater, a positive value if the RHS is greater, and a zero if they're equal.
/// @param lhs Left-hand side of the comparison
/// @param rhs Light-hand side of the comparison
/// @param count Amount of memory to compare
/// @returns Result of the comparison
int memcmp(const void *lhs, const void *rhs, size_t count);

/// Fill memory with zeroes.
void memclear(void *dest, size_t count);
