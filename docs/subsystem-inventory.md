# SplintOS subsystem inventory

This inventory records the verified 32-bit baseline before the x86_64 migration.
It complements [architecture.md](architecture.md) and [ownership.md](ownership.md);
those files remain authoritative for detailed behavior and lifetime rules.

## Boot and architecture

| Subsystem | Implementation | Depends on | Current contract | Principal gap |
|---|---|---|---|---|
| Multiboot entry | `src/boot.S`, `linker.ld`, `grub/grub.cfg` | GRUB Multiboot v1, i686 | Validated magic and bounded boot information | No long-mode transition or UEFI path |
| Kernel startup | `src/kernel.c` | devices, memory, hardware, storage, scheduler | Dependency-ordered initialization with memory fail-stop | Initialization is still monolithic |
| CPU tables | `src/interrupts.c`, `src/interrupt_stubs.S` | x86 GDT, TSS, IDT, PIC, PIT | Ring transitions, exceptions, IRQ routing, 100 Hz timer | 32-bit only; no APIC/SMP or IST |
| Architecture boundary | `include/arch/x86/io.h`, `include/arch/x86/cpu.h` | x86 port and CPU instructions | Sole inline-assembly boundary, enforced statically | No architecture-neutral MMIO/DMA layer |

## Memory and execution

| Subsystem | Implementation | Owns | Current contract | Principal gap |
|---|---|---|---|---|
| Physical memory | `src/memory.c` | page bitmap and provenance bitmap | Checked Multiboot ranges, reserved pages, exact free ownership | Limited to 4 GiB and uniprocessor access |
| Virtual memory | `src/memory.c` | kernel directory and per-process directories | Supervisor kernel identity map and isolated user range | No PAE/4-level paging, NX, mmap, COW, or ASLR |
| Kernel heap | `src/memory.c` | bounded heap block list | Aligned allocation, split/coalesce, invalid-free rejection | Fixed capacity and no slab/object allocators |
| Scheduler | `src/scheduler.c` | tasks, open files, pipes, wait state | Preemption, sleep, spawn/wait, process groups, blocking I/O | Fixed tables; no signals, sessions, SMP, or priorities |
| ELF loader | `src/elf.c` | new address space until task transfer | Strict static ELF32 validation and bounded initial stack | No ELF64, dynamic loader, environment, PIE, or shared libraries |

## ABI and userspace

| Boundary | Authoritative source | Version/layout rule | Current consumers | Principal gap |
|---|---|---|---|---|
| Syscall numbers | `include/splint/abi.h` | ABI v1; numbers append-only; current count 40 | `src/syscall.c`, `user/libc/syscall.S` | `int 0x80` only; incomplete process/terminal API |
| Public records | `user/include/splint/syscall.h` | Fixed-width records with compile-time size checks | Ring 3 programs | Some APIs still expose only generic `-1` errors |
| User memory | `src/syscall.c`, `src/memory.c` | Page-table validation before bounded copy | Every pointer-bearing syscall | No shared memory or mapped files |
| C runtime | `user/crt0.S`, `user/libc/` | Freestanding static ELF32 startup | Embedded `/bin` programs | Not a general libc; no dynamic linking or SDK |

Current Ring 3 programs are `hello`, `cat`, `runner`, `sh`, `fdtest`, `echo`,
`pipetest`, `wc`, `ls`, `mem`, `uptime`, `ps`, `heaptest`, and `disk`. They are
built separately, embedded in the kernel image, and installed into RAMFS.

## Filesystems and storage

| Subsystem | Implementation | Owns | Current contract | Principal gap |
|---|---|---|---|---|
| VFS/RAMFS | `src/filesystem.c` | fixed node/descriptor tables and RAM buffers | Absolute paths, permissions, directories, regular/device files | No mount objects, symlinks, scalable caches, or deep persistence |
| Block layer | `src/block.c` | registered devices and 16-sector cache | 512-byte cached I/O, flush, structured errors, fault injection | No async requests, queueing, discard, or general DMA API |
| Partitioning | `src/partition.c` | bounded partition views | Narrow validated MBR subset | No GPT or partition management tools |
| VirtIO block | `src/virtio_block.c` | static legacy queue and DMA buffers | Legacy transitional polling, timeout, persistence | No modern capabilities, interrupts, concurrency, or multiqueue |
| SPLFS5 | `src/diskfs.c`, `docs/splfs5.md` | directory, bitmap, contiguous extents | Checksums, dirty/clean commits, timestamps, SPLFS4 recovery | Eight flat 4 KiB files; no scalable directories or journal |

