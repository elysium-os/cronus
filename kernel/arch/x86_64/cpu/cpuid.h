#pragma once

#include <stdint.h>

#define X86_64_CPUID_DEFINE_FEATURE(LEAF, REGISTER, BIT) ((x86_64_cpuid_feature_t) { .leaf = (LEAF), .reg = (REGISTER), .bit = (BIT) })

#define X86_64_CPUID_FEATURE_SSE3 X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 0)
#define X86_64_CPUID_FEATURE_PCLMUL X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 1)
#define X86_64_CPUID_FEATURE_DTES64 X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 2)
#define X86_64_CPUID_FEATURE_MONITOR X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 3)
#define X86_64_CPUID_FEATURE_DS_CPL X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 4)
#define X86_64_CPUID_FEATURE_VMX X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 5)
#define X86_64_CPUID_FEATURE_SMX X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 6)
#define X86_64_CPUID_FEATURE_EST X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 7)
#define X86_64_CPUID_FEATURE_TM2 X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 8)
#define X86_64_CPUID_FEATURE_SSSE3 X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 9)
#define X86_64_CPUID_FEATURE_CID X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 10)
#define X86_64_CPUID_FEATURE_SDBG X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 11)
#define X86_64_CPUID_FEATURE_FMA X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 12)
#define X86_64_CPUID_FEATURE_CX16 X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 13)
#define X86_64_CPUID_FEATURE_XTPR X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 14)
#define X86_64_CPUID_FEATURE_PDCM X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 15)
#define X86_64_CPUID_FEATURE_PCID X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 17)
#define X86_64_CPUID_FEATURE_DCA X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 18)
#define X86_64_CPUID_FEATURE_SSE4_1 X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 19)
#define X86_64_CPUID_FEATURE_SSE4_2 X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 20)
#define X86_64_CPUID_FEATURE_X2APIC X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 21)
#define X86_64_CPUID_FEATURE_MOVBE X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 22)
#define X86_64_CPUID_FEATURE_POPCNT X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 23)
#define X86_64_CPUID_FEATURE_TSC_DEADLINE X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 24)
#define X86_64_CPUID_FEATURE_AES X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 25)
#define X86_64_CPUID_FEATURE_XSAVE X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 26)
#define X86_64_CPUID_FEATURE_OSXSAVE X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 27)
#define X86_64_CPUID_FEATURE_AVX X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 28)
#define X86_64_CPUID_FEATURE_F16C X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 29)
#define X86_64_CPUID_FEATURE_RDRAND X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 30)
#define X86_64_CPUID_FEATURE_HYPERVISOR X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_ECX, 31)
#define X86_64_CPUID_FEATURE_FPU X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 0)
#define X86_64_CPUID_FEATURE_VME X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 1)
#define X86_64_CPUID_FEATURE_DE X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 2)
#define X86_64_CPUID_FEATURE_PSE X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 3)
#define X86_64_CPUID_FEATURE_TSC X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 4)
#define X86_64_CPUID_FEATURE_MSR X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 5)
#define X86_64_CPUID_FEATURE_PAE X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 6)
#define X86_64_CPUID_FEATURE_MCE X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 7)
#define X86_64_CPUID_FEATURE_CX8 X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 8)
#define X86_64_CPUID_FEATURE_APIC X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 9)
#define X86_64_CPUID_FEATURE_SEP X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 11)
#define X86_64_CPUID_FEATURE_MTRR X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 12)
#define X86_64_CPUID_FEATURE_PGE X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 13)
#define X86_64_CPUID_FEATURE_MCA X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 14)
#define X86_64_CPUID_FEATURE_CMOV X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 15)
#define X86_64_CPUID_FEATURE_PAT X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 16)
#define X86_64_CPUID_FEATURE_PSE36 X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 17)
#define X86_64_CPUID_FEATURE_PSN X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 18)
#define X86_64_CPUID_FEATURE_CLFLUSH X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 19)
#define X86_64_CPUID_FEATURE_DS X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 21)
#define X86_64_CPUID_FEATURE_ACPI X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 22)
#define X86_64_CPUID_FEATURE_MMX X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 23)
#define X86_64_CPUID_FEATURE_FXSR X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 24)
#define X86_64_CPUID_FEATURE_SSE X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 25)
#define X86_64_CPUID_FEATURE_SSE2 X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 26)
#define X86_64_CPUID_FEATURE_SS X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 27)
#define X86_64_CPUID_FEATURE_HTT X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 28)
#define X86_64_CPUID_FEATURE_TM X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 29)
#define X86_64_CPUID_FEATURE_IA64 X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 30)
#define X86_64_CPUID_FEATURE_PBE X86_64_CPUID_DEFINE_FEATURE(1, X86_64_CPUID_REGISTER_EDX, 31)
#define X86_64_CPUID_FEATURE_AVX512 X86_64_CPUID_DEFINE_FEATURE(7, X86_64_CPUID_REGISTER_EBX, 16)

typedef enum {
    X86_64_CPUID_REGISTER_EAX,
    X86_64_CPUID_REGISTER_EBX,
    X86_64_CPUID_REGISTER_ECX,
    X86_64_CPUID_REGISTER_EDX
} x86_64_cpuid_register_t;

typedef struct {
    uint32_t leaf;
    x86_64_cpuid_register_t reg;
    uint32_t bit;
} x86_64_cpuid_feature_t;

/**
 * @brief Test for a feature exposed in CPUID.
 * @param feature feature in CPUID
 * @retval true = supported
 * @retval false = unsupported
 */
bool x86_64_cpuid_feature(x86_64_cpuid_feature_t feature);

/**
 * @brief Retrieve the value from a specific register exposed by CPUID.
 * @param out value returned by CPUID
 * @returns false = success
 */
bool x86_64_cpuid_register(uint32_t leaf, x86_64_cpuid_register_t reg, uint32_t *out);
