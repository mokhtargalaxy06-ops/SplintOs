# SplintOS

SplintOS is a tiny educational x86 kernel. It boots through GRUB's Multiboot
interface, creates its own stack, enters C code, and writes directly to VGA text
memory. It also includes an RTL8139 driver and a small Ethernet stack.

SplintOS has an independent kernel written from scratch in C and x86 assembly.
GRUB starts it, but it does not use:

- The Linux kernel
- GNU/Linux userland
- Linux drivers
- Linux applications or package managers

![SplintOS graphical desktop](docs/images/splintos-desktop.png)

## What can SplintOS do?

SplintOS currently provides:

- Bootable 32-bit x86 kernel and ISO using GRUB Multiboot
- 800x600 graphical desktop with a software compositor and mouse pointer
- Interactive Network, Files, and About windows
- Preemptive multitasking plus isolated Ring 3 ELF32 processes
- Ring 3 voluntary yield and timer-backed sleep syscalls
- Explicit-frequency Ring 3 monotonic clock
- Validated read-only CMOS wall-clock snapshots for Ring 3
- Fixed-layout Ring 3 system and machine identity
- Physical page allocation, paging, and a coalescing kernel heap
- Writable RAM filesystem with files, directories, permissions, and `/dev`
- Ring 3 checked creation and removal of empty RAMFS directories
- Ownership-checked Ring 3 permission changes
- Read-only Ring 3 process identity lookup
- Generic block devices, a bounded write-back cache, and checked MBR partitions
- Deterministic block-write fault injection for storage error-path tests
- Boot-time proof that interrupted filesystem metadata commits fail closed
- A small educational disk filesystem mounted through the VFS at `/disk`
- Legacy VirtIO block support with an automated two-boot persistence test
- Interactive serial command shell with file, memory, task, network, hardware,
  and identity commands
- Defensive ELF32 loader with separately built `/bin/hello` and `/bin/cat`
  Ring 3 programs
- Checked user file syscalls backed by process-owned descriptor tables
- Shared-offset descriptor seeking with bounds checks
- Checked path metadata lookup using fixed-width VFS records
- Writable descriptor truncation with zero-filled persistent growth
- Bounded argument stacks plus `spawn` and blocking `wait`
- Ring 3 command shell with event-driven keyboard and serial input
- Reference-counted open files, inherited standard streams, and `dup2`
- Separate Ring 3 `/bin/echo` and descriptor-lifecycle test programs
- Bounded blocking pipes with EOF and peer-close wake-up behavior
- Bounded multi-descriptor polling with timer deadlines
- Atomic child descriptor actions, shell redirection, and two-process pipelines
- Ring 3 `wc`, `ls`, `mem`, `uptime`, and `ps` commands
- Page-backed userspace `brk` with reusable `malloc`/`free`/`calloc`/`realloc`
- Selectable trusted recovery console from the GRUB menu
- PS/2 keyboard and mouse input plus COM1 serial diagnostics
- A bounded 4 KiB in-memory boot and diagnostic log
- Checked Ring 3 access to bounded diagnostic-log snapshots
- PCI device enumeration and ACPI RSDP/RSDT discovery
- RTL8139 Ethernet with ARP, IPv4, ICMP ping, UDP, and DHCP
- Capability-checked descriptor-backed Ring 3 UDP send/receive and polling
- Stack-smashing protection, task identities, capabilities, and VFS permissions
- Automated ELF, Multiboot, and headless QEMU boot tests

SplintOS is still an experimental educational OS. It cannot run Linux
applications or use most real Wi-Fi, USB,
audio, and GPU hardware. Its Ring 3 ELF32 environment remains intentionally
small: pipelines are simple, and the C library is
minimal. TCP, TLS, HTTP, additional filesystems, and modern hardware drivers
remain future work; bounded DNS and persistent VirtIO storage are available.

## Graphical desktop

GRUB requests an 800x600 32-bit linear framebuffer. The kernel draws its own
desktop UI without an external graphics library. The dashboard shows kernel,
network, and graphics status and provides selectable Network, Files, and About
buttons. Use Left/Right or Tab to move between buttons. VGA text mode remains a
fallback if the boot environment cannot provide a compatible framebuffer.

After opening Files, use Up/Down to move the highlighted selection through the
current directory. Press Enter to open the selected directory and Left to
return to its parent. The current absolute path is shown above the listing;
selection wraps between the first and last visible entries.

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
the CPU error code, and the CR2 faulting address over COM1. User processes have
private address spaces and can grow a zeroed heap between `0x80000000` and
`0x90000000` through `brk`.

## Processes and scheduling

SplintOS has fixed-size process control blocks, separately allocated 16 KiB
kernel stacks, task creation/termination, sleeping, explicit yielding, and a
preemptive round-robin scheduler. The 100 Hz timer changes saved interrupt
frames every five ticks, producing a 20 Hz scheduling quantum. A maintenance
kernel task is created during boot to exercise context switching and sleeping.

Kernel services still use trusted Ring 0 tasks, while ELF32 applications run in
private page directories with supervisor-only kernel mappings. The TSS provides
kernel-entry stacks. Parent tracking, argument stacks, `spawn`, blocking `wait`,
descriptor inheritance, and bounded pipes support the Ring 3 shell and commands.

## Files and directories

