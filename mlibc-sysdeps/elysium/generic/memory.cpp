#include <elysium/syscall.h>
#include <errno.h>
#include <mlibc/all-sysdeps.hpp>
#include <mlibc/debug.hpp>

namespace mlibc {

static int syscall_error_to_errno(syscall_error_t error) {
    switch(error) {
        case SYSCALL_ERROR_NONE:          return 0;
        case SYSCALL_ERROR_INVALID_VALUE: return EINVAL;
    }
    __builtin_unreachable();
}

int sys_anon_allocate(size_t size, void **pointer) {
    syscall_return_t ret = syscall1(SYSCALL_ANON_ALLOC, size);
    if(ret.error != SYSCALL_ERROR_NONE) return syscall_error_to_errno(ret.error);
    *pointer = (void *) ret.value;
    return 0;
}

int sys_anon_free(void *pointer, size_t size) {
    return syscall_error_to_errno(syscall2(SYSCALL_ANON_FREE, (syscall_int_t) pointer, size).error);
}

} // namespace mlibc
