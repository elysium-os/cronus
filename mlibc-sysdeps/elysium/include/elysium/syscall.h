#ifndef _ELYSIUM__SYSCALL_H
#define _ELYSIUM__SYSCALL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#ifndef __MLIBC_ABI_ONLY
#define DEFINE_SYSCALL(...)                                                                                   \
    syscall_return_t ret;                                                                                     \
    asm volatile("syscall" : "=a"(ret.value), "=b"(ret.error) : __VA_ARGS__ : "rcx", "r11", "r15", "memory"); \
    return ret;

#ifdef __cplusplus
extern "C" {
#endif

[[maybe_unused]] static syscall_return_t syscall0(int sc) {
    DEFINE_SYSCALL("a"(sc));
}

[[maybe_unused]] static syscall_return_t syscall1(int sc, syscall_int_t arg1) {
    DEFINE_SYSCALL("a"(sc), "D"(arg1));
}

[[maybe_unused]] static syscall_return_t syscall2(int sc, syscall_int_t arg1, syscall_int_t arg2) {
    DEFINE_SYSCALL("a"(sc), "D"(arg1), "S"(arg2));
}

[[maybe_unused]] static syscall_return_t syscall3(int sc, syscall_int_t arg1, syscall_int_t arg2, syscall_int_t arg3) {
    DEFINE_SYSCALL("a"(sc), "D"(arg1), "S"(arg2), "d"(arg3));
}

[[maybe_unused]] static syscall_return_t syscall4(int sc, syscall_int_t arg1, syscall_int_t arg2, syscall_int_t arg3, syscall_int_t arg4) {
    register syscall_int_t arg4_reg asm("r10") = arg4;
    DEFINE_SYSCALL("a"(sc), "D"(arg1), "S"(arg2), "d"(arg3), "r"(arg4_reg));
}

[[maybe_unused]] static syscall_return_t syscall5(int sc, syscall_int_t arg1, syscall_int_t arg2, syscall_int_t arg3, syscall_int_t arg4, syscall_int_t arg5) {
    register syscall_int_t arg4_reg asm("r10") = arg4;
    register syscall_int_t arg5_reg asm("r8") = arg5;
    DEFINE_SYSCALL("a"(sc), "D"(arg1), "S"(arg2), "d"(arg3), "r"(arg4_reg), "r"(arg5_reg));
}

[[maybe_unused]] static syscall_return_t syscall6(int sc, syscall_int_t arg1, syscall_int_t arg2, syscall_int_t arg3, syscall_int_t arg4, syscall_int_t arg5, syscall_int_t arg6) {
    register syscall_int_t arg4_reg asm("r10") = arg4;
    register syscall_int_t arg5_reg asm("r8") = arg5;
    register syscall_int_t arg6_reg asm("r9") = arg6;
    DEFINE_SYSCALL("a"(sc), "D"(arg1), "S"(arg2), "d"(arg3), "r"(arg4_reg), "r"(arg5_reg), "r"(arg6_reg));
}

#ifdef __cplusplus
}
#endif

#endif

#endif
