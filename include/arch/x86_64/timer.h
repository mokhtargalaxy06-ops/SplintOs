#ifndef SPLINTOS_ARCH_X86_64_TIMER_H
#define SPLINTOS_ARCH_X86_64_TIMER_H

#include <stdint.h>

struct x86_64_interrupt_frame {
    uint64_t r15, r14, r13, r12, rbx, rbp;
    uint64_t r11, r10, r9, r8, rdi, rsi, rdx, rcx, rax;
    uint64_t vector, error, rip, cs, rflags, rsp, ss;
};

void x86_64_timer_scheduler_init(void);
int x86_64_timer_scheduler_test(void);
uint64_t x86_64_timer_ticks(void);
void x86_64_pic_unmask(uint8_t irq);
struct x86_64_interrupt_frame *x86_64_irq_dispatch(
    struct x86_64_interrupt_frame *frame);

#endif
