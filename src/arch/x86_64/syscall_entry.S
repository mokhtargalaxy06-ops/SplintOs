.section .text
.code64
.global x86_64_syscall_entry
.extern x86_64_syscall_dispatch
.extern x86_64_syscall_return_valid
.extern x86_64_syscall_reject
x86_64_syscall_entry:
    swapgs
    movq %rsp, %gs:0
    movq %gs:8, %rsp
    pushq %r11
    pushq %rcx
    movq %gs:0, %rcx
    pushq %rcx
    pushq %r9
    pushq %r8
    pushq %r10
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rax
    movq %rsp, %rdi
    call x86_64_syscall_dispatch
    movq %rsp, %rdi
    call x86_64_syscall_return_valid
    testl %eax, %eax
    jz x86_64_syscall_reject
    popq %rax
    popq %rdi
    popq %rsi
    popq %rdx
    popq %r10
    popq %r8
    popq %r9
    movq 0(%rsp), %rdx
    movq 8(%rsp), %rcx
    movq 16(%rsp), %r11
    movq %rdx, %rsp
    swapgs
    sysretq

.section .note.GNU-stack,"",@progbits
