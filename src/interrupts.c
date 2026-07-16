#include "interrupts.h"

#include "devices.h"
#include "kernel.h"
#include "network.h"
#include "scheduler.h"
#include "syscall.h"

#include <stddef.h>
#include <stdint.h>

#define PACKED __attribute__((packed))

struct PACKED descriptor_pointer {
    uint16_t limit;
    uint32_t base;
};

struct PACKED idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t attributes;
    uint16_t offset_high;
};

struct PACKED task_state_segment {
    uint32_t previous, esp0, ss0, esp1, ss1, esp2, ss2, cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, iomap;
};

static uint64_t gdt[6];
static struct task_state_segment tss;
static struct idt_entry idt[256];
static volatile uint32_t ticks;

extern void gdt_flush(const struct descriptor_pointer *pointer);
extern void idt_load(const struct descriptor_pointer *pointer);

#define DECLARE_ISR(n) extern void isr##n(void)
DECLARE_ISR(0); DECLARE_ISR(1); DECLARE_ISR(2); DECLARE_ISR(3);
DECLARE_ISR(4); DECLARE_ISR(5); DECLARE_ISR(6); DECLARE_ISR(7);
DECLARE_ISR(8); DECLARE_ISR(9); DECLARE_ISR(10); DECLARE_ISR(11);
DECLARE_ISR(12); DECLARE_ISR(13); DECLARE_ISR(14); DECLARE_ISR(15);
DECLARE_ISR(16); DECLARE_ISR(17); DECLARE_ISR(18); DECLARE_ISR(19);
DECLARE_ISR(20); DECLARE_ISR(21); DECLARE_ISR(22); DECLARE_ISR(23);
DECLARE_ISR(24); DECLARE_ISR(25); DECLARE_ISR(26); DECLARE_ISR(27);
DECLARE_ISR(28); DECLARE_ISR(29); DECLARE_ISR(30); DECLARE_ISR(31);
DECLARE_ISR(32); DECLARE_ISR(33); DECLARE_ISR(34); DECLARE_ISR(35);
DECLARE_ISR(36); DECLARE_ISR(37); DECLARE_ISR(38); DECLARE_ISR(39);
DECLARE_ISR(40); DECLARE_ISR(41); DECLARE_ISR(42); DECLARE_ISR(43);
DECLARE_ISR(44); DECLARE_ISR(45); DECLARE_ISR(46); DECLARE_ISR(47);
DECLARE_ISR(128);

static void (*const stubs[48])(void) = {
    isr0,isr1,isr2,isr3,isr4,isr5,isr6,isr7,
    isr8,isr9,isr10,isr11,isr12,isr13,isr14,isr15,
    isr16,isr17,isr18,isr19,isr20,isr21,isr22,isr23,
    isr24,isr25,isr26,isr27,isr28,isr29,isr30,isr31,
    isr32,isr33,isr34,isr35,isr36,isr37,isr38,isr39,
    isr40,isr41,isr42,isr43,isr44,isr45,isr46,isr47,
};

static const char *const exception_names[32] = {
    "Divide by zero", "Debug", "Non-maskable interrupt", "Breakpoint",
    "Overflow", "Bound range", "Invalid opcode", "Device unavailable",
    "Double fault", "Coprocessor overrun", "Invalid TSS", "Segment absent",
    "Stack fault", "General protection fault", "Page fault", "Reserved",
    "Floating-point exception", "Alignment check", "Machine check", "SIMD exception",
    "Virtualization exception", "Control protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Hypervisor injection",
    "VMM communication", "Security exception", "Reserved",
};

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void serial_hex(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char output[11] = "0x00000000";
    for (int i = 9; i >= 2; --i) {
        output[i] = digits[value & 0x0F];
        value >>= 4;
    }
    serial_write(output);
}

