#include "x86_64/cpu/cpuid.h"

bool x86_64_cpuid_feature(x86_64_cpuid_feature_t feature) {
    uint32_t value = 0;
    if(x86_64_cpuid_register(feature.leaf, feature.reg, &value)) return false;
    return (value & (1 << feature.bit));
}

bool x86_64_cpuid_register(uint32_t leaf, x86_64_cpuid_register_t reg, uint32_t *out) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(leaf), "c"(0));
    switch(reg) {
        case X86_64_CPUID_REGISTER_EAX: *out = eax; break;
        case X86_64_CPUID_REGISTER_EBX: *out = ebx; break;
        case X86_64_CPUID_REGISTER_ECX: *out = ecx; break;
        case X86_64_CPUID_REGISTER_EDX: *out = edx; break;
    }
    return false;
}
