#include "sysv.h"

#include "arch/page.h"
#include "common/assert.h"
#include "lib/string.h"
#include "memory/vm.h"

#include <stdint.h>

uintptr_t x86_64_sysv_stack_setup(vm_address_space_t *address_space, size_t stack_size, char **argv, char **envp, x86_64_auxv_t *auxv) {
    ASSERT(stack_size % ARCH_PAGE_GRANULARITY == 0);

#define WRITE_QWORD(VALUE)                                               \
    {                                                                    \
        stack -= sizeof(uint64_t);                                       \
        uint64_t tmp = (VALUE);                                          \
        size_t copyto_count = vm_copy_to(address_space, stack, &tmp, 4); \
        ASSERT(copyto_count == 4);                                       \
    }

    void *stack_ptr = vm_map_anon(address_space, NULL, stack_size, (vm_protection_t) { .read = true, .write = true }, VM_CACHE_STANDARD, VM_FLAG_NONE);
    ASSERT(stack_ptr != NULL);
    uintptr_t stack = (uintptr_t) stack_ptr + stack_size - 1;
    stack &= ~0xF;

    int argc = 0;
    for(; argv[argc]; argc++) stack -= string_length(argv[argc]) + 1;
    uintptr_t arg_data = stack;

    int envc = 0;
    for(; envp[envc]; envc++) stack -= string_length(envp[envc]) + 1;
    uintptr_t env_data = stack;

    stack -= (stack - (12 + 1 + envc + 1 + argc + 1) * sizeof(uint64_t)) % 0x10;

#define WRITE_AUX(ID, VALUE) \
    {                        \
        WRITE_QWORD(VALUE);  \
        WRITE_QWORD(ID);     \
    }
    WRITE_AUX(0, 0);
    WRITE_AUX(X86_64_AUXV_SECURE, 0);
    WRITE_AUX(X86_64_AUXV_ENTRY, auxv->entry);
    WRITE_AUX(X86_64_AUXV_PHDR, auxv->phdr);
    WRITE_AUX(X86_64_AUXV_PHENT, auxv->phent);
    WRITE_AUX(X86_64_AUXV_PHNUM, auxv->phnum);
#undef WRITE_AUX

    WRITE_QWORD(0);
    for(int i = 0; i < envc; i++) {
        WRITE_QWORD(env_data);
        size_t str_sz = string_length(envp[i]) + 1;
        size_t copyto_count = vm_copy_to(address_space, env_data, envp[i], str_sz);
        ASSERT(copyto_count == str_sz);
        env_data += str_sz;
    }

    WRITE_QWORD(0);
    for(int i = 0; i < argc; i++) {
        WRITE_QWORD(arg_data);
        size_t str_sz = string_length(argv[i]) + 1;
        size_t copyto_count = vm_copy_to(address_space, arg_data, argv[i], str_sz);
        ASSERT(copyto_count == str_sz);
        arg_data += str_sz;
    }
    WRITE_QWORD(argc);

    return stack;
#undef WRITE_QWORD
}
