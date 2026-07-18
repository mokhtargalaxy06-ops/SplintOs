OUTPUT_FORMAT(elf64-x86-64)
ENTRY(x86_64_start)

SECTIONS
{
    . = 1M;
    x86_64_kernel_start = .;
    .multiboot : { KEEP(*(.multiboot)) }
    .text ALIGN(4K) : { *(.text) }
    .rodata ALIGN(4K) : { *(.rodata) }
    .data ALIGN(4K) : { *(.data) }
    .bss ALIGN(4K) : { *(COMMON) *(.bss) }
    x86_64_kernel_end = ALIGN(4K);
}
