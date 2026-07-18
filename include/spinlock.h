#ifndef SPLINTOS_SPINLOCK_H
#define SPLINTOS_SPINLOCK_H

#include "interrupts.h"

#include <stdint.h>

struct spinlock { volatile uint32_t held; };
#define SPINLOCK_INITIALIZER {0}

struct spinlock_guard { interrupt_state_t interrupt_state; };

static inline struct spinlock_guard spinlock_lock_irqsave(struct spinlock *lock)
{
    struct spinlock_guard guard = {interrupts_save_disable()};
    while (__atomic_exchange_n(&lock->held, 1U, __ATOMIC_ACQUIRE) != 0)
        arch_cpu_relax();
    return guard;
}

static inline void spinlock_unlock_irqrestore(struct spinlock *lock,
                                               struct spinlock_guard guard)
{
    __atomic_store_n(&lock->held, 0U, __ATOMIC_RELEASE);
    interrupts_restore(guard.interrupt_state);
}

#endif
