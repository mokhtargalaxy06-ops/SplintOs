# SplintOS

SplintOS is a tiny educational x86 kernel. It boots through GRUB's Multiboot
interface, creates its own stack, enters C code, and writes directly to VGA text
memory. It also includes a polling RTL8139 driver and a small Ethernet stack.

## Graphical desktop

GRUB requests an 800x600 32-bit linear framebuffer. The kernel draws its own
desktop UI without an external graphics library. The dashboard shows kernel,
network, and graphics status and provides selectable Network, Files, and About
buttons. Use Left/Right or Tab to move between buttons. VGA text mode remains a
fallback if the boot environment cannot provide a compatible framebuffer.

The UI accepts PS/2 keyboard navigation and PS/2 mouse movement/clicks. The
kernel also initializes COM1 as a serial diagnostics console; `make run` maps it
to the launching terminal. The buttons are currently navigation elements only.

## Device support

Current I/O support is deliberately hardware-specific: VGA/framebuffer video,
PS/2 keyboard, PS/2 mouse, COM1 serial, PCI configuration-space discovery, and
RTL8139 Ethernet. Supporting "all" modern devices requires independent USB,
PCIe, ACPI, storage, audio, GPU, Bluetooth, and vendor-driver projects. The
device input layer in `include/devices.h` is the starting boundary for adding
those drivers without coupling them directly to the desktop.

## Networking

The kernel currently supports Ethernet receive/transmit, ARP replies, IPv4
validation, and ICMP echo replies (ping). Its address is statically configured
as `10.0.2.15/24`, matching QEMU's usual user-network subnet.

Start QEMU with an RTL8139 adapter:

```sh
qemu-system-i386 -cdrom build/splintos.iso -nic user,model=rtl8139
```

Networking is polling-based because interrupt handling has not been added yet.
There is no DHCP, DNS, UDP, TCP, or sockets API at this stage.

## Build

Requirements: GNU Make, GCC with 32-bit code-generation support, and GNU ld.

```sh
make
```

The freestanding kernel binary is written to `build/splintos.bin`. If
`grub-file` is installed, verify its Multiboot header with:

```sh
make check
```

## Create and run the bootable ISO

Install GRUB utilities, xorriso, and QEMU, then run:

```sh
make iso
make run
```

On Debian/Ubuntu, the relevant packages are commonly `gcc-multilib`,
`grub-pc-bin`, `xorriso`, and `qemu-system-x86`.

## Project layout

- `src/boot.S` contains the Multiboot header and assembly entry point.
- `src/kernel.c` contains the first kernel code and VGA text driver.
- `linker.ld` lays the kernel out at the 1 MiB physical address.
- `grub/grub.cfg` defines the bootloader entry.

This is a real kernel foundation, but not yet a general-purpose operating
system. Useful next steps are a GDT/IDT, interrupt-driven networking, physical
memory management, paging, DHCP, UDP/TCP, keyboard input, and a command shell.
