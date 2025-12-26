#pragma once

#include <stddef.h>
#include <tartarus.h>

// Initialize the BSP.
void arch_init_bsp();

/// Initialize the aps & change bsp to permanent cpu local.
void arch_init_smp(tartarus_boot_info_t *boot_info);