The virtual filesystem currently mounts a writable RAM filesystem at `/`. It
supports absolute path resolution, directories, file creation, open/read/write,
append, seek, close, and directory listing through integer file descriptors.
Boot creates `/dev`, `/etc`, `/tmp`, `/etc/motd`, `/README`, `/dev/null`, and a
serial output device at `/dev/serial`.

RAM filesystem contents disappear when the machine powers off. A generic block
layer, 16-sector write-back cache, checked MBR parser, and compact `SPLFS4`
educational filesystem now run over `ramblk0`. `SPLFS4` is mounted at `/disk`;
Ring 3 programs use normal file descriptors and can explicitly flush dirty
blocks. The versioned `SPLFS4` layout stores eight flat files of at most 4 KiB
each in dynamically placed contiguous extents. A checked allocation bitmap,
metadata checksums, and clean/dirty transaction state detect allocation drift
and interrupted metadata commits. A dirty image whose previous checksummed
directory remains intact can be inspected through a strictly read-only mount.
A legacy VirtIO block
driver provides persistent storage under QEMU; `ramblk0` remains the fallback
and deterministic conformance device. Unknown hardware partitions are probed
read-only and never formatted automatically. Checked `unlink` removes closed
files and makes their directory slots and extents reusable; same-directory `rename` preserves
the source extent and can atomically replace a closed destination through one
flushed metadata-sector update.

The storage integration suite also boots deterministic images containing an
unknown filesystem, overlapping extents, and an out-of-range extent. Each must
be rejected, and a full-image SHA-256 comparison proves the probe wrote nothing.

## Built-in applications

The default interactive shell is `/bin/sh`, available in the QEMU terminal
through `-serial stdio`; PS/2 input feeds the same blocking console queue. It
launches ELF programs, passes arguments, redirects output with `>`, and supports
one `producer | consumer` pipeline.

The old trusted kernel commands remain available only through the recovery boot
entry. Normal programs include `sh`, `cat`, `echo`, `wc`, `ls`, `mem`, `uptime`,
`ps`, and `disk`. The kernel has per-process page tables, user-pointer validation, a
defensive ELF32 loader, and checked file and process syscalls.
The default `/bin/sh` shell runs in Ring 3, executes `/bin/cat /README` through
`spawn` and `wait`, and blocks on descriptor `0` until keyboard or serial input
arrives. Standard streams are inherited through reference-counted open-file
objects, and `dup2` supports safe replacement. Child descriptor actions power
shell redirection and pipelines. `/bin/pipetest` verifies blocking and EOF.

## UDP and DHCP

The network layer maintains a small learned ARP cache, can issue ARP requests,
demultiplexes incoming UDP datagrams by local port, and provides eight bounded
UDP sockets with send/receive operations. DHCP discovery starts when RTL8139
initialization completes; offers trigger a request and acknowledgements replace
the `10.0.2.15` fallback address. The `net` shell command reports the result.
IPv4 loopback delivery supports deterministic local UDP tests without a peer.
DHCP also supplies subnet, gateway, and DNS state used for next-hop routing and
available to Ring 3 through a fixed-layout configuration snapshot.

UDP payloads are capped at 512 bytes with four queued datagrams per socket.
Passing local port zero requests a collision-checked ephemeral port from the
IANA dynamic range, which client libraries such as DNS use automatically.
The userspace library includes a bounded IPv4 DNS resolver. It validates labels,
uses the DHCP-provided server, retries while ARP resolution settles, waits with a
finite poll timeout, and accepts only checked A/IN answers. Routing tables, TCP,
TLS, Wi-Fi, and HTTP remain later networking milestones.

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
clicks. Files provides keyboard navigation across VFS directories: Up/Down
selects an entry, Enter opens a selected directory, and Left returns to the
parent. Network shows stack state, and About identifies the project. Enter
closes the Network and About windows; the red title-bar button closes every
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

The freestanding kernel binary is written to `build/splintos.bin`, and the
separately linked Ring 3 programs are written under `build/user/`. If
`grub-file` is installed, verify its Multiboot header with:

```sh
make check
```

`make test` verifies the Multiboot header, ELF formats, entry address, required
symbols, and absence of unresolved references. `make test-boot` creates the ISO,
boots it headlessly, and asserts serial milestones without a panic.
`make test-storage` performs persistent VirtIO boots and non-destructive probes
of unknown, structurally corrupt, checksum-invalid, and bitmap-invalid images,
plus hash-verified read-only recovery of safe dirty images. Boundary fixtures
distinguish recoverable bitmap drift from unsafe directory drift.
`make debug` waits for GDB on TCP port 1234.

GitHub Actions runs both static and headless boot checks. See
[documentation index](docs/README.md) for the development contract,
architecture, roadmap, build history, and hardware support matrix. The project
is distributed under the MIT License.

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

- `src/` contains kernel and architecture implementation code.
- `include/` contains kernel subsystem interfaces.
- `user/` contains the Ring 3 runtime, libc subset, linker script, and programs.
- `grub/` contains normal and recovery boot entries.
- `scripts/` contains static, boot, storage, and QEMU launch tooling.
- `docs/` contains maintained design, support, and style documentation.
- `Makefile` builds the kernel, user ELF files, ISO, and verification targets.

SplintOS is an educational operating system, not yet a general-purpose desktop.
See [NEXT_STEP.md](NEXT_STEP.md) for the immediate milestone and
[NEXT_STEPS.md](NEXT_STEPS.md) for the longer roadmap.
