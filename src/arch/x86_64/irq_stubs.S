.section .text
.code64
.extern x86_64_irq_dispatch

.macro IRQ_STUB number
.align 16
irq_\number:
    pushq $0
    pushq $(32 + \number)
    jmp irq_common
.endm

IRQ_STUB 0
IRQ_STUB 1
IRQ_STUB 2
IRQ_STUB 3
IRQ_STUB 4
IRQ_STUB 5
IRQ_STUB 6
IRQ_STUB 7
IRQ_STUB 8
IRQ_STUB 9
IRQ_STUB 10
IRQ_STUB 11
IRQ_STUB 12
IRQ_STUB 13
IRQ_STUB 14
IRQ_STUB 15

irq_common:
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
    movq %rsp, %rdi
    movq %rsp, %rbx
    andq $-16, %rsp
    call x86_64_irq_dispatch
    movq %rax, %rsp
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
.global x86_64_irq_stubs
x86_64_irq_stubs:
.quad irq_0, irq_1, irq_2, irq_3, irq_4, irq_5, irq_6, irq_7
.quad irq_8, irq_9, irq_10, irq_11, irq_12, irq_13, irq_14, irq_15

.section .note.GNU-stack,"",@progbits
