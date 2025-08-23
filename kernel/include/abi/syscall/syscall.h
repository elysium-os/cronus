#pragma once

#include <stdint.h>

#define SYSCALL_EXIT 0
#define SYSCALL_DEBUG 1
#define SYSCALL_SYSINFO 2
#define SYSCALL_ANON_ALLOC 3
#define SYSCALL_ANON_FREE 4
#define SYSCALL_SET_TCB 5

typedef struct {
    char release[32];
    char version[64];
} syscall_system_info_t;

typedef uint64_t syscall_int_t;

typedef enum : syscall_int_t {
    SYSCALL_ERROR_NONE = 0, // this is assumed to be zero
    SYSCALL_ERROR_INVALID_VALUE
} syscall_error_t;

typedef struct {
    syscall_int_t value;
    syscall_error_t error;
} syscall_return_t;
