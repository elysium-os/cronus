#include "arch/sched.h"
#include "common/assert.h"
#include "common/log.h"
#include "lib/string.h"
#include "memory/heap.h"
#include "memory/vm.h"

#include <elysium/syscall.h>
#include <stddef.h>

int syscall_buffer_out(void *dest, void *src, size_t count) {
    ASSERT(arch_sched_thread_current()->proc != NULL);
    return vm_copy_to(arch_sched_thread_current()->proc->address_space, (uintptr_t) dest, src, count);
}

void *syscall_buffer_in(void *src, size_t count) {
    ASSERT(arch_sched_thread_current()->proc != NULL);
    void *buffer = heap_alloc(count);
    size_t read_count = vm_copy_from(buffer, arch_sched_thread_current()->proc->address_space, (uintptr_t) src, count);
    if(read_count != count) {
        heap_free(buffer, count);
        return NULL;
    }
    return buffer;
}

int syscall_string_out(char *dest, char *src, size_t max) {
    size_t len = string_length(src) + 1;
    if(max < len) {
        log(LOG_LEVEL_WARN, "SYSCALL", "string_out: string length(%lu) exceeds maximum(%lu)", len, max);
        len = max;
    }
    return syscall_buffer_out(dest, src, len);
}

char *syscall_string_in(char *src, size_t length) {
    char *str = syscall_buffer_in(src, length + 1);
    if(str == NULL) return NULL;
    str[length] = 0;
    return str;
}

void syscall_string_free(char *str, size_t length) {
    heap_free(str, length + 1);
}

[[noreturn]] void syscall_exit(int code, bool panic) {
    log(LOG_LEVEL_DEBUG, "SYSCALL", "exit(code: %i, is_panic: %s, tid: %li)", code, panic ? "true" : "false", arch_sched_thread_current()->id);
    arch_sched_thread_current()->state = THREAD_STATE_DESTROY;
    arch_sched_yield();
    __builtin_unreachable();
}

syscall_return_t syscall_debug(size_t length, char *str) {
    syscall_return_t ret = {};

    str = syscall_string_in(str, length);
    if(str == NULL) {
        syscall_string_free(str, length);
        ret.error = SYSCALL_ERROR_INVALID_VALUE;
        return ret;
    }

    log(LOG_LEVEL_INFO, "SYSCALL", "debug(%s)", str);

    syscall_string_free(str, length);
    return ret;
}
