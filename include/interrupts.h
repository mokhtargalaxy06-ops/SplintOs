#ifndef SPLINTOS_INTERRUPTS_H
#define SPLINTOS_INTERRUPTS_H

#include <stdint.h>

struct interrupt_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t vector, error_code;
    uint32_t eip, cs, eflags;
    uint32_t useresp, ss;
};

void interrupts_init(void);
struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame);
uint32_t timer_ticks(void);
void kernel_panic(const char *message, const struct interrupt_frame *frame);
void interrupts_set_kernel_stack(uintptr_t stack_top);

#endif
