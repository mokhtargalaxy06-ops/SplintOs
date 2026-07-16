# SplintOS

SplintOS is a tiny educational x86 kernel. It boots through GRUB's Multiboot
interface, creates its own stack, enters C code, and writes directly to VGA text
memory. It also includes an RTL8139 driver and a small Ethernet stack.

![SplintOS graphical desktop](docs/images/splintos-desktop.png)

## What can SplintOS do?

SplintOS currently provides:

- Bootable 32-bit x86 kernel and ISO using GRUB Multiboot
- 800x600 graphical desktop with a software compositor and mouse pointer
- Interactive Network, Files, and About windows
- Preemptive multitasking for trusted kernel tasks
- Physical page allocation, paging, and a coalescing kernel heap
- Writable RAM filesystem with files, directories, permissions, and `/dev`
- Interactive serial command shell with file, memory, task, network, hardware,
  and identity commands
- PS/2 keyboard and mouse input plus COM1 serial diagnostics
- PCI device enumeration and ACPI RSDP/RSDT discovery
- RTL8139 Ethernet with ARP, IPv4, ICMP ping, UDP, and DHCP
- Stack-smashing protection, task identities, capabilities, and VFS permissions
- Automated ELF, Multiboot, and headless QEMU boot tests

SplintOS is still an experimental educational OS. It cannot yet run normal
Linux applications, preserve files after shutdown, use most real Wi-Fi/USB/
audio/GPU hardware, or safely isolate user applications in Ring 3. DNS, TCP,
TLS, HTTP, persistent storage, ELF user programs, and modern hardware drivers
remain future work.

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

## CPU and interrupt foundation

The kernel installs its own flat kernel/user GDT, a 256-entry IDT, handlers for
all 32 architectural CPU exceptions, and IRQ stubs for both legacy PICs. The
8259 PIC is remapped away from exception vectors, and the PIT supplies a 100 Hz
kernel tick. Keyboard and mouse input are dispatched from hardware IRQs, while
the main loop retains polling as a temporary compatibility fallback. Fatal CPU
exceptions stop the machine through a kernel panic path and report over VGA and
COM1 serial output.

## Memory management

SplintOS reads the variable-length Multiboot physical-memory map and tracks up
to 4 GiB of RAM with a 4 KiB page-frame bitmap. Kernel, low-memory, boot-info,
and memory-map pages are reserved before allocation begins. Paging is enabled
with an initial 4 GiB identity map using x86 4 MiB pages, which keeps legacy
devices and the high physical framebuffer accessible during early development.

The kernel also provides `physical_page_alloc`/`physical_page_free` and a 1 MiB
coalescing heap through `kmalloc`/`kfree`. Page-fault panic reports include EIP,
the CPU error code, and the CR2 faulting address over COM1. Per-process address
spaces and strict kernel/user isolation belong to the process milestone.

## Processes and scheduling

SplintOS has fixed-size process control blocks, separately allocated 16 KiB
kernel stacks, task creation/termination, sleeping, explicit yielding, and a
preemptive round-robin scheduler. The 100 Hz timer changes saved interrupt
frames every five ticks, producing a 20 Hz scheduling quantum. A maintenance
kernel task is created during boot to exercise context switching and sleeping.

All current tasks execute in kernel mode. Ring 3 processes, isolated page
directories, system calls, IPC, and ELF user programs are intentionally the
next sub-milestone: enabling Ring 3 against the current 4 GiB identity map would
give applications access to the entire kernel and would not be a safe design.

## Files and directories

The virtual filesystem currently mounts a writable RAM filesystem at `/`. It
supports absolute path resolution, directories, file creation, open/read/write,
append, seek, close, and directory listing through integer file descriptors.
Boot creates `/dev`, `/etc`, `/tmp`, `/etc/motd`, `/README`, `/dev/null`, and a
serial output device at `/dev/serial`.

RAM filesystem contents disappear when the machine powers off. Persistent disk
storage, a block-device cache, ATA/AHCI, and FAT32 are the next storage
sub-milestone; no on-disk writes are attempted by this version.

## Built-in applications

The scheduler runs an interactive command shell as a separate task. With QEMU,
the shell is available in the terminal attached through `-serial stdio`; basic
PS/2 keyboard characters feed the same console queue. Commands include `help`,
`echo`, `ls`, `cat`, `write`, `mem`, `tasks`, `uptime`, and `net`.
Additional commands include `devices` and `whoami`.

These commands are trusted kernel-mode built-ins, not isolated user programs.
An ELF loader and normal Ring 3 applications require per-process page tables,
a TSS, privilege-changing interrupt returns, and a validated system-call ABI.

## UDP and DHCP

The network layer maintains a small learned ARP cache, can issue ARP requests,
demultiplexes incoming UDP datagrams by local port, and provides eight bounded
UDP sockets with send/receive operations. DHCP discovery starts when RTL8139
initialization completes; offers trigger a request and acknowledgements replace
the `10.0.2.15` fallback address. The `net` shell command reports the result.

