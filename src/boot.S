/* Multiboot v1 header: GRUB loads us in 32-bit protected mode. */
.set ALIGN,    1 << 0
.set MEMINFO,  1 << 1
.set VIDEO,    1 << 2
.set FLAGS,    ALIGN | MEMINFO | VIDEO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0                 /* header address; ignored without a.out-kludge flag */
.long 0                 /* load address */
.long 0                 /* load end address */
.long 0                 /* bss end address */
.long 0                 /* entry address */
.long 0                 /* linear graphics mode */
.long 800
.long 600
.long 32

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp
    xor %ebp, %ebp

    /* Seed the compiler's global stack canary before entering protected C. */
    push %eax
    push %ebx
    rdtsc
    xor %edx, %eax
    xor $0x534F5300, %eax
    mov %eax, __stack_chk_guard
    pop %ebx
    pop %eax

    /* Multiboot provides magic in EAX and its information pointer in EBX. */
    push %ebx
    push %eax
    call kernel_main

1:
    cli
    hlt
    jmp 1b
.size _start, . - _start

.section .note.GNU-stack,"",@progbits
