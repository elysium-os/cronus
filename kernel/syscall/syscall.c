#include "syscall.h"

#include "abi/syscall/syscall.h"
#include "arch/sched.h"
#include "common/assert.h"
#include "common/log.h"
#include "lib/string.h"
#include "memory/heap.h"
#include "memory/vm.h"

#include <stddef.h>
#include <stdint.h>

#define MAX_DEBUG_LENGTH 512

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

[[noreturn]] void syscall_exit(int code, bool is_panic) {
    log(LOG_LEVEL_DEBUG, "SYSCALL", "exit(code: %i, is_panic: %s, tid: %li)", code, is_panic ? "true" : "false", arch_sched_thread_current()->id);
    sched_yield(THREAD_STATE_DESTROY);
    ASSERT_UNREACHABLE();
}

syscall_return_t syscall_debug(size_t length, char *str) {
    syscall_return_t ret = {};

    if(length > MAX_DEBUG_LENGTH) {
        log(LOG_LEVEL_WARN, "SYSCALL_DEBUG", "exceeded maximum length (%lu)", length);
        length = MAX_DEBUG_LENGTH;
    }

    str = syscall_string_in(str, length);
    if(str == NULL) {
        ret.error = SYSCALL_ERROR_INVALID_VALUE;
        return ret;
    }

    log(LOG_LEVEL_INFO, "SYSCALL_DEBUG", "%s", str);

    heap_free(str, length + 1);
    return ret;
}

syscall_return_t syscall_system_info(syscall_system_info_t *buffer) {
    syscall_return_t ret = {};

    log(LOG_LEVEL_DEBUG, "SYSCALL", "system_info(buffer: %#lx)", (uintptr_t) buffer);

    syscall_string_out(buffer->release, "pre-alpha", sizeof(buffer->release));
    syscall_string_out(buffer->version, "(" __DATE__ " " __TIME__ ")", sizeof(buffer->release));

    return ret;
}