UDP payloads are currently capped at 512 bytes with one queued datagram per
socket. Routing tables, DNS, TCP, TLS, Wi-Fi, and HTTP remain later networking
milestones.

## Hardware discovery

A shared PCI subsystem enumerates all buses, slots, and functions, including
multifunction devices. It records vendor/device IDs, class/subclass, programming
interface, six BAR values, and legacy interrupt lines. Drivers can safely enable
I/O, memory decoding, and bus mastering through the common API; RTL8139 now uses
this layer instead of maintaining a private PCI scanner.

The ACPI bootstrap searches the EBDA and BIOS region for a checksummed RSDP,
validates the RSDT, and reports its table count. Run `devices` in the shell to
inspect discovered hardware. ACPI table interpretation, PCIe ECAM, xHCI USB,
AHCI/NVMe, audio, Wi-Fi, and accelerated graphics require dedicated drivers on
top of this discovery layer.

## Software compositor and windows

Graphics render into an aligned 800x600 off-screen backbuffer. Dirty rectangles
copy only changed regions into the physical framebuffer, preventing visible
tearing during pointer and widget updates. The desktop includes reusable text,
rectangle, button, title-bar, and window-frame primitives.

Network, Files, and About now open real overlay windows through Enter or mouse
clicks. Files lists the current RAM filesystem root, Network shows stack state,
and About identifies the project. Enter or the red title-bar button closes a
window. This is a software compositor; accelerated GPU commands, scalable font
rendering, image codecs, and video surfaces are later graphics milestones.

## Security foundation

All C translation units use strong compiler stack protection with a boot-time
TSC-seeded global canary; corruption stops the CPU and reports over serial.
Scheduler tasks carry inherited user IDs, and a capability layer controls
privileged identity, permission, device, and network-administration operations.

VFS nodes now store owners and Unix-style mode bits. Open, create, read, write,
device access, and `chmod` enforce those permissions; `/tmp` is intentionally
world-writable while system files remain root-owned. Security-sensitive changes
emit serial audit records, and `whoami` displays the shell task identity.

This is kernel authorization, not a complete security boundary. Ring 3 address
spaces, NX, ASLR, password authentication, cryptographic randomness, and driver
isolation require the protected user-process architecture still on the roadmap.

## Networking

The kernel currently supports interrupt-driven Ethernet receive/transmit, an
ARP cache, IPv4 validation, ICMP echo replies, UDP sockets, and DHCP. It uses
`10.0.2.15/24` as a fallback address, matching QEMU's usual user-network subnet.

Start QEMU with an RTL8139 adapter:

```sh
qemu-system-i386 -cdrom build/splintos.iso -nic user,model=rtl8139
```

The main loop retains polling as a compatibility fallback. DNS, TCP, TLS, HTTP,
and Wi-Fi are not implemented yet.

## Build

Requirements: GNU Make, GCC with 32-bit code-generation support, and GNU ld.

```sh
make
```

For a dedicated cross-compiler, set its prefix explicitly:

```sh
make CROSS=i686-elf-
```

The freestanding kernel binary is written to `build/splintos.bin`. If
`grub-file` is installed, verify its Multiboot header with:

```sh
make check
```

`make test` verifies the Multiboot header, ELF format, entry address, required
symbols, and absence of unresolved references. `make test-boot` creates the ISO,
boots it headlessly for eight seconds, and asserts the expected serial startup
milestones without a panic. `make debug` waits for GDB on TCP port 1234.

GitHub Actions runs both static and headless boot checks. See
`CONTRIBUTING.md`, `docs/coding-style.md`, and `docs/hardware-support.md` for the
development contract and current support matrix. The project is distributed
under the MIT License.

## Create and run the bootable ISO

Install GRUB utilities, xorriso, and QEMU, then run:

```sh
make iso
make run
```

The Makefile launches system QEMU through `scripts/qemu-clean.sh`, preserving
only the desktop-session variables it needs. This prevents Snap-provided GTK,
GIO, and glibc paths from injecting incompatible `core20` libraries when the
terminal was opened from a Snap-packaged IDE.

On Debian/Ubuntu, the relevant packages are commonly `gcc-multilib`,
`grub-pc-bin`, `mtools`, `xorriso`, and `qemu-system-x86`:

```sh
sudo apt-get install gcc-multilib grub-pc-bin mtools xorriso qemu-system-x86
```

## Project layout

- `src/boot.S` contains the Multiboot header and assembly entry point.
- `src/kernel.c` contains the first kernel code and VGA text driver.
- `linker.ld` lays the kernel out at the 1 MiB physical address.
- `grub/grub.cfg` defines the bootloader entry.

This is a real kernel foundation, but not yet a general-purpose operating
system. Useful next steps are a GDT/IDT, interrupt-driven networking, physical
memory management, paging, DHCP, UDP/TCP, keyboard input, and a command shell.
