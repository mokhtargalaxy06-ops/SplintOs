# Next Steps: From SplintOS to a Useful Operating System

SplintOS already has a strong educational kernel foundation: booting, paging,
interrupts, kernel tasks, a RAM filesystem, input, graphics, networking, and
basic authorization. The next goal should not be to add isolated features at
random. It should be to build a stable platform on which untrusted programs can
run, use persistent files, communicate through standard interfaces, and fail
without crashing the whole machine.

Building a fully general-purpose OS is a multi-year project. This roadmap first
targets a **useful hobby OS** that can boot on supported x86 hardware or QEMU,
run user programs safely, save files, provide a shell and graphical
applications, and connect to a network.

## Progress snapshot

The version 1 system-call contract is now single-source: call numbers and
bounds are shared by kernel C and Ring 3 assembly, public record sizes are
checked at compile time, and generated dependencies rebuild ABI consumers.

Completed foundations now include supervisor-only kernel mappings, isolated
Ring 3 address spaces, a TSS and syscall gate, a defensive ELF32 loader,
separately compiled userspace programs, bounded user-copy checks, process-owned
file descriptors, and `open`/`read`/`write`/`close`/`getpid` syscalls.

The Ring 3 environment now includes shared descriptors, blocking pipes, atomic
spawn actions, redirection, pipelines, system-information commands, and a basic
page-backed heap. Recovery boot is selectable from GRUB. Storage now has a
generic block API, bounded write-back cache, checked MBR parser, compact disk
filesystem, `/disk` VFS mount, Ring 3 conformance program, and a legacy VirtIO
block driver. `SPLFS4` supports bounded 4 KiB files in validated dynamic
extents with a cross-checked allocation bitmap, checksummed metadata, and
clean/dirty commit state. A two-boot QEMU test proves a 1,300-byte Ring 3 write, `fsync`, remount,
and byte verification. Files now use checked first-fit dynamic extents derived
from the authoritative directory. Allocation failure, reclamation, splitting,
and coalescing coverage is complete; the active milestone is closing
stabilization evidence before the versioned `SPLFS5` timestamp layout.
Automated VirtIO boots now cover
unknown formats, overlapping extents, and out-of-range extents non-destructively.
Normal hardware-backed probing is now non-destructive and failed mounts reject
all later operations.

## Guiding rules

Throughout the work, I should:

- Keep the kernel small and move policies and applications into user space.
- Define stable interfaces before adding many drivers or applications.
- Complete and test one vertical feature at a time.
- Preserve serial diagnostics and a headless QEMU test path.
- Never allow user pointers or device input to reach kernel code unchecked.
- Document unsupported hardware instead of claiming universal support.
- Prefer simple, well-documented formats and devices for the first version.

## Phase 1: Stabilize the current kernel

Before introducing user space, I need to make the existing foundation reliable.

### Work

- [x] Remove the reentrant main-loop input polling fallback; PS/2 and COM1
  receive paths are interrupt-driven.
- [x] Remove foreground RTL8139 receive polling; receive-ring ownership is
  interrupt-only.
- [x] Bind RTL8139 dispatch to its validated PCI legacy IRQ line instead of
  forwarding every otherwise-unhandled PIC interrupt to the network driver.
- [x] Detect spurious legacy PIC IRQ7/IRQ15 and apply correct EOI semantics.
- [x] Mask unowned PIC lines and enable only initialized timer, input, serial,
  cascade, and network IRQs.
- [x] Add kernel assertions and make panic reports consistent for framed CPU
  faults and frameless internal invariant failures.
- Add structured kernel error codes at subsystem boundaries.
- [x] Define the structured kernel result vocabulary and apply it to generic
  block validation, unsupported writes, capacity, and injected I/O failures.
- [x] Preserve structured invalid, unavailable, unsupported, I/O, and timeout
  causes through the legacy VirtIO block driver.
- [x] Preserve structured scan and I/O causes through checked MBR partitions.
- [x] Preserve structured flush failures through diskfs, VFS, and scheduler
  descriptors while normalizing the version 1 Ring 3 ABI to `-1`.
