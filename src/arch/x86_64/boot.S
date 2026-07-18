.set MULTIBOOT_MAGIC, 0x1BADB002
.set MULTIBOOT_FLAGS, 0x00000007
.set MULTIBOOT_CHECKSUM, -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)
.set BOOTLOADER_MAGIC, 0x2BADB002
.set EFER_MSR, 0xC0000080
.set EFER_LME_NXE, (1 << 8) | (1 << 11)

.section .multiboot,"a"
.align 4
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long MULTIBOOT_CHECKSUM
.long 0
.long 800
.long 600
.long 32

.section .text
.code32
.global x86_64_start
.type x86_64_start, @function
x86_64_start:
    cli
    cmpl $BOOTLOADER_MAGIC, %eax
    jne boot_fail
    movl %ebx, multiboot_info

    /* CPUID exists only if EFLAGS.ID can be toggled. */
    pushfl
    popl %eax
    movl %eax, %ecx
    xorl $(1 << 21), %eax
    pushl %eax
    popfl
    pushfl
    popl %eax
    pushl %ecx
    popfl
    xorl %ecx, %eax
    testl $(1 << 21), %eax
    jz boot_fail

    movl $1, %eax
    cpuid
    testl $(1 << 6), %edx
    jz boot_fail

    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb boot_fail
    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx
    jz boot_fail
    testl $(1 << 20), %edx
    jz boot_fail

    /* Identity-map the first GiB with 2 MiB pages. */
    xorl %ecx, %ecx
1:
    movl %ecx, %eax
    shll $21, %eax
    orl $0x83, %eax
    movl %eax, page_directory(,%ecx,8)
    movl $0, page_directory+4(,%ecx,8)
    incl %ecx
    cmpl $512, %ecx
    jne 1b

    movl $page_directory, %eax
    orl $3, %eax
    movl %eax, pdpt_low
    movl $0, pdpt_low+4
    movl %eax, pdpt_high+510*8
    movl $0, pdpt_high+510*8+4

    movl $pdpt_low, %eax
    orl $3, %eax
    movl %eax, pml4
    movl $0, pml4+4
    movl $pdpt_high, %eax
    orl $3, %eax
    movl %eax, pml4+511*8
    movl $0, pml4+511*8+4

    movl %cr4, %eax
    orl $((1 << 5) | (1 << 9) | (1 << 10)), %eax
    movl %eax, %cr4
    movl $EFER_MSR, %ecx
    rdmsr
    orl $EFER_LME_NXE, %eax
    wrmsr
    movl $pml4, %eax
    movl %eax, %cr3
    movl %cr0, %eax
    andl $~(1 << 2), %eax
    orl $(1 << 1), %eax
    orl $0x80010000, %eax
    movl %eax, %cr0

    lgdt gdt_pointer
    ljmp $0x08, $long_mode_entry

boot_fail:
    movw $0x4f45, 0xb8000
2:  hlt
    jmp 2b

.code64
long_mode_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    xorw %ax, %ax
    movw %ax, %fs
    movw %ax, %gs
    leaq boot_stack_top(%rip), %rsp
    xorq %rbp, %rbp
    movl multiboot_info(%rip), %edi
    call x86_64_kernel_main
3:  hlt
    jmp 3b

.align 8
gdt:
    .quad 0
    .quad 0x00af9a000000ffff
    .quad 0x00cf92000000ffff
gdt_end:
gdt_pointer:
    .word gdt_end - gdt - 1
    .long gdt

.section .bss
.align 4096
pml4: .skip 4096
pdpt_low: .skip 4096
pdpt_high: .skip 4096
page_directory: .skip 4096
.align 16
boot_stack: .skip 32768
boot_stack_top:
multiboot_info: .long 0

.section .note.GNU-stack,"",@progbits
