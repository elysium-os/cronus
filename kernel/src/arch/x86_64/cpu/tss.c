#include "tss.h"

void x86_64_tss_set_rsp0(x86_64_tss_t *tss, uintptr_t stack_pointer) {
    tss->rsp0_lower = (uint32_t) (uint64_t) stack_pointer;
    tss->rsp0_upper = (uint32_t) ((uint64_t) stack_pointer >> 32);
}
