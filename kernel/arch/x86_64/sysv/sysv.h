#pragma once

#include "memory/vm.h"

#include "arch/x86_64/sysv/auxv.h"

/**
 * @brief Setup stack according to SysV ABI.
 * @param stack_size size of stack aligned to page granularity
 */
uintptr_t x86_64_sysv_stack_setup(vm_address_space_t *address_space, size_t stack_size, char **argv, char **envp, x86_64_auxv_t *auxv);
