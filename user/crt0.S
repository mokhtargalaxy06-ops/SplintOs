.section .text
.global _start
.extern main
.extern sys_exit
.type _start, @function
_start:
    mov (%esp), %eax
    lea 4(%esp), %edx
    push %edx
    push %eax
    call main
    add $8, %esp
    push %eax
    call sys_exit
1:
    jmp 1b
.size _start, . - _start

.section .note.GNU-stack,"",@progbits
