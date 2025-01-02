#include "ipl.h"

#include "arch/interrupt.h"
#include "common/assert.h"

ipl_t ipl_raise(ipl_t ipl_to) {
    ipl_t current_ipl = arch_interrupt_get_ipl();
    if(ipl_to == current_ipl) return current_ipl;
    ASSERT(ipl_to > current_ipl);
    arch_interrupt_set_ipl(ipl_to);
    return current_ipl;
}

void ipl_lower(ipl_t ipl_to) {
    ipl_t current_ipl = arch_interrupt_get_ipl();
    if(ipl_to == current_ipl) return;
    ASSERT(ipl_to < current_ipl);
    arch_interrupt_set_ipl(ipl_to);
}
