#ifndef SPLINTOS_ARCH_X86_64_SYSCALL_H
#define SPLINTOS_ARCH_X86_64_SYSCALL_H
#include <stdint.h>
struct x86_64_syscall_frame {
    uint64_t rax, rdi, rsi, rdx, r10, r8, r9;
    uint64_t user_rsp, user_rip, user_rflags;
};
void x86_64_syscall_init(void);
int x86_64_syscall_conformance_test(void);
void x86_64_syscall_dispatch(struct x86_64_syscall_frame *frame);
int x86_64_syscall_return_valid(struct x86_64_syscall_frame *frame);
void x86_64_syscall_reject(void) __attribute__((noreturn));
#endif
