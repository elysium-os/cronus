#pragma once

/**
 * @brief Log error, then halt.
*/
[[noreturn]] void panic(const char *fmt, ...);