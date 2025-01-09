#include "mutex.h"

#include "arch/cpu.h"

void mutex_acquire(mutex_t *mutex) {
    ipl_t previous_ipl = ipl_raise(IPL_NORMAL);

    while(true) {
        bool acquired = true;
        size_t spin = 100000;
        while(__atomic_test_and_set(&mutex->lock, __ATOMIC_ACQUIRE)) {
            while(__atomic_load_n(&mutex->lock, __ATOMIC_RELAXED)) {
                arch_cpu_relax();
                if(--spin == 0) break;
            }

            if(spin == 0) {
                acquired = false;
                break;
            }
        }
        if(acquired) break;

        wait_on(&mutex->waitable);
    }

    ipl_lower(previous_ipl);
}

void mutex_release(mutex_t *mutex) {
    __atomic_clear(&mutex->lock, __ATOMIC_RELEASE);
    wait_signal(&mutex->waitable);
}