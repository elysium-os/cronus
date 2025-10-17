#pragma once

#include <stddef.h>
#include <tartarus.h>

/// Initialize the bsp (early cpu local).
void arch_init_bsp_local(size_t seqid);

/// Initialize the bsp (cpu state).
void arch_init_bsp();

/// Initialize proper cpu locals (allocates and moves the bsp local).
void arch_init_cpu_locals(tartarus_boot_info_t *boot_info);

/// Initialize the aps & change bsp to permanent cpu local.
void arch_init_smp(tartarus_boot_info_t *boot_info);
