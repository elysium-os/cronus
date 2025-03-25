#pragma once

#include <stddef.h>

/**
 * @brief Fill memory with a value.
 * @param ch value to fill memory with
 * @param count amount of memory to fill
 */
void memset(void *dest, int ch, size_t count);

/**
 * @brief Copy memory. `dest` and `src` are not allowed to overlap
 * @param count amount of memory to copy
 */
void memcpy(void *dest, const void *src, size_t count);

/**
 * @brief Copy memory. `dest` and `src` are allowed to overlap.
 * @param count amount of memory to move
 */
void memmove(void *dest, const void *src, size_t count);

/**
 * @brief Compare two regions of memory. Results in a negative value if the LHS is greater, a positive value if the RHS is greater, and a zero if they're equal.
 * @param lhs left-hand side of the comparison
 * @param rhs light-hand side of the comparison
 * @param count amount of memory to compare
 * @returns result of the comparison
 */
int memcmp(const void *lhs, const void *rhs, size_t count);

/**
 * @brief Fill memory with zeroes.
 */
void memclear(void *dest, size_t count);
