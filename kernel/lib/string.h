#pragma once

#include <stddef.h>

/**
 * @brief Compute the length of a string. Excluding the null terminator.
 */
size_t string_length(const char *str);

/**
 * @brief Compare two strings.
 * @returns Negative value LHS is greater, positive value RHS is greater, zero equal.
 */
int string_cmp(const char *lhs, const char *rhs);

/**
 * @brief Test if two strings are equal.
 */
bool string_eq(const char *lhs, const char *rhs);

/**
 * @brief Copy string. Including the null terminator.
 */
char *string_copy(char *dest, const char *src);

/**
 * @brief Copy string. If null terminator is reached, the rest is padded with `\0`.
 */
char *string_ncopy(char *dest, const char *src, size_t n);
