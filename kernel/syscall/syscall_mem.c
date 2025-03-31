#include "arch/page.h"
#include "arch/sched.h"
#include "common/log.h"
#include "memory/vm.h"

#include <elysium/syscall.h>
#include <stddef.h>
#include <stdint.h>

syscall_return_t syscall_mem_anon_allocate(size_t size) {
    syscall_return_t ret = {};

    if(size == 0 || size % ARCH_PAGE_GRANULARITY != 0) {
        ret.error = SYSCALL_ERROR_INVALID_VALUE;
        return ret;
    }

    void *ptr = vm_map_anon(arch_sched_thread_current()->proc->address_space, NULL, size, (vm_protection_t) { .read = true, .write = true }, VM_CACHE_STANDARD, VM_FLAG_ZERO);
    ret.value = (uintptr_t) ptr;
    log(LOG_LEVEL_DEBUG, "SYSCALL", "anon_allocate(size: %#lx) -> %#lx", size, ret.value);
    return ret;
}

syscall_return_t syscall_mem_anon_free(void *pointer, size_t size) {
    syscall_return_t ret = {};
    if(size == 0 || size % ARCH_PAGE_GRANULARITY != 0 || pointer == NULL || ((uintptr_t) pointer) % ARCH_PAGE_GRANULARITY != 0) {
        ret.error = SYSCALL_ERROR_INVALID_VALUE;
        return ret;
    }

    // CRITICAL: ensure this is safe for userspace to just do (currently throws a kern panic...)
    vm_unmap(arch_sched_thread_current()->proc->address_space, pointer, size);
    log(LOG_LEVEL_DEBUG, "SYSCALL", "anon_free(ptr: %#lx, size: %#lx)", (uintptr_t) pointer, size);
    return ret;
}
