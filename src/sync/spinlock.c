#include <stdbool.h>
#include <event/event.h>
#include "spinlock.h"

void spinlock_acquire_high_tpl(spinlock_t* lock) {
    lock->owner_tpl = raise_tpl(TPL_HIGH_LEVEL);
    while(!atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire)) {
        // restore it if failed and raise it again, this makes so
        // if we hit highest priority level it will enable back
        // interrupts and then lower them again
        restore_tpl(lock->owner_tpl);
        raise_tpl(TPL_HIGH_LEVEL);
    }
}

void spinlock_acquire(spinlock_t* lock) {
    lock->owner_tpl = get_tpl();
    while(!atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire));
}

void spinlock_release(spinlock_t* lock) {
    if (lock->flag._Value) {
        atomic_flag_clear_explicit(&lock->flag, memory_order_release);
        restore_tpl(lock->owner_tpl);
    }
}
