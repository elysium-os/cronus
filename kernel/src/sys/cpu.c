#include "cpu.h"

#include "arch/sched.h"

cpu_t *cpu_current() {
    return arch_sched_thread_current()->cpu;
}