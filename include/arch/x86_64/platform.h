#ifndef SPLINTOS_ARCH_X86_64_PLATFORM_H
#define SPLINTOS_ARCH_X86_64_PLATFORM_H

void x86_64_serial_write(const char *text);
void x86_64_idt_init(void);
int x86_64_idt_breakpoint_test(void);
void x86_64_idt_install_irq(uint8_t vector, uintptr_t handler);

#endif
