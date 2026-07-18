#include <stdint.h>

#include "arch/x86_64/platform.h"

#define PACKED __attribute__((packed))
#define ALIGNED(value) __attribute__((aligned(value)))

struct PACKED idt_entry {
    uint16_t offset_low, selector;
    uint8_t ist, attributes;
    uint16_t offset_middle;
    uint32_t offset_high, reserved;
};
struct PACKED descriptor_pointer { uint16_t limit; uint64_t base; };

extern const uintptr_t x86_64_exception_stubs[32];
static struct idt_entry idt[256] ALIGNED(16);
static volatile int breakpoint_seen;

_Static_assert(sizeof(struct idt_entry) == 16, "x86_64 IDT entry layout changed");

static void idt_set(uint8_t vector, uintptr_t handler, uint8_t ist)
{
    idt[vector] = (struct idt_entry){
        (uint16_t)handler, 0x08, ist, vector == 3 ? 0xee : 0x8e,
        (uint16_t)(handler >> 16), (uint32_t)(handler >> 32), 0
    };
}

void x86_64_idt_install_irq(uint8_t vector, uintptr_t handler)
{
    idt_set(vector, handler, 0);
}

void x86_64_idt_init(void)
{
    for (uint16_t i = 0; i < 256; ++i) idt[i] = (struct idt_entry){0};
    for (uint8_t i = 0; i < 32; ++i)
        idt_set(i, x86_64_exception_stubs[i], i == 8 ? 1 : (i == 2 ? 2 : 0));
    const struct descriptor_pointer pointer = {sizeof(idt) - 1U, (uintptr_t)idt};
    __asm__ volatile ("lidt %0" : : "m"(pointer) : "memory");
}

void x86_64_exception_dispatch(uint64_t vector, uint64_t error)
{
    (void)error;
    if (vector == 3) { breakpoint_seen = 1; return; }
    x86_64_serial_write("SplintOS: fatal x86_64 exception vector ");
    char number[4] = {(char)('0' + vector / 10U),
                      (char)('0' + vector % 10U), '\r', '\0'};
    if (vector < 10) x86_64_serial_write(number + 1);
    else x86_64_serial_write(number);
    x86_64_serial_write("\n");
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}

int x86_64_idt_breakpoint_test(void)
{
    breakpoint_seen = 0;
    __asm__ volatile ("int3");
    return breakpoint_seen != 0;
}
