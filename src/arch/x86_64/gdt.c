#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/gdt.h"

#define PACKED __attribute__((packed))
#define ALIGNED(value) __attribute__((aligned(value)))

struct PACKED task_state_segment {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
};

struct PACKED descriptor_pointer { uint16_t limit; uint64_t base; };
enum { PRIVILEGE_STACK_SIZE = 32768, INTERRUPT_STACK_SIZE = 16384 };
static uint8_t privilege_stack[PRIVILEGE_STACK_SIZE] ALIGNED(16);
static uint8_t double_fault_stack[INTERRUPT_STACK_SIZE] ALIGNED(16);
static uint8_t nmi_stack[INTERRUPT_STACK_SIZE] ALIGNED(16);
static struct task_state_segment tss ALIGNED(16);
static uint64_t gdt[7] ALIGNED(16);

_Static_assert(sizeof(struct task_state_segment) == 104, "x86_64 TSS layout changed");

void x86_64_gdt_tss_init(void)
{
    tss = (struct task_state_segment){0};
    tss.rsp[0] = (uintptr_t)(privilege_stack + sizeof(privilege_stack));
    tss.ist[0] = (uintptr_t)(double_fault_stack + sizeof(double_fault_stack));
    tss.ist[1] = (uintptr_t)(nmi_stack + sizeof(nmi_stack));
    tss.io_map_base = sizeof(tss);
    gdt[0] = 0;
    gdt[1] = UINT64_C(0x00AF9A000000FFFF);
    gdt[2] = UINT64_C(0x00CF92000000FFFF);
    gdt[3] = UINT64_C(0x00CFF2000000FFFF);
    gdt[4] = UINT64_C(0x00AFFA000000FFFF);
    uintptr_t base = (uintptr_t)&tss;
    uint64_t limit = sizeof(tss) - 1U;
    gdt[5] = (limit & UINT64_C(0xFFFF)) |
             (uint64_t)(base & UINT64_C(0xFFFFFF)) << 16 |
             UINT64_C(0x89) << 40 |
             (limit & UINT64_C(0xF0000)) << 32 |
             (uint64_t)(base & UINT64_C(0xFF000000)) << 32;
    gdt[6] = (uint64_t)(base >> 32);
    const struct descriptor_pointer pointer = {sizeof(gdt) - 1U, (uintptr_t)gdt};
    __asm__ volatile ("lgdt %0" : : "m"(pointer) : "memory");
    __asm__ volatile ("mov $0x28, %%ax; ltr %%ax" : : : "rax", "memory");
}
