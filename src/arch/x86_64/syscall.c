#include <stddef.h>
#include <stdint.h>
#include "arch/x86_64/layout.h"
#include "arch/x86_64/platform.h"
#include "arch/x86_64/syscall.h"
#include "splint/abi64.h"

#define ALIGNED(value) __attribute__((aligned(value)))
enum { EFER = 0xc0000080, STAR = 0xc0000081, LSTAR = 0xc0000082,
       FMASK = 0xc0000084, GS_BASE = 0xc0000101, KERNEL_GS_BASE = 0xc0000102,
       STACK_SIZE = 32768 };
extern void x86_64_syscall_entry(void);
static uint8_t syscall_stack[STACK_SIZE] ALIGNED(16);
static struct { uint64_t user_rsp, kernel_rsp; } kernel_gs;

static uint64_t read_msr(uint32_t msr)
{
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return (uint64_t)high << 32 | low;
}
static void write_msr(uint32_t msr, uint64_t value)
{
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"((uint32_t)value),
                      "d"((uint32_t)(value >> 32)) : "memory");
}

void x86_64_syscall_init(void)
{
    kernel_gs = (typeof(kernel_gs)){0, (uintptr_t)(syscall_stack + STACK_SIZE)};
    write_msr(GS_BASE, 0);
    write_msr(KERNEL_GS_BASE, (uintptr_t)&kernel_gs);
    write_msr(STAR, (uint64_t)0x13 << 48 | (uint64_t)0x08 << 32);
    write_msr(LSTAR, (uintptr_t)x86_64_syscall_entry);
    write_msr(FMASK, (1U << 8) | (1U << 9) | (1U << 10) | (1U << 18));
    write_msr(EFER, read_msr(EFER) | 1U);
}

void x86_64_syscall_dispatch(struct x86_64_syscall_frame *frame)
{
    if (frame == NULL) return;
    if (frame->rax == SPLINT64_SYS_ABI_QUERY)
        frame->rax = (uint64_t)SPLINT64_ABI_MAGIC << 32 | SPLINT64_ABI_VERSION;
    else frame->rax = (uint64_t)SPLINT64_ENOTSUP;
}

int x86_64_syscall_return_valid(struct x86_64_syscall_frame *frame)
{
    if (frame == NULL || !x86_64_user_range_valid(frame->user_rip, 1) ||
        frame->user_rsp < X86_64_USER_MIN || frame->user_rsp > X86_64_USER_MAX ||
        !x86_64_address_is_canonical(frame->user_rsp)) return 0;
    frame->user_rflags &= ~UINT64_C(0x0000000000077000);
    frame->user_rflags |= UINT64_C(0x202);
    return 1;
}

void x86_64_syscall_reject(void)
{
    x86_64_serial_write("SplintOS: rejected unsafe x86_64 syscall return\r\n");
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}

int x86_64_syscall_conformance_test(void)
{
    struct x86_64_syscall_frame frame = {0};
    frame.rax = SPLINT64_SYS_ABI_QUERY;
    frame.user_rip = X86_64_USER_MIN;
    frame.user_rsp = X86_64_USER_STACK_TOP;
    frame.user_rflags = UINT64_MAX;
    x86_64_syscall_dispatch(&frame);
    if (frame.rax != ((uint64_t)SPLINT64_ABI_MAGIC << 32 | SPLINT64_ABI_VERSION) ||
        !x86_64_syscall_return_valid(&frame) || (frame.user_rflags & (3U << 12)) != 0)
        return 0;
    frame.user_rip = UINT64_C(0x0000800000000000);
    if (x86_64_syscall_return_valid(&frame)) return 0;
    frame = (struct x86_64_syscall_frame){.rax = UINT64_MAX,
        .user_rip = X86_64_USER_MIN, .user_rsp = X86_64_USER_STACK_TOP,
        .user_rflags = 2};
    x86_64_syscall_dispatch(&frame);
    return (int64_t)frame.rax == SPLINT64_ENOTSUP &&
        read_msr(LSTAR) == (uintptr_t)x86_64_syscall_entry &&
        (read_msr(EFER) & 1U) != 0 && (read_msr(FMASK) & (1U << 9)) != 0 &&
        kernel_gs.kernel_rsp == (uintptr_t)(syscall_stack + STACK_SIZE);
}