- [x] Record boot logs in a bounded in-memory ring buffer.
- [x] Expose bounded read-only boot-log snapshots through checked user copies.
- [x] Reap zombie children whose parent exits before collecting them.
- [x] Support blocking and immediate `wait(0)` collection of any child.
- [x] Add bounded Ring 3 `yield` and wrap-safe timer-tick `sleep`.
- [x] Expose bounded VFS seek with shared open-file offsets across `dup2`.
- [x] Add checked path `stat` using the directory metadata ABI.
- [x] Add fixed-layout `uname` system identity.
- [x] Add descriptor truncation with offset clamping and zero-filled growth.
- [x] Add checked Ring 3 empty-directory creation and removal.
- [x] Expose ownership-checked `chmod` with a bounded permission mask.
- [x] Expose read-only process user identity without change authority.
- [x] Prove true sector exhaustion with a free directory slot and post-unlink reuse.
- [x] Add blocking bounded descriptor polling with timer deadlines.
- [x] Expose the monotonic tick clock and its explicit frequency.
- [x] Add hostile syscall-boundary regression cases that must fail without a fault.
- [x] Audit interrupt locking, scheduler wait state, boot logging, input rings,
  mouse/key snapshots, networking queues, and shared VFS state for races.
- [x] Define nest-safe local interrupt save/restore helpers and remove
  unconditional re-enabling from scheduler sleep and exit critical sections.
- [x] Make keyboard and mouse snapshot-and-clear consumers atomic against their
  IRQ producers.
- [x] Make trusted console take/readiness consumers atomic against IRQ producers.
- [x] Guard every public VFS operation against timer-preemption races with
  foreground GUI and recovery-console access.
- [x] Validate VFS write end-offset arithmetic once before disk bounds,
  allocation, copying, and descriptor advancement.
- [x] Prove a maximum-size Ring 3 truncate fails without changing file metadata
  or terminating the caller.
- [x] Centralize subsystem interrupt control and reject new raw `cli`/`sti`
  instructions through the static check.
- [x] Add an IRQ-saving spinlock abstraction, locking rules, and protection for
  diagnostic-log snapshots and writers.
- Add timer deadlines and timeouts so hardware failures cannot hang forever.
- [x] Bound RTL8139 reset and per-dispatch receive work so stuck device state
  cannot hang initialization or interrupt handling.
- [x] Preserve RTL8139 receive interrupt assertion when the bounded dispatch
  budget leaves frames pending.
- Separate architecture-specific x86 code from portable kernel code.
- [x] Centralize x86 port-I/O assembly behind an architecture-owned header and
  enforce the boundary with static checks.
- [x] Document ownership, borrowing, transfer, reference, and release rules for
  dynamic memory, tasks, descriptors, VFS nodes, storage, and network objects.
- Test allocation failure, malformed Multiboot data, missing devices, and bad
  network packets.
- [x] Reject overflowing, undersized, truncated, and misaligned-end Multiboot
  memory-map records before allocator initialization.
- [x] Exercise valid, undersized, truncated, and overflowing Multiboot map
  fixtures during every automated boot.
- [x] Exhaust the kernel heap deterministically, verify bounded allocation
  failure, free all blocks, and prove coalesced reuse on every boot.
- [x] Reject kernel allocation-alignment and RAMFS capacity-growth overflow;
  exercise a `SIZE_MAX` allocation in the boot fixture.
- [x] Validate `kfree` ownership against the live heap-block list and prove an
  interior-pointer free cannot release storage.
- [x] Track physical-page allocation provenance separately so reserved and
  duplicate page frees cannot enter the free pool.
- [x] Exercise unowned-page rejection and exact allocate/free/double-free page
  accounting during every boot.
- [x] Count unique usable pages across overlapping Multiboot available ranges.
- [x] Validate the Multiboot information-object extent and widen reserved-range
  page rounding to prevent 32-bit wrap.
- [x] Gate recovery/graphics boot-data parsing, bound command-line scanning, and
  validate the complete framebuffer pitch and physical extent.
- [x] Stop boot before dependent subsystems and interrupts when memory
  initialization fails.
- [x] Roll back partial userspace `brk` growth on allocation or mapping failure
  and preserve the original break.
