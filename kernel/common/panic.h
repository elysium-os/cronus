#pragma once

/**
 * @brief Log error, then halt.
 */
[[noreturn]] void panic(const char *tag, const char *fmt, ...);
