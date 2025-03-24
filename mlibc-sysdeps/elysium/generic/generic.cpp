#include <bits/ensure.h>
#include <elysium/syscall.h>
#include <errno.h>
#include <mlibc/all-sysdeps.hpp>
#include <mlibc/debug.hpp>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace mlibc {

void sys_libc_log(const char *message) {
    syscall2(SYSCALL_DEBUG, strlen(message), (syscall_int_t) message);
}

[[noreturn]] void sys_libc_panic() {
    syscall2(SYSCALL_EXIT, 0, true);
    __builtin_unreachable();
}

[[noreturn]] void sys_exit(int status) {
    syscall2(SYSCALL_EXIT, status, false);
    __builtin_unreachable();
}

int sys_tcb_set(void *pointer) {
    syscall1(SYSCALL_SET_TCB, (syscall_int_t) pointer);
    return 0;
}

int sys_futex_wait(int *pointer [[maybe_unused]], int expected [[maybe_unused]], const struct timespec *time [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_futex_wait called" << frg::endlog;
    return -1;
}

int sys_futex_wake(int *pointer [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_futex_wake called" << frg::endlog;
    return -1;
}

int sys_openat(int dirfd [[maybe_unused]], const char *path [[maybe_unused]], int flags [[maybe_unused]], mode_t mode [[maybe_unused]], int *fd [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_openat called" << frg::endlog;
    return -1;
}

int sys_open(const char *path [[maybe_unused]], int flags [[maybe_unused]], mode_t mode [[maybe_unused]], int *fd [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_open called" << frg::endlog;
    return -1;
}

int sys_read(int fd [[maybe_unused]], void *buf [[maybe_unused]], size_t count [[maybe_unused]], ssize_t *bytes_read [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_read called" << frg::endlog;
    return -1;
}

int sys_write(int fd [[maybe_unused]], const void *buf [[maybe_unused]], size_t count [[maybe_unused]], ssize_t *bytes_written [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_write called" << frg::endlog;
    return -1;
}

int sys_seek(int fd [[maybe_unused]], off_t offset [[maybe_unused]], int whence [[maybe_unused]], off_t *new_offset [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_seek called" << frg::endlog;
    return -1;
}

int sys_close(int fd [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_close called" << frg::endlog;
    return -1;
}

int sys_stat(fsfd_target fsfdt [[maybe_unused]], int fd [[maybe_unused]], const char *path [[maybe_unused]], int flags [[maybe_unused]], struct stat *statbuf [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_stat called" << frg::endlog;
    return -1;
}

// mlibc assumes that anonymous memory returned by sys_vm_map() is zeroed by the kernel / whatever is behind the sysdeps
int sys_vm_map(void *hint [[maybe_unused]], size_t size [[maybe_unused]], int prot [[maybe_unused]], int flags [[maybe_unused]], int fd [[maybe_unused]], off_t offset [[maybe_unused]], void **window [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_vm_map called" << frg::endlog;
    return -1;
}

int sys_vm_unmap(void *pointer [[maybe_unused]], size_t size [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_vm_unmap called" << frg::endlog;
    return -1;
}

int sys_vm_protect(void *pointer [[maybe_unused]], size_t size [[maybe_unused]], int prot [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_vm_protect called" << frg::endlog;
    return -1;
}

int sys_clock_getres(int clock [[maybe_unused]], time_t *secs [[maybe_unused]], long *nanos [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_clock_getres called" << frg::endlog;
    return -1;
}

int sys_clock_get(int clock [[maybe_unused]], time_t *secs [[maybe_unused]], long *nanos [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_clock_get called" << frg::endlog;
    return -1;
}

int sys_isatty(int fd [[maybe_unused]]) {
    // TODO: Implement
    return ENOTTY;
}

int sys_getcwd(char *buffer [[maybe_unused]], size_t size [[maybe_unused]]) {
    // TODO: Implement
    mlibc::infoLogger() << "unimplemented sys_getcwd called" << frg::endlog;
    return -1;
}

} // namespace mlibc