static uint64_t gdt_entry(uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
{
    return (limit & 0xFFFFU) | ((uint64_t)(base & 0xFFFFFFU) << 16) |
           ((uint64_t)access << 40) | ((uint64_t)((limit >> 16) & 0x0FU) << 48) |
           ((uint64_t)(flags & 0x0FU) << 52) | ((uint64_t)(base >> 24) << 56);
}

static void set_gate(uint8_t vector, void (*handler)(void), uint8_t attributes)
{
    uintptr_t address = (uintptr_t)handler;
    idt[vector].offset_low = (uint16_t)address;
    idt[vector].selector = 0x08;
    idt[vector].zero = 0;
    idt[vector].attributes = attributes;
    idt[vector].offset_high = (uint16_t)(address >> 16);
}

static void pic_remap(void)
{
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    /* Enable legacy IRQ lines; individual drivers acknowledge their devices. */
    outb(0x21, 0x00);
    outb(0xA1, 0x00);
}

static void pit_init(void)
{
    uint16_t divisor = 1193182U / 100U;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)divisor);
    outb(0x40, (uint8_t)(divisor >> 8));
}

void interrupts_init(void)
{
    __asm__ volatile ("cli");
    gdt[0] = 0;
    gdt[1] = gdt_entry(0, 0xFFFFF, 0x9A, 0x0C);
    gdt[2] = gdt_entry(0, 0xFFFFF, 0x92, 0x0C);
    gdt[3] = gdt_entry(0, 0xFFFFF, 0xFA, 0x0C);
    gdt[4] = gdt_entry(0, 0xFFFFF, 0xF2, 0x0C);
    tss = (struct task_state_segment){0};
    tss.ss0 = 0x10;
    tss.iomap = sizeof(tss);
    gdt[5] = gdt_entry((uint32_t)(uintptr_t)&tss, sizeof(tss) - 1, 0x89, 0x00);
    struct descriptor_pointer gdtr = {sizeof(gdt) - 1, (uint32_t)(uintptr_t)gdt};
    gdt_flush(&gdtr);
    __asm__ volatile ("ltr %%ax" : : "a"(0x28));

    for (uint16_t i = 0; i < 256; ++i) idt[i] = (struct idt_entry){0};
    for (uint8_t i = 0; i < 48; ++i) set_gate(i, stubs[i], i == 3 ? 0xEE : 0x8E);
    set_gate(128, isr128, 0xEE);
    struct descriptor_pointer idtr = {sizeof(idt) - 1, (uint32_t)(uintptr_t)idt};
    idt_load(&idtr);
    pic_remap();
    pit_init();
    serial_write("SplintOS: GDT, TSS, IDT, PIC and PIT initialized\r\n");
    __asm__ volatile ("sti");
}

struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame)
{
    if (frame->vector == 128) return syscall_dispatch(frame);
    if (frame->vector < 32) {
        if ((frame->cs & 3U) == 3U) {
            serial_write("SplintOS: terminated faulting user process\r\n");
            return scheduler_fault(frame);
        }
        kernel_panic(exception_names[frame->vector], frame);
    }
    uint32_t irq = frame->vector - 32;
    if (irq == 0) ++ticks;
    else if (irq == 1 || irq == 4 || irq == 12) devices_poll();
    else network_interrupt();
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
    return irq == 0 ? scheduler_tick(frame) : frame;
}

void interrupts_set_kernel_stack(uintptr_t stack_top)
{
    tss.esp0 = (uint32_t)stack_top;
}

uint32_t timer_ticks(void)
{
    return ticks;
}

void kernel_panic(const char *message, const struct interrupt_frame *frame)
{
    __asm__ volatile ("cli");
    serial_write("\r\nKERNEL PANIC: ");
    serial_write(message);
    serial_write("\r\nEIP=");
    serial_hex(frame->eip);
    serial_write(" ERROR=");
    serial_hex(frame->error_code);
    if (frame->vector == 14) {
        uint32_t fault_address;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));
        serial_write(" CR2=");
        serial_hex(fault_address);
    }
    serial_write("\r\nCPU halted.\r\n");
    terminal_write("\nKERNEL PANIC: ");
    terminal_write(message);
    terminal_write("\nCPU halted.\n");
    for (;;) __asm__ volatile ("hlt");
}