- [x] Boot to Ring 3 under QEMU with no network device and require a bounded
  missing-RTL8139 diagnostic.
- [x] Diagnose missing or unsupported legacy VirtIO block devices and verify
  the explicit volatile `ramblk0` fallback in the missing-device boot.

### Exit criteria

- SplintOS boots repeatedly in QEMU without leaks, hangs, or intermittent tests.
- Every kernel subsystem can report initialization failure without corrupting
  unrelated subsystems.
- Static checks and headless boot tests run automatically on every change.
- [x] Provide one aggregate `make test-all` gate for every supported automated
  verification profile.

## Phase 2: Build protected user space

This is the most important next milestone. Current tasks run in Ring 0 and can
overwrite the kernel. A useful OS needs isolated Ring 3 processes.

### Work

1. Add a Task State Segment and a dedicated kernel stack for each process.
2. Create a separate page directory for every user process.
3. Map the kernel as supervisor-only and user memory as user-accessible.
4. Map user code, data, heap, and stack with appropriate permissions.
5. Switch address spaces by loading `CR3` during context switches.
6. Enter Ring 3 using an interrupt-return frame.
7. Add a controlled system-call entry mechanism.
8. Validate all user addresses, lengths, strings, and integer arithmetic.
9. Implement process creation, exit, waiting, and termination.
10. Deliver process faults to the owning process instead of panicking the OS.

### Initial system calls

Start with a small, versioned ABI:

```text
process: exit, spawn, wait, getpid, yield, sleep
files:   open, close, read, write, seek, stat, readdir
memory:  map, unmap, brk
time:    clock_get, sleep
system:  uname, log
```

Networking, graphics, and advanced process control can be added after this ABI
works reliably.

### Security requirements

- Copy data through checked `copy_from_user` and `copy_to_user` helpers.
- Reject user mappings that overlap kernel space or overflow an address range.
- Mark writable data non-executable when CPU support permits it.
- Clear newly allocated pages before exposing them to another process.
- Give processes handles instead of raw kernel pointers.

### Exit criteria

- A Ring 3 test program can print, allocate memory, read a file, and exit.
- A crashing or malicious test process is terminated without crashing SplintOS.
- One process cannot read or modify another process or the kernel.

## Phase 3: Load real executable programs

Once Ring 3 works, I need a way to turn files into processes.

### Work

- Implement a strict ELF32 loader for supported x86 executables.
- Validate ELF headers, segment sizes, offsets, permissions, and address ranges.
- Load `PT_LOAD` segments into a fresh address space.
- Create an initial user stack containing arguments and environment values.
- Define a small userspace C ABI and program entry convention.
- Create a freestanding userspace support library with syscall wrappers.
- Build separate kernel and userspace toolchains in the Makefile.
- Add an `init` program as the first user process.

### First user programs

```text
/bin/init
/bin/sh
/bin/ls
/bin/cat
/bin/echo
/bin/cp
/bin/mv
/bin/rm
/bin/mkdir
/bin/ps
```

The existing kernel shell should gradually become a recovery console. Normal
commands should execute as separate user processes.

### Exit criteria

- The kernel loads `/bin/init` from the filesystem.
- The shell can launch multiple ELF programs and receive their exit status.
- Programs use only the documented syscall ABI and userspace library.

## Phase 4: Add persistent block storage

The RAM root filesystem remains temporary, while the completed VirtIO-backed
`/disk` path provides bounded persistent files under QEMU.

### Recommended first path

Use a simple virtualized device first, such as VirtIO block, or a narrowly
supported ATA/AHCI path. VirtIO is a good QEMU target; AHCI provides a path to
more physical machines. Avoid implementing several storage controllers at the
same time.

### Work

- [x] Define a generic block-device API with sector reads, writes, flushes, and
  capacity reporting.
