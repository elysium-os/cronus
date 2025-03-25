#pragma once

#ifdef __ARCH_X86_64

#define ARCH_MEM_LOW_SIZE 0x100'0000
#define ARCH_MEM_LOW_MASK 0xFF'FFFF

#else
#error Unimplemented
#endif

#if !defined(ARCH_MEM_LOW_SIZE) || !defined(ARCH_MEM_LOW_MASK)
#error Missing implementation
#endif
