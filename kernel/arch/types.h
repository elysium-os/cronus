#pragma once

#ifdef __ARCH_X86_64

#define ARCH_TYPES_ELF_MACHINE 0x3E

#endif

#if !defined(ARCH_TYPES_ELF_MACHINE)
#error Missing implementation
#endif