- [x] Implement a bounded legacy VirtIO request queue, status handling, and timeout.
- [x] Add a block cache with explicit dirty-sector writeback.
- [x] Parse a checked MBR subset.
- [x] Implement the documented teaching filesystem, now versioned as `SPLFS4`.
- [x] Mount it at `/disk` and keep RAMFS as root and for temporary files.
- [x] Add descriptor-level `fsync` with error propagation.
- [x] Reject malformed `SPLFS4` directory metadata during mount.
- [x] Checksum directory metadata and test one-byte corruption rejection.
- [x] Verify a persistent QEMU image across two independent boots.
- [x] Add versioned multi-sector files and eliminate large kernel-stack buffers.
- [x] Separate probing from formatting and prove unknown disks remain unchanged.
- [x] Add checked unlink and prove a deleted disk slot can be reused.
- [x] Add bounded same-directory rename with a single-sector metadata commit.
- [x] Support flushed rename-over-existing replacement and reclaim the old slot.
- [x] Add dynamic contiguous extents with overlap validation and reclamation.
- [x] Add hashed QEMU corruption tests for overlap and partition-bound failures.
- [x] Verify directory exhaustion returns an error and reclaimed entries are reusable.
- [x] Add a bounded free-sector bitmap and cross-check it against every extent.
- [x] Reject dirty/interrupted metadata state and bitmap corruption non-destructively.
- [x] Add deterministic block-write failure injection and cache propagation coverage.
- [x] Prove an injected partial `SPLFS4` metadata commit is rejected on remount.
- [x] Mount a dirty but unchanged checksummed directory read-only without writes.
- [x] Distinguish recoverable dirty bitmap drift from rejected directory drift.
- [x] Add userspace `free`, first-fit reuse, splitting, and adjacent-block coalescing.
- [x] Support shrinking `brk` and return complete tail pages to physical memory.
- [x] Add overflow-checked `calloc` and data-preserving `realloc`.
- Add filesystem timestamps through a versioned `SPLFS5` layout and explicit
  migration; rename, unlink, truncation, and full-space checks are complete.
- [x] Specify the fixed-width `SPLFS5` timestamp entry, update semantics, and
  explicit fail-closed `SPLFS4` migration policy.
- [x] Persist `SPLFS5` timestamps and expose them through an append-only,
  fixed-layout Ring 3 metadata query without changing the version 1 stat ABI.
- [x] Decode validated `SPLFS4` entries separately and prove compatibility
  mounts are strictly read-only and byte-for-byte non-destructive.
- [x] Add an explicit recovery-console-only `SPLFS4` to `SPLFS5` migration
  operation using the checked dirty-to-clean metadata commit protocol.
- [x] Add clean unmount; `fsync` and read-only dirty recovery are complete.

### Safety tests

- Power off during file creation, replacement, and metadata updates.
- Reject malformed partition tables and filesystem structures.
- Test full disks, failed writes, duplicate names, and very long paths.
- Run filesystem images through host-side consistency checks where available.

### Exit criteria

- Files survive reboot and clean shutdown.
- A corrupted disk image produces an error or read-only mount, not memory
  corruption or arbitrary kernel execution.
- The OS can install and load its user programs from disk.

## Phase 5: Design stable kernel interfaces

As more programs appear, ad hoc interfaces will become difficult to maintain.

### Work

- Finalize file descriptors as the common interface for files, devices, pipes,
  terminals, and sockets.
- Add anonymous pipes and basic inter-process communication.
- Implement signals or a smaller event-notification mechanism.
- [x] Add inherited process-group identity and checked `getpgrp`/`setpgid`
  operations as the foundation for pipelines and foreground jobs.
- Add terminal sessions, foreground process-group selection, and group-directed
  stop, continue, and interrupt delivery.
- Add terminal devices, line discipline, and job-control foundations.
- Provide `poll` or a similar API for waiting on multiple descriptors.
- Add shared-memory mappings only after ownership and cleanup are well defined.
- Version the syscall ABI and document structures with fixed-width fields.

### Exit criteria

- A shell can create pipelines and redirect standard input and output.
- Blocking I/O sleeps instead of busy-waiting.
- Kernel ABI tests prevent accidental compatibility breaks.

## Phase 6: Make networking usable from applications

The kernel already supports Ethernet, ARP, IPv4, ICMP, UDP, and DHCP. The next
step is a reliable socket interface and the protocols applications need.

### Work

