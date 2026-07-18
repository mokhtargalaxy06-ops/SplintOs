#ifndef SPLINTOS_ARCH_X86_CPU_H
#define SPLINTOS_ARCH_X86_CPU_H

#include <stdint.h>

typedef uint32_t arch_interrupt_state_t;

static inline arch_interrupt_state_t arch_interrupts_save_disable(void)
{
    arch_interrupt_state_t state;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(state) : : "memory");
    return state;
}

static inline void arch_interrupts_restore(arch_interrupt_state_t state)
{
    if ((state & (1U << 9)) != 0)
        __asm__ volatile ("sti" : : : "memory");
    else
        __asm__ volatile ("" : : : "memory");
}

static inline void arch_interrupts_enable(void)
{ __asm__ volatile ("sti" : : : "memory"); }
static inline void arch_interrupts_disable(void)
{ __asm__ volatile ("cli" : : : "memory"); }
static inline void arch_cpu_halt(void) { __asm__ volatile ("hlt"); }
static inline void arch_cpu_relax(void) { __asm__ volatile ("pause"); }
static inline void arch_compiler_barrier(void)
{ __asm__ volatile ("" : : : "memory"); }

static inline void arch_enable_paging(uint32_t page_directory)
{
    __asm__ volatile (
        "mov %%cr4, %%eax\n"
        "or $0x10, %%eax\n"
        "mov %%eax, %%cr4\n"
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0"
        : : "r"(page_directory) : "eax", "memory");
}

static inline void arch_load_page_directory(uint32_t page_directory)
{ __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory) : "memory"); }
static inline void arch_invalidate_page(const void *address)
{ __asm__ volatile ("invlpg (%0)" : : "r"(address) : "memory"); }
static inline uint32_t arch_fault_address(void)
{ uint32_t value; __asm__ volatile ("mov %%cr2, %0" : "=r"(value)); return value; }
static inline void arch_load_task_register(uint16_t selector)
{ __asm__ volatile ("ltr %%ax" : : "a"(selector)); }
static inline void arch_request_reschedule(void)
{ __asm__ volatile ("int $32"); }

static inline uint16_t arch_physical_read16(uintptr_t address)
{
    uint16_t value;
    __asm__ volatile ("movw (%1), %0" : "=r"(value) : "r"(address) : "memory");
    return value;
}

static inline uintptr_t arch_frame_pointer(void)
{
    uintptr_t value;
    __asm__ volatile ("mov %%ebp, %0" : "=r"(value));
    return value;
}

#endif