The trusted recovery console is the only caller allowed to request explicit
SPLFS4-to-SPLFS5 migration. Unknown persistent media is never formatted.

## Devices, graphics, and networking

| Subsystem | Implementation | Ownership/concurrency | Current contract | Principal gap |
|---|---|---|---|---|
| Serial/PS2/RTC | `src/devices.c` | IRQ-owned receive rings; bounded consumers | COM1, keyboard, mouse packets, validated CMOS snapshots | No terminal line discipline, USB input, or settable wall clock |
| PCI/ACPI | `src/hardware.c` | boot-time static discovery tables | PCI config enumeration and checked RSDT discovery | No ECAM, AML, power management, or hotplug |
| Graphics/desktop | `src/gui.c` | Ring 0 framebuffer and compositor state | 800x600 software desktop and window composition | No userspace display server, toolkit, acceleration, or multi-monitor |
| Ethernet | `src/network.c` | IRQ-owned RTL8139 RX; bounded packet state | Ethernet, ARP, IPv4, ICMP, UDP, DHCP | No TCP, IPv6, firewall, Wi-Fi, or socket compatibility |
| UDP userspace | scheduler and syscall layers | descriptor-owned socket queues | Multiple sockets, bounded FIFO, poll, loopback | Fixed capacity and no general socket options |

## Security and diagnostics

| Subsystem | Implementation | Current contract | Principal gap |
|---|---|---|---|
| Identities/capabilities | `src/security.c` | Fixed root/guest identities and bounded capabilities | No authentication, MAC, sandbox policy, or credential service |
| Stack protection | compiler flags, `src/security.c` | Kernel stack canary and panic on corruption | No user canaries, NX, ASLR, CSPRNG, or control-flow hardening |
| Diagnostics | serial paths and boot-log ring | Bounded log snapshot, panic output, serial milestones | No symbols-based stack unwind, crash dump, log daemon, or watchdog |
| Recovery | GRUB recovery entry and kernel console | Normal Ring 3 startup skipped; trusted maintenance commands | No authenticated or transactional recovery environment |

## Dependency order

```text
Multiboot/serial
  -> physical and virtual memory
  -> PCI/ACPI and architecture discovery
  -> generic block -> partitions -> SPLFS5
  -> scheduler/security -> VFS -> ELF processes
  -> interrupt tables/controllers/timer
  -> interrupt-driven input, networking, and runtime
```

The storage stack depends downward only. Filesystem callers may retain structured
lower-layer errors, while ABI v1 normalizes public failures. Interrupts are enabled
only after handlers, scheduler state, and consumers exist.

## Verification profiles

| Command | Evidence |
|---|---|
| `make test` | Strict build, Multiboot/ELF checks, syscall/ownership static rules |
| `make test-boot` | Normal RAM-backed headless boot and Ring 3 regressions |
| `make test-unit` | Deterministic allocator, parser, clock, and data-structure fixtures |
| `make test-missing-devices` | Networking/storage fallback behavior |
| `make test-recovery` | Trusted recovery console with normal Ring 3 startup suppressed |
| `make test-storage` | VirtIO persistence, corruption, dirty recovery, SPLFS4 policy |
| `make test-all` | Aggregate supported baseline |

## Baseline limitations carried into the roadmap

- The kernel is 32-bit, monolithic, uniprocessor, and primarily fixed-capacity.
- GUI, networking policy, filesystems, and most drivers execute in Ring 0.
- Only PS/2, RTL8139, legacy VirtIO block, VGA/framebuffer, serial, PCI, and a
  small ACPI subset are intentionally supported.
- Persistent storage is a teaching filesystem, not a general daily-driver format.
- Security boundaries are meaningful but lack modern exploit mitigations and a
  cryptographically secure source of randomness.
- The build has deterministic QEMU evidence but not broad physical-hardware,
  fuzzing, long-duration, performance, or release qualification.

This inventory must be updated whenever a subsystem is replaced, an ABI is
extended, ownership changes, or a limitation becomes a supported contract.
