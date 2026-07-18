#ifndef SPLINTOS_ARCH_X86_64_LAYOUT_H
#define SPLINTOS_ARCH_X86_64_LAYOUT_H

#include <stdint.h>

/* Initial 48-bit canonical map; LA57 is deliberately outside the first port. */
#define X86_64_USER_MIN UINT64_C(0x0000000000400000)
#define X86_64_USER_MAX UINT64_C(0x00007FFFFFFFFFFF)
#define X86_64_USER_STACK_TOP UINT64_C(0x00007FFFFFF00000)
#define X86_64_DIRECT_MAP_BASE UINT64_C(0xFFFF800000000000)
#define X86_64_KERNEL_BASE UINT64_C(0xFFFFFFFF80000000)
#define X86_64_RECURSIVE_PT_BASE UINT64_C(0xFFFFFF0000000000)

#define X86_64_PAGE_SIZE UINT64_C(4096)
#define X86_64_LARGE_PAGE_SIZE UINT64_C(0x200000)
#define X86_64_KERNEL_STACK_SIZE UINT64_C(0x8000)
#define X86_64_KERNEL_STACK_GUARD X86_64_PAGE_SIZE

static inline int x86_64_address_is_canonical(uint64_t address)
{
    return address <= UINT64_C(0x00007FFFFFFFFFFF) ||
           address >= UINT64_C(0xFFFF800000000000);
}

static inline int x86_64_user_range_valid(uint64_t address, uint64_t length)
{
    return address >= X86_64_USER_MIN && length <= X86_64_USER_MAX - address &&
           x86_64_address_is_canonical(address + length);
}

_Static_assert((X86_64_KERNEL_BASE & (X86_64_LARGE_PAGE_SIZE - 1)) == 0,
               "kernel base must be large-page aligned");
_Static_assert((X86_64_USER_STACK_TOP & (X86_64_PAGE_SIZE - 1)) == 0,
               "user stack top must be page aligned");
_Static_assert(X86_64_USER_STACK_TOP <= X86_64_USER_MAX,
               "user stack must remain in the lower canonical half");
_Static_assert(X86_64_DIRECT_MAP_BASE > X86_64_USER_MAX,
               "direct map must remain supervisor-only");
_Static_assert(X86_64_KERNEL_BASE > X86_64_DIRECT_MAP_BASE,
               "kernel image and direct-map regions must be ordered");

#endif
