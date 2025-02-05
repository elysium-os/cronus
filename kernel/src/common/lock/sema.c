// #include "sema.h"

// #include "sys/ipl.h"

// bool sema_try_wait(sema_t *semaphore) {
//     ipl_t previous_ipl = spinlock_acquire(&semaphore->lock);
//     bool success = semaphore->counter > 0;
//     if(success) semaphore->counter--;
//     spinlock_release(&semaphore->lock, previous_ipl);
//     return success;
// }

// bool sema_wait(sema_t *semaphore, time_t time) {
//     ipl_t previous_ipl = spinlock_acquire(&semaphore->lock);

//     if(semaphore->counter > 0) {
//         semaphore->counter--;
//         spinlock_release(&semaphore->lock, previous_ipl);
//         return true;
//     }
//     spinlock_release(&semaphore->lock, previous_ipl);

//     wait_on(&semaphore->waitable);
//     previous_ipl = spinlock_acquire(&semaphore->lock);
//     semaphore->counter--;
//     spinlock_release(&semaphore->lock, previous_ipl);
//     return false;
// }

// void sema_signal(sema_t *semaphore) {
//     ipl_t previous_ipl = spinlock_acquire(&semaphore->lock);


//     spinlock_release(&semaphore->lock, previous_ipl);
// }
