#include "ipl.h"

#include "arch/interrupt.h"

ipl_t ipl(ipl_t ipl) {
    ipl_t old = arch_interrupt_get_ipl();
    arch_interrupt_set_ipl(ipl);
    return old;
}