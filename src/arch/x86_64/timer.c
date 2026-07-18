#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/platform.h"
#include "arch/x86_64/timer.h"
#include "arch/x86_64/input.h"
#include "arch/x86_64/network.h"

#define ALIGNED(value) __attribute__((aligned(value)))

enum { PIC1 = 0x20, PIC2 = 0xa0, PIT_HZ = 100, TASKS = 3,
       TASK_STACK_SIZE = 16384 };
extern const uintptr_t x86_64_irq_stubs[16];
static uint8_t task_stacks[2][TASK_STACK_SIZE] ALIGNED(16);
static struct x86_64_interrupt_frame *frames[TASKS];
static volatile uint64_t task_runs[2];
static uint8_t current;
static uint64_t ticks;

static inline void outb(uint16_t port, uint8_t value)
{ __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port)); }
static inline uint8_t inb(uint16_t port)
{ uint8_t value; __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port)); return value; }

static void task0(void) { for (;;) { ++task_runs[0]; __asm__ volatile ("pause"); } }
static void task1(void) { for (;;) { ++task_runs[1]; __asm__ volatile ("pause"); } }

static struct x86_64_interrupt_frame *make_frame(unsigned index, void (*entry)(void))
{
    uintptr_t top = (uintptr_t)&task_stacks[index][TASK_STACK_SIZE];
    top &= ~(uintptr_t)15;
    struct x86_64_interrupt_frame *frame =
        (struct x86_64_interrupt_frame *)(top - sizeof(*frame));
    *frame = (struct x86_64_interrupt_frame){0};
    frame->rip = (uintptr_t)entry;
    frame->cs = 0x08;
    frame->rflags = 0x202;
    frame->rsp = top;
    frame->ss = 0x10;
    return frame;
}

void x86_64_timer_scheduler_init(void)
{
    for (uint8_t irq = 0; irq < 16; ++irq)
        x86_64_idt_install_irq((uint8_t)(32 + irq), x86_64_irq_stubs[irq]);
    frames[1] = make_frame(0, task0);
    frames[2] = make_frame(1, task1);
    outb(PIC1 + 1, 0xff); outb(PIC2 + 1, 0xff);
    outb(PIC1, 0x11); outb(PIC2, 0x11);
    outb(PIC1 + 1, 0x20); outb(PIC2 + 1, 0x28);
    outb(PIC1 + 1, 4); outb(PIC2 + 1, 2);
    outb(PIC1 + 1, 1); outb(PIC2 + 1, 1);
    outb(PIC1 + 1, 0xfe); outb(PIC2 + 1, 0xff);
    uint16_t divisor = (uint16_t)(1193182U / PIT_HZ);
    outb(0x43, 0x36); outb(0x40, (uint8_t)divisor);
    outb(0x40, (uint8_t)(divisor >> 8));
    __asm__ volatile ("sti");
}

struct x86_64_interrupt_frame *x86_64_irq_dispatch(
    struct x86_64_interrupt_frame *frame)
{
    uint8_t irq = (uint8_t)(frame->vector - 32U);
    if (irq == 0) {
        frames[current] = frame;
        ++ticks;
        current = (uint8_t)((current + 1U) % TASKS);
        frame = frames[current];
    } else if (irq == 1) x86_64_ps2_keyboard_irq();
    else if (irq == 12) x86_64_ps2_mouse_irq();
    else x86_64_rtl8139_irq(irq);
    if (irq >= 8) outb(PIC2, 0x20);
    outb(PIC1, 0x20);
    return frame;
}

uint64_t x86_64_timer_ticks(void) { return ticks; }

void x86_64_pic_unmask(uint8_t irq)
{
    if (irq >= 16) return;
    uint16_t port = irq < 8 ? PIC1 + 1 : PIC2 + 1;
    uint8_t bit = irq < 8 ? irq : (uint8_t)(irq - 8U);
    outb(port, (uint8_t)(inb(port) & (uint8_t)~(1U << bit)));
    if (irq >= 8) outb(PIC1 + 1, (uint8_t)(inb(PIC1 + 1) & (uint8_t)~(1U << 2)));
}

int x86_64_timer_scheduler_test(void)
{
    uint64_t deadline = ticks + 50;
    while ((task_runs[0] == 0 || task_runs[1] == 0) && ticks < deadline)
        __asm__ volatile ("hlt");
    return task_runs[0] != 0 && task_runs[1] != 0 && frames[0] != NULL;
}
