#ifndef SPLINTOS_INTERRUPTS_H
#define SPLINTOS_INTERRUPTS_H

#include <stdint.h>
#include "arch/x86/cpu.h"

struct interrupt_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t vector, error_code;
    uint32_t eip, cs, eflags;
    uint32_t useresp, ss;
};

typedef arch_interrupt_state_t interrupt_state_t;

static inline interrupt_state_t interrupts_save_disable(void)
{
    return arch_interrupts_save_disable();
}

static inline void interrupts_restore(interrupt_state_t state)
{
    arch_interrupts_restore(state);
}

static inline void interrupts_enable(void)
{
    arch_interrupts_enable();
}

void interrupts_init(void);
struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame);
uint32_t timer_ticks(void);
void kernel_panic(const char *message, const struct interrupt_frame *frame);
void interrupts_set_kernel_stack(uintptr_t stack_top);

#define KERNEL_ASSERT(condition, message) do { \
    if (!(condition)) kernel_panic("assertion failed: " message, NULL); \
} while (0)

#endif
