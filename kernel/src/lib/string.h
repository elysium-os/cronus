#pragma once
#include <stddef.h>

/**
 * @brief Retrieve the length of a string. Not including the null terminator.
 * @param str string to compute the length of
 * @returns string length
 */
size_t strlen(const char *str);

/**
 * @brief Compare two strings. Results in a negative value if the LHS string is greater, a positive number if the RHS is greater, and a zero if they're equal.
 * @param lhs left-hand side string of the comparison
 * @param rhs right-hand side string of the comparison
 * @returns result
 */
int strcmp(const char *lhs, const char *rhs);

/**
 * @brief Copy a string. Including the null terminator.
 * @param dest
 * @param src
 * @returns destination
 */
char *strcpy(char *dest, const char *src);

/**
 * @brief Copy a string. If a null terminator is reached, the string will be padded with NULL's.
 * @param dest
 * @param src
 * @param n size
 * @returns destination
 */
char *strncpy(char *dest, const char *src, size_t n);

/**
 * @brief Fill memory with a value.
 * @param dest
 * @param ch value to fill memory with
 * @param count amount of memory to fill
 * @returns destination
 */
void *memset(void *dest, int ch, size_t count);

/**
 * @brief Copy memory.
 * @param dest
 * @param src
 * @param count amount of memory to copy
 * @returns destination
 */
void *memcpy(void *dest, const void *src, size_t count);

/**
 * @brief Compare two regions of memory. Results in a negative value if the LHS is greater, a positive value if the RHS is greater, and a zero if they're equal.
 * @param lhs left-hand side of the comparison
 * @param rhs light-hand side of the comparison
 * @param count amount of memory to compare
 * @returns result of the comparison
 */
int memcmp(const void *lhs, const void *rhs, size_t count);