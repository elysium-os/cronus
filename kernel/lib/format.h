#pragma once

#include <stdarg.h>

typedef void (*format_writer_t)(char ch);

/**
 * @brief Format a string as described by the C specification.
 * @param writer function to handle output
 * @returns character count written
 */
int format(format_writer_t writer, const char *format, va_list list);