- [x] Expose UDP through capability-checked file-descriptor-based sockets.
- [x] Add single-interface subnet, gateway, DNS, and next-hop routing state.
- [x] Implement userspace IPv4 DNS resolution with bounded parsing and timeouts.
- Implement TCP state handling, retransmission, congestion basics, and cleanup.
- [x] Add deterministic IPv4 loopback networking for local UDP tests.
- [x] Replace the overwrite-prone UDP slot with a four-datagram FIFO.
- [x] Support multiple simultaneous UDP sockets and poll-based blocking or
  non-blocking operation.
- Move DHCP and DNS policy into user-space services when practical.
- Add TLS through a carefully selected, portable library only after the random
  number generator and timekeeping are trustworthy.

### Exit criteria

- A user program can resolve a hostname and exchange TCP data.
- Packet loss, duplicate packets, malformed packets, and closed peers do not
  leak memory or lock the kernel.
- Network services run without kernel privileges unless they need them.

## Phase 7: Improve time, randomness, and security

Network security, authentication, and safe multi-user operation need stronger
primitives than a boot-time TSC seed.

### Work

- Read ACPI power-management and timer information more completely.
- Add a user-settable wall clock; monotonic time and read-only RTC snapshots are
  available.
- [x] Support validated CMOS RTC access where hardware permits it.
- Collect entropy from timing and supported hardware sources.
- Build a cryptographically secure random generator with explicit readiness.
- Add non-executable mappings, guard pages, and randomized user layouts where
  supported.
- Enforce least privilege for services and device access.
- Add password hashing only after secure randomness is available.
- Audit syscall boundaries, executable loading, filesystem parsing, and network
  parsing with fuzz tests.

### Exit criteria

- `/dev/random` or its equivalent never claims security before it is seeded.
- Processes receive only the capabilities and resources they need.
- Common malformed-input tests cannot crash or compromise the kernel.

## Phase 8: Build a real userspace environment

A technically capable kernel becomes useful only when programs can accomplish
real tasks.

### Work

- Expand the userspace C library with strings, memory, allocation, files,
  processes, time, sockets, and terminal support.
- Port or implement a small shell with quoting, variables, pipes, and scripts.
- Add text editing, file management, system monitoring, and network utilities.
- Define a package/archive format with checksums and version metadata.
- Create service supervision for networking, logging, and desktop services.
- Store configuration in documented text files.
- Add users, groups, sessions, and a login program.
- Provide a recovery mode that can repair or replace damaged configuration.

### Useful first application set

- Shell and script runner
- Text editor
- File manager
- Process and memory monitor
- Network configuration and diagnostic tools
- Simple HTTP client
- Log viewer
- Installer and updater

### Exit criteria

- A user can boot, log in, edit and save a file, run programs, inspect the
  system, and transfer a file over the network.
- Services can restart after failure without rebooting the kernel.

## Phase 9: Move the graphical desktop into user space

The current desktop demonstrates framebuffer graphics, but a mature design
should not keep the entire UI inside the kernel.

### Work

- Expose framebuffer or display surfaces through a restricted device API.
- Create a user-space display server or compositor.
- Send keyboard and pointer events through protected input interfaces.
- Define window creation, drawing, event, clipboard, and focus protocols.
- Add bitmap fonts first, then scalable fonts and Unicode text shaping.
- Implement a reusable widget toolkit for buttons, menus, dialogs, and layouts.
- Add a terminal emulator and graphical file manager.
- Support damage tracking and double buffering without exposing kernel memory.

### Exit criteria

- The desktop and applications run in separate processes.
- A crashed application does not freeze the compositor or kernel.
- The shell remains usable if the graphical environment fails.

## Phase 10: Expand hardware support carefully

Supporting every PC is unrealistic for a small project. I should publish a
specific hardware target and add drivers based on that target.

### Recommended order

1. VirtIO block and network for virtual machines.
2. AHCI storage for common SATA systems.
3. PCIe ECAM and MSI/MSI-X interrupt support.
4. xHCI and a minimal USB stack for keyboards, mice, and storage.
5. A basic audio controller and PCM output API.
6. UEFI boot support while retaining a tested GRUB path.
7. SMP startup and multi-core scheduling.
8. A 64-bit x86_64 port after interfaces are stable.

