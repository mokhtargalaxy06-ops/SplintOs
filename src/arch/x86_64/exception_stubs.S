.section .text
.code64
.extern x86_64_exception_dispatch

.macro EXCEPTION_NO_ERROR vector
.align 16
exception_\vector:
    pushq $0
    pushq $\vector
    jmp exception_common
.endm

.macro EXCEPTION_ERROR vector
.align 16
exception_\vector:
    pushq $\vector
    jmp exception_common
.endm

EXCEPTION_NO_ERROR 0
EXCEPTION_NO_ERROR 1
EXCEPTION_NO_ERROR 2
EXCEPTION_NO_ERROR 3
EXCEPTION_NO_ERROR 4
EXCEPTION_NO_ERROR 5
EXCEPTION_NO_ERROR 6
EXCEPTION_NO_ERROR 7
EXCEPTION_ERROR 8
EXCEPTION_NO_ERROR 9
EXCEPTION_ERROR 10
EXCEPTION_ERROR 11
EXCEPTION_ERROR 12
EXCEPTION_ERROR 13
EXCEPTION_ERROR 14
EXCEPTION_NO_ERROR 15
EXCEPTION_NO_ERROR 16
EXCEPTION_ERROR 17
EXCEPTION_NO_ERROR 18
EXCEPTION_NO_ERROR 19
EXCEPTION_NO_ERROR 20
EXCEPTION_ERROR 21
EXCEPTION_NO_ERROR 22
EXCEPTION_NO_ERROR 23
EXCEPTION_NO_ERROR 24
EXCEPTION_NO_ERROR 25
EXCEPTION_NO_ERROR 26
EXCEPTION_NO_ERROR 27
EXCEPTION_NO_ERROR 28
EXCEPTION_ERROR 29
EXCEPTION_ERROR 30
EXCEPTION_NO_ERROR 31

exception_common:
    cld
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq 120(%rsp), %rdi
    movq 128(%rsp), %rsi
    movq %rsp, %rbx
    andq $-16, %rsp
    call x86_64_exception_dispatch
    movq %rbx, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax
    addq $16, %rsp
    iretq

.section .rodata
.align 8
.global x86_64_exception_stubs
x86_64_exception_stubs:
.quad exception_0, exception_1, exception_2, exception_3
.quad exception_4, exception_5, exception_6, exception_7
.quad exception_8, exception_9, exception_10, exception_11
.quad exception_12, exception_13, exception_14, exception_15
.quad exception_16, exception_17, exception_18, exception_19
.quad exception_20, exception_21, exception_22, exception_23
.quad exception_24, exception_25, exception_26, exception_27
.quad exception_28, exception_29, exception_30, exception_31

.section .note.GNU-stack,"",@progbits
