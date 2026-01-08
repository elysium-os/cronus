#include "x86_64/cpu/fpu.h"

#include "common/assert.h"
#include "sys/init.h"
#include "x86_64/cpu/cpuid.h"
#include "x86_64/cpu/cr.h"

uint32_t g_x86_64_fpu_area_size = 0;
void (*g_x86_64_fpu_save)(void *area) = nullptr;
void (*g_x86_64_fpu_restore)(void *area) = nullptr;

static inline void xsave(void *area) {
    asm volatile("xsaveq (%0)" : : "r"(area), "a"(0xFFFF'FFFF), "d"(0xFFFF'FFFF) : "memory");
}

static inline void xrstor(void *area) {
    asm volatile("xrstorq (%0)" : : "r"(area), "a"(0xFFFF'FFFF), "d"(0xFFFF'FFFF) : "memory");
}

static inline void fxsave(void *area) {
    asm volatile("fxsaveq (%0)" : : "r"(area) : "memory");
}

static inline void fxrstor(void *area) {
    asm volatile("fxrstorq (%0)" : : "r"(area) : "memory");
}

INIT_TARGET(fpu, INIT_STAGE_MAIN, INIT_SCOPE_BSP, INIT_DEPS()) {
    if(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_XSAVE)) {
        uint32_t area_size = 0;
        bool success = !x86_64_cpuid_register(0xD, X86_64_CPUID_REGISTER_ECX, &area_size);
        ASSERT(success);
        ASSERT(area_size > 0);
        g_x86_64_fpu_area_size = area_size;
        g_x86_64_fpu_save = xsave;
        g_x86_64_fpu_restore = xrstor;
    } else {
        g_x86_64_fpu_area_size = 512;
        g_x86_64_fpu_save = fxsave;
        g_x86_64_fpu_restore = fxrstor;
    }
}

INIT_TARGET(fpu_cpu, INIT_STAGE_MAIN, INIT_SCOPE_ALL, INIT_DEPS("fpu")) {
    ASSERT(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_FXSR));

    /* Enable FPU */
    uint64_t cr0 = x86_64_cr0_read();
    cr0 &= ~(1 << 2); /* CR0.EM */
    cr0 |= 1 << 1; /* CR0.MP */
    x86_64_cr0_write(cr0);

    /* Enable MMX & friends */
    uint64_t cr4 = x86_64_cr4_read();
    cr4 |= 1 << 9; /* CR4.OSFXSR */
    cr4 |= 1 << 10; /* CR4.OSXMMEXCPT */
    x86_64_cr4_write(cr4);

    if(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_XSAVE)) {
        cr4 = x86_64_cr4_read();
        cr4 |= 1 << 18; /* CR4.OSXSAVE */
        x86_64_cr4_write(cr4);

        uint64_t xcr0 = 0;
        xcr0 |= 1 << 0; /* XCR0.X87 */
        xcr0 |= 1 << 1; /* XCR0.SSE */
        if(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_AVX)) xcr0 |= 1 << 2; /* XCR0.AVX */
        if(x86_64_cpuid_feature(X86_64_CPUID_FEATURE_AVX512)) {
            xcr0 |= 1 << 5; /* XCR0.opmask */
            xcr0 |= 1 << 6; /* XCR0.ZMM_Hi256 */
            xcr0 |= 1 << 7; /* XCR0.Hi16_ZMM */
        }
        asm volatile("xsetbv" : : "a"(xcr0), "d"(xcr0 >> 32), "c"(0) : "memory");
    }
}