Modern Wi-Fi and accelerated GPUs are large projects and often require vendor
firmware or extensive specifications. They should come after the core OS is
stable, or be addressed through a narrow list of supported devices.

### Exit criteria

- A published compatibility matrix names every tested virtual and physical
  configuration.
- Missing or failed hardware degrades cleanly and never prevents recovery.
- Drivers have timeouts, bounds checks, and isolated resource ownership.

## Phase 11: Add installation, updates, and recovery

A useful OS must be maintainable after its first boot.

### Work

- Build reproducible boot media containing the kernel and root filesystem.
- Add an installer with explicit disk selection and destructive-action checks.
- Maintain two bootable system versions for rollback after a failed update.
- Sign release metadata and verify packages before installation.
- Separate user data from replaceable system files.
- Add safe configuration migrations between releases.
- Provide a recovery image, filesystem repair tools, and serial recovery shell.
- Back up critical metadata before modifying partitions or boot files.

### Exit criteria

- A clean disk can be installed from documented media.
- Interrupted or invalid updates roll back to a bootable system.
- User data survives normal system upgrades.

## Phase 12: Test for reliability and compatibility

Testing must grow with every subsystem rather than being postponed until the
end.

### Automated test layers

- Unit tests for parsers, allocators, filesystems, protocols, and libraries.
- Kernel integration tests that boot under QEMU and report over serial.
- User-space ABI and command tests inside the running OS.
- Fuzz tests for ELF, filesystem, syscall, USB, and network inputs.
- Fault injection for failed allocation, I/O errors, timeouts, and packet loss.
- Long-running stress tests for processes, files, sockets, and memory reuse.
- Boot tests for every supported QEMU machine and storage/network combination.
- Real-hardware smoke tests for each configuration in the support matrix.

### Quality gates

- Treat compiler warnings as errors.
- Run static analysis and undefined-behavior checks where freestanding code
  permits them.
- Track kernel memory leaks and unreleased process resources.
- Require tests and documentation for every new syscall and driver.
- Keep disk images and protocol test cases that reproduce past bugs.

## Recommended implementation order

The shortest dependency path to a useful release is:

```text
Kernel stabilization
-> Ring 3 and separate address spaces
-> System calls and process lifecycle
-> ELF loader and userspace library
-> Persistent block storage and filesystem
-> init, shell, pipes, and core utilities
-> Socket API, DNS, and TCP
-> Secure time and randomness
-> User-space desktop and applications
-> Installer, updater, and recovery
-> Broader hardware support
```

Protected user space and persistent storage are now established. TCP should
remain incremental, and USB, audio, or a large desktop should wait until their
supporting interfaces and tests are designed.

## Definition of a useful SplintOS 1.0

SplintOS can reasonably call itself a useful hobby OS when it can:

- Boot reliably in a documented QEMU configuration and selected real hardware.
- Run multiple isolated Ring 3 ELF programs.
- Prevent one process from reading or corrupting another process or the kernel.
- Save files to disk and recover safely from ordinary I/O failures.
- Provide a userspace shell, pipes, core file tools, and a text editor.
- Obtain network configuration and provide DNS, UDP, and TCP sockets.
- Run a user-space graphical desktop with a terminal and file manager.
- Install, update, roll back, and enter recovery mode.
- Produce useful crash logs and pass repeatable automated tests.
- Clearly document its security limits and supported hardware.

That version would still not match Linux, Windows, or macOS in hardware and
application coverage. It would, however, be a coherent, independently built OS
that can perform useful work and provide a stable base for continued growth.

## Immediate next milestone

The isolation, lifecycle, shell, descriptor, pipeline, heap, recovery, VirtIO,
cache, partition, persistent filesystem, dynamic extent, checksum, and
corruption-rejection milestones are complete. Immediate work is crash-state
detection, fragmentation handling, and scalable allocation metadata.

See [NEXT_STEP.md](NEXT_STEP.md) for the detailed design and completion criteria.

For the story of how the current kernel was built, see
[How I Built SplintOS](HOW_I_BUILT_SPLINTOS.md). For current features and build
commands, see the main [README](README.md).
