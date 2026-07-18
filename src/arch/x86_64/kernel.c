#include <stdint.h>

#include "arch/x86_64/layout.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/platform.h"
#include "arch/x86_64/paging.h"
#include "arch/x86_64/physical.h"
#include "arch/x86_64/heap.h"
#include "arch/x86_64/timer.h"
#include "arch/x86_64/hardware.h"
#include "arch/x86_64/block.h"
#include "arch/x86_64/input.h"
#include "arch/x86_64/vfs.h"
#include "arch/x86_64/network.h"
#include "arch/x86_64/graphics.h"
#include "arch/x86_64/virtio_block.h"
#include "arch/x86_64/dma.h"
#include "arch/x86_64/abi.h"
#include "arch/x86_64/syscall.h"
#include "arch/x86_64/usercopy.h"

static inline void serial_out(uint16_t port, uint8_t value)
{ __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port)); }

static inline uint8_t serial_in(uint16_t port)
{ uint8_t value; __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port)); return value; }

static void serial_init(void)
{
    enum { COM1 = 0x3f8 };
    serial_out(COM1 + 1, 0);
    serial_out(COM1 + 3, 0x80);
    serial_out(COM1, 3);
    serial_out(COM1 + 1, 0);
    serial_out(COM1 + 3, 3);
    serial_out(COM1 + 2, 0xc7);
    serial_out(COM1 + 4, 0x0b);
}

void x86_64_serial_write(const char *text)
{
    enum { COM1 = 0x3f8 };
    while (*text != '\0') {
        while ((serial_in(COM1 + 5) & 0x20U) == 0) {}
        serial_out(COM1, (uint8_t)*text++);
    }
}

void x86_64_kernel_main(uint32_t multiboot_info)
{
    (void)multiboot_info;
    serial_init();
    x86_64_serial_write("SplintOS: x86_64 CPUID, PAE, long mode and NX verified\r\n");
    x86_64_serial_write("SplintOS: x86_64 temporary paging and trampoline online\r\n");
    x86_64_gdt_tss_init();
    x86_64_serial_write("SplintOS: x86_64 GDT, TSS, RSP0 and IST stacks online\r\n");
    x86_64_idt_init();
    x86_64_serial_write(x86_64_idt_breakpoint_test()
        ? "SplintOS: x86_64 IDT and exception return online\r\n"
        : "SplintOS: x86_64 IDT breakpoint failure\r\n");
    x86_64_serial_write(x86_64_paging_init()
        ? "SplintOS: x86_64 four-level paging owner online\r\n"
        : "SplintOS: x86_64 paging validation failure\r\n");
    x86_64_serial_write(x86_64_physical_init(multiboot_info)
        ? "SplintOS: x86_64 physical allocator ownership online\r\n"
        : "SplintOS: x86_64 physical allocator failure\r\n");
    x86_64_serial_write(x86_64_heap_init()
        ? "SplintOS: x86_64 kernel heap exhaustion and reuse online\r\n"
        : "SplintOS: x86_64 kernel heap failure\r\n");
    x86_64_serial_write(x86_64_address_is_canonical(X86_64_KERNEL_BASE)
        ? "SplintOS: x86_64 canonical layout online\r\n"
        : "SplintOS: x86_64 layout failure\r\n");
    x86_64_serial_write(x86_64_abi_conformance_test()
        ? "SplintOS: x86_64 syscall ABI v1 contract online\r\n"
        : "SplintOS: x86_64 syscall ABI contract failure\r\n");
    x86_64_syscall_init();
    x86_64_serial_write(x86_64_syscall_conformance_test()
        ? "SplintOS: x86_64 syscall entry and safe return online\r\n"
        : "SplintOS: x86_64 syscall entry failure\r\n");
    x86_64_serial_write(x86_64_usercopy_conformance_test()
        ? "SplintOS: x86_64 hostile user pointers rejected\r\n"
        : "SplintOS: x86_64 user-copy validation failure\r\n");
    x86_64_timer_scheduler_init();
    x86_64_serial_write(x86_64_timer_scheduler_test()
        ? "SplintOS: x86_64 PIC, PIT and preemptive scheduler online\r\n"
        : "SplintOS: x86_64 timer scheduler failure\r\n");
    x86_64_serial_write(x86_64_hardware_init()
        ? "SplintOS: x86_64 PCI and ACPI discovery online\r\n"
        : "SplintOS: x86_64 PCI or ACPI discovery failure\r\n");
    x86_64_serial_write(x86_64_block_conformance_test()
        ? "SplintOS: x86_64 bounded block layer online\r\n"
        : "SplintOS: x86_64 block layer failure\r\n");
    x86_64_serial_write(x86_64_dma_conformance_test()
        ? "SplintOS: x86_64 low DMA and bounce buffers online\r\n"
        : "SplintOS: x86_64 DMA helper failure\r\n");
    x86_64_serial_write(x86_64_input_conformance_test()
        ? "SplintOS: x86_64 bounded input queue online\r\n"
        : "SplintOS: x86_64 input queue failure\r\n");
    x86_64_serial_write(x86_64_ps2_keyboard_init()
        ? "SplintOS: x86_64 PS/2 keyboard IRQ online\r\n"
        : "SplintOS: x86_64 PS/2 keyboard failure\r\n");
    x86_64_serial_write(x86_64_ps2_mouse_init()
        ? "SplintOS: x86_64 PS/2 mouse IRQ online\r\n"
        : "SplintOS: x86_64 PS/2 mouse failure\r\n");
    x86_64_serial_write(x86_64_vfs_conformance_test()
        ? "SplintOS: x86_64 bounded VFS online\r\n"
        : "SplintOS: x86_64 VFS failure\r\n");
    x86_64_serial_write(x86_64_network_conformance_test()
        ? "SplintOS: x86_64 bounded UDP core online\r\n"
        : "SplintOS: x86_64 network core failure\r\n");
    if (x86_64_rtl8139_init()) {
        x86_64_serial_write("SplintOS: x86_64 RTL8139 RX, TX DMA and IRQ online\r\n");
        if (x86_64_dhcp_configure())
            x86_64_serial_write("SplintOS: x86_64 RTL8139 DHCP configured\r\n");
    }
    if (x86_64_virtio_block_init() && x86_64_virtio_block_conformance_test()) {
        x86_64_serial_write("SplintOS: x86_64 persistent VirtIO block online\r\n");
        int mount = x86_64_vfs_mount_virtio();
        if (mount == 1)
            x86_64_serial_write("SplintOS: x86_64 persistent VFS created\r\n");
        else if (mount == 2)
            x86_64_serial_write("SplintOS: x86_64 persistent VFS recovered\r\n");
    }
    x86_64_serial_write(x86_64_graphics_init(multiboot_info) &&
                        x86_64_graphics_conformance_test()
        ? "SplintOS: x86_64 framebuffer compositor online\r\n"
        : "SplintOS: x86_64 framebuffer compositor failure\r\n");
}
