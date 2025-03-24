#pragma once

#include <stddef.h>

/**
 * @brief Write a buffer to userspace safely.
 * @warning only intended for syscall handlers
 */
int syscall_buffer_out(void *dest, void *src, size_t count);

/**
 * @brief Read a buffer from userspace safely.
 * @warning only intended for syscall handlers
 */
void *syscall_buffer_in(void *src, size_t count);

/**
 * @brief Write a string to userspace safely.
 * @warning only intended for syscall handlers
 * @param max max string length
 */
int syscall_string_out(char *dest, char *src, size_t max);

/**
 * @brief Read a string from userspace safely.
 * @warning only intended for syscall handlers
 */
char *syscall_string_in(char *src, size_t length);
