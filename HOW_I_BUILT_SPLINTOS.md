# How I Built SplintOS

This document explains, step by step, how I built SplintOS: a small,
independent 32-bit x86 operating-system kernel written in C and x86 assembly.
GRUB loads the kernel, but the kernel itself does not use Linux, a GNU/Linux
userland, or Linux drivers.

## 1. I prepared a freestanding build environment

I started with a compiler that can generate 32-bit x86 code, GNU `ld`, GNU
Make, GRUB tools, xorriso, mtools, and QEMU. On Debian or Ubuntu, the required
packages can be installed with:

```sh
sudo apt-get install gcc-multilib grub-pc-bin mtools xorriso qemu-system-x86
```

The kernel is compiled as a freestanding program because no operating system
or C standard library exists underneath it. The important compiler options in
the `Makefile` are:

- `-m32` to generate 32-bit x86 code
- `-ffreestanding` to compile without assuming a hosted environment
- `-fno-pie` because the kernel is linked at a fixed address
- `-nostdlib` at link time to avoid host libraries and startup files
- `-fstack-protector-strong` to add stack-corruption checks

I can also use an `i686-elf` cross-compiler:

```sh
make CROSS=i686-elf-
```

## 2. I created the Multiboot entry point

I wrote `src/boot.S` as the first code that runs. It contains a Multiboot v1
header so GRUB recognizes the binary as a kernel. The header requests memory
information and an 800x600, 32-bit linear framebuffer.

The assembly entry point `_start` then:

1. Creates and selects a 16 KiB bootstrap stack.
2. Clears the frame pointer.
3. Seeds the compiler's global stack canary using the CPU time-stamp counter.
4. Passes GRUB's Multiboot magic value and information pointer to C.
5. Calls `kernel_main`.
6. Halts safely if `kernel_main` ever returns.

This small assembly layer is the bridge between the bootloader and the C
kernel.

## 3. I defined the kernel's memory layout

I created `linker.ld` and set `_start` as the executable entry point. The
linker places the kernel at the 1 MiB physical address and arranges its
Multiboot header, code, read-only data, writable data, and zero-initialized
data in page-aligned sections.

The script also exports `kernel_start` and `kernel_end`. The memory manager uses
these symbols to ensure that it never allocates pages occupied by the kernel.

## 4. I proved that the kernel could boot

My first C milestone was `kernel_main` in `src/kernel.c`. I validated the magic
number supplied by GRUB and wrote characters directly to VGA text memory at
physical address `0xB8000`. Seeing a message on screen proved that GRUB had
loaded the binary correctly and that control had passed from assembly to C.

I added `grub/grub.cfg` to load `/boot/splintos.bin` through GRUB's `multiboot`
command. The Makefile builds the kernel and packages it into a bootable ISO:

```sh
make
make iso
make run
```

The build pipeline is:

```text
C and assembly sources -> 32-bit object files -> splintos.bin
-> GRUB ISO directory -> splintos.iso -> QEMU
```

## 5. I added device input and diagnostics

Next, I built the basic device layer in `src/devices.c`. It initializes COM1
serial output and PS/2 keyboard and mouse input. Serial logging was especially
important because it lets me inspect boot progress in the terminal even when
graphics are unavailable.

I kept polling as a compatibility fallback, while later routing normal device
input through hardware interrupts.

## 6. I built CPU tables and interrupt handling

In `src/interrupts.c` and `src/interrupt_stubs.S`, I added the processor and
interrupt foundation:

- A flat Global Descriptor Table (GDT)
- A 256-entry Interrupt Descriptor Table (IDT)
- Assembly stubs for CPU exceptions and hardware IRQs
- Remapping for the two legacy 8259 PIC controllers
- A 100 Hz Programmable Interval Timer tick
- Keyboard, mouse, and network interrupt dispatch
- Panic reports for fatal exceptions and page faults

This changed the kernel from a polling-only program into an event-driven
system that can respond to the CPU and hardware.

## 7. I implemented memory management

In `src/memory.c`, I read the variable-length memory map supplied by GRUB. I
created a bitmap allocator that tracks 4 KiB physical pages across as much as
4 GiB of RAM. Before allowing allocation, I reserve low memory, the kernel,
Multiboot data, and other regions already in use.

I then enabled paging with an initial 4 GiB identity map based on 4 MiB pages.
The identity map keeps early development simple because physical and virtual
addresses are initially the same, including legacy device memory and the
framebuffer.

Finally, I added:

- `physical_page_alloc` and `physical_page_free`
- A 1 MiB kernel heap
- `kmalloc` and `kfree`
- Block splitting and coalescing in the heap
- Detailed page-fault diagnostics

## 8. I added tasks and preemptive scheduling

In `src/scheduler.c`, I created fixed-size process control blocks and separate
16 KiB stacks for kernel tasks. Tasks can be created, terminated, put to sleep,
or asked to yield.

The timer interrupt drives a round-robin scheduler. A context switch occurs
every five timer ticks, giving a 20 Hz scheduling quantum from the 100 Hz PIT.
I created a maintenance task during boot to verify context switching and sleep
behavior.

At this stage, all tasks were trusted kernel-mode tasks. Isolated Ring 3
applications still required per-process address spaces, a TSS, system calls,
and an ELF loader.

## 9. I created an in-memory filesystem

In `src/filesystem.c`, I built a writable virtual filesystem backed by RAM. It
supports directories, absolute path lookup, file creation, open/read/write,
append, seek, close, and directory listing through integer file descriptors.

During startup, SplintOS creates a small initial hierarchy including:

```text
/
|-- README
|-- dev
|   |-- null
|   `-- serial
|-- etc
|   `-- motd
`-- tmp
```

The filesystem disappears after shutdown because there is no disk driver yet.
This let me establish a useful VFS interface before implementing ATA, AHCI,
NVMe, or an on-disk filesystem.

## 10. I built a shell and kernel applications

In `src/applications.c`, I added an interactive command shell as its own
scheduled task. It reads from the shared console input queue and writes through
the serial console. Commands allow me to inspect and exercise the OS, including
files, memory, tasks, uptime, networking, devices, and the current identity.

These commands are built into the kernel. They are not Linux programs or
separately loaded user applications.

## 11. I added hardware discovery

In `src/hardware.c`, I implemented PCI configuration-space enumeration across
buses, slots, functions, and multifunction devices. The kernel records device
identifiers, class information, BARs, and interrupt lines, and exposes helpers
that drivers use to enable I/O, memory decoding, and bus mastering.

I also added early ACPI discovery by searching for and validating the RSDP and
RSDT. This provides a base for later work on PCIe, USB, storage, audio, and
power management.

## 12. I implemented Ethernet and a small network stack

In `src/network.c`, I wrote an RTL8139 Ethernet driver and built a small network
stack above it. It currently includes:

- Interrupt-driven Ethernet transmission and reception
- ARP requests and a learned ARP cache
- IPv4 packet validation
- ICMP echo replies
- Bounded UDP sockets
- DHCP discovery, request, and acknowledgement handling

When DHCP is unavailable, the kernel falls back to `10.0.2.15/24`, which works
with QEMU's usual user-mode network. DNS, TCP, TLS, HTTP, and Wi-Fi are later
milestones.

## 13. I drew a graphical desktop

In `src/gui.c`, I used the linear framebuffer requested in the Multiboot header
to draw an 800x600 desktop without an external graphics library. I implemented
rectangles, text, buttons, title bars, window frames, a mouse pointer, and
Network, Files, and About windows.

Rendering first goes to an off-screen backbuffer. Dirty rectangles copy only
the changed areas to the physical framebuffer, reducing flicker and unnecessary
work. VGA text mode remains available when a suitable framebuffer is not
provided.

## 14. I added a security foundation

In `src/security.c` and the related subsystems, I added defenses and
authorization primitives:

- A boot-seeded stack canary and stack-smashing failure handler
- User and group identities attached to tasks
- Capabilities for privileged operations
- File ownership and Unix-style permission bits
- Permission enforcement for VFS and device operations
- Serial audit records for security-sensitive changes

At this stage, this was not complete process isolation. Ring 3, separate address
spaces, NX, ASLR, authentication, and stronger randomness were still future
work.

## 15. I connected the boot sequence

The final initialization order in `kernel_main` is intentional:

```text
VGA diagnostics
-> device and serial setup
-> physical memory and paging
-> PCI and ACPI discovery
-> graphical framebuffer
-> RTL8139 and networking
-> scheduler, security, filesystem, and shell tasks
-> interrupt enablement
-> kernel event loop
```

Memory-dependent services start only after memory initialization succeeds.
Interrupts are enabled after their handlers and the subsystems they call are
ready. The final loop services network, GUI, and device polling fallbacks while
interrupts handle normal asynchronous events.

## 16. I added repeatable checks

I use the Makefile targets to validate each build:

```sh
make clean      # remove generated build files
make            # compile and link build/splintos.bin
make check      # verify Multiboot and static ELF properties
make test       # run all static kernel checks
make iso        # create build/splintos.iso
make test-boot  # boot headlessly and check serial milestones
make run        # launch the graphical OS in QEMU
make debug      # start QEMU paused with a GDB server on port 1234
```

The headless boot test watches the serial log for expected startup milestones
and fails if it detects a kernel panic. This gives me a reproducible way to
confirm that changes still produce a valid, bootable kernel.

## 17. I crossed into protected user space

I replaced the original user-accessible identity map with supervisor-only
kernel mappings, added per-process page directories and a TSS, and taught the
scheduler to switch address spaces. Ring 3 programs enter the kernel through a
checked `int 0x80` syscall gate; invalid user pointers are rejected, and user
faults terminate only the offending process.

I then added a defensive ELF32 loader. `/bin/hello` is compiled and linked
separately from the kernel, packaged into RAMFS, validated through its ELF
headers and loadable segments, mapped into a new address space, executed in
Ring 3, and terminated through the syscall ABI. The headless QEMU test verifies
this complete lifecycle on every test run.

## 18. I gave user programs safe file access

I added bounded descriptor tables to each process and mapped user-visible file
numbers to private kernel VFS handles. The syscall layer validates and copies
paths and buffers before calling the filesystem, so raw Ring 3 pointers never
enter VFS code. Open handles close automatically on process exit or fault.

I also created a reusable userspace startup routine and syscall wrapper library.
`/bin/hello` now uses that runtime, and `/bin/cat` proves that a separately
compiled Ring 3 application can open `/README`, read it through checked kernel
buffers, print it, close its descriptor, and exit successfully.

## 19. I added process creation and waiting

I extended process state with parent identity, exit status, waiting, and zombie
states. The ELF loader now constructs a bounded `argc`/`argv` stack, and the
userspace startup code passes it to `main`.

The new `spawn` syscall validates a userspace path and argument vector before
loading a child into a fresh address space. The new `wait` syscall blocks the
parent in the scheduler, wakes it when the child exits, returns the exact status,
and makes the child slot reusable. `/bin/runner` proves the lifecycle by
spawning `/bin/cat /README`, waiting for it, and reporting status zero.

## 20. I moved the command shell into Ring 3

I added an I/O-waiting task state and connected standard input to the keyboard
and serial character queue. When the queue is empty, a userspace read blocks in
the scheduler. Device input copies a checked byte into the waiting address space
and wakes the process without a polling loop.

I then built `/bin/sh` as a freestanding ELF32 program. It reads bounded lines,
resolves programs under `/bin`, passes arguments through `spawn`, and collects
foreground children with `wait`. Its startup test launches `/bin/cat /README`
before displaying the `user>` prompt. The trusted kernel shell remains compiled
as a disabled recovery path.

## 21. I made descriptors shareable

I replaced direct per-task VFS handles with reference-counted open-file objects.
Console input, serial output, and ordinary files now use the same ownership
layer. A spawned child retains its parent's standard streams, while close, exit,
and faults release references. The VFS handle closes exactly once when the last
reference disappears.

I added `dup2` for controlled descriptor replacement. `/bin/fdtest` duplicates
standard output to descriptor 5, writes through it, and closes it. The new
`/bin/echo` command also proves that spawned programs inherit standard output.

## 22. I added blocking pipes

I added bounded pipe objects with reference-counted read and write endpoints.
The `pipe` syscall installs both endpoints atomically in the caller's descriptor
table. Empty reads and full writes sleep in the scheduler, peer activity wakes
them, final-writer close produces EOF, and writes fail after the last reader
closes.

`/bin/pipetest` duplicates a pipe endpoint onto standard output, spawns
`/bin/echo`, and reads before the child runs. This proves that the parent blocks,
the child wakes it through a pipe write, multiple writes can be drained, and EOF
arrives after child exit.

## 23. I added child descriptor actions

I extended spawn with a bounded request containing child-side `dup2` and close
actions. The kernel validates every source and destination before creating the
task and applies the actions before the child can run.

## 24. I added shell redirection

Using descriptor actions, `/bin/sh` now supports `command > file`. The shell
opens the file, gives only the child a replaced standard output, and keeps its
own terminal output intact.

## 25. I added shell pipelines

The shell can create one `producer | consumer` pipeline. It assigns pipe ends
to child standard streams, closes the parent's copies so EOF can propagate, and
waits for both processes.

## 26. I built a pipeline consumer

I added `/bin/wc`, which reads until EOF from standard input and reports the
number of bytes received. `echo pipeline-data | wc` is now an automated boot
test of descriptor actions, scheduling, pipes, and collection.

## 27. I moved directory listing into user space

I added a bounded directory snapshot syscall and `/bin/ls`. The kernel copies
fixed-layout entries through checked user memory instead of exposing VFS nodes.

## 28. I exposed memory statistics safely

I added a narrow memory-information syscall and `/bin/mem`, allowing Ring 3 to
report total and free physical memory without accessing allocator internals.

## 29. I exposed monotonic uptime

I added an uptime syscall derived from the 100 Hz kernel tick and `/bin/uptime`
to format the result in seconds.

## 30. I added a process summary

I added a checked process-information structure and `/bin/ps`, which reports
the active process count and its own PID without reading scheduler memory.

## 31. I added a userspace heap

Each user process now starts with a break at `0x80000000`. The `brk` syscall
maps zeroed pages up to a fixed limit, while a small userspace bump allocator
provides `splint_malloc`. `/bin/heaptest` verifies writable heap memory.

## 32. I made recovery boot selectable

I added a second GRUB entry that passes `recovery` through Multiboot. Recovery
boot skips normal ELF startup and enables the trusted kernel console; normal
boot continues into `/bin/sh`.

## 33. I defined a generic block layer

I separated sector I/O from storage drivers with checked capacity, sector-size,
read, write, and flush operations. A 256-sector RAM block device provides a
deterministic first implementation and boot-time conformance test.

## 34. I added bounded caching and partition discovery

I added a 16-entry write-back sector cache with dirty eviction and explicit
flush. A narrow MBR parser rejects missing signatures, empty entries, arithmetic
overflow, and partitions outside the underlying device.

## 35. I built a small educational disk filesystem

`SPLFS1` has a versioned superblock, a fixed 12-entry directory, and one data
sector per file. Boot formats an empty partition, writes and flushes a test
file, remounts the metadata, and verifies the bytes read back.

## 36. I connected disk files to Ring 3

I mounted `SPLFS1` at `/disk` through the normal VFS. `/bin/disk` creates,
writes, closes, reopens, and reads a
mounted file using ordinary descriptors before explicitly flushing the cache.

## 37. I added descriptor-level synchronization

I replaced the test program's storage-wide flush with `fsync(fd)`. The syscall
resolves the process descriptor through the shared open-file layer, rejects
non-VFS objects, and flushes only when the VFS node is disk-backed.

## 38. I removed the raw disk-file ABI

Once `/disk` worked through VFS, I removed the provisional raw storage syscalls.
This leaves one checked file interface for RAMFS and diskfs and avoids
duplicating path, permission, descriptor, and user-copy policy.

## 39. I hardened filesystem mounting

The diskfs mount now validates every directory entry before exposing it through
VFS. It rejects missing name terminators, duplicate names, oversized lengths,
wrong slot sectors, stale empty entries, and sectors beyond the partition.

## 40. I added persistent VirtIO block storage

I implemented the legacy VirtIO PCI transport behind the generic block API.
The driver validates its I/O BAR, capacity, and queue size; negotiates flush;
builds bounded descriptor chains; checks completion status; and stops waiting
after a fixed deadline. `ramblk0` remains available when VirtIO is absent.

## 41. I proved persistence across QEMU boots

I added `make test-storage`. It creates a clean MBR disk image, boots SplintOS
to format the current diskfs version, writes and `fsync`s a Ring 3 file, then boots the same image
again and requires an existing filesystem mount. Both boots reject kernel
panics and require the userspace disk test to succeed.

## 42. I expanded disk files beyond one sector

I introduced `SPLFS2`, with explicit geometry and eight fixed eight-sector
extents. Each file can hold 4 KiB, and mount validation checks names, sizes,
slot geometry, and partition bounds before exposing any entry.

The Ring 3 disk test now writes and byte-verifies 1,300 bytes across three
sectors. This uncovered kernel-stack pressure in the VFS adapter, so I moved its
bounded 4 KiB scratch buffers to the coalescing kernel heap. The ordinary boot
and two-boot persistence suites verify the result.

## 43. I made disk probing non-destructive

I stopped diskfs from formatting unknown VirtIO partitions. Formatting is now
implicit only for the private RAM conformance device; host test fixtures create
their `SPLFS2` superblock explicitly.

The new negative QEMU test boots with an MBR partition containing no recognized
filesystem, requires diskfs to report unavailable, and compares SHA-256 hashes
of the complete image before and after boot. This test exposed a stale mount
pointer that still allowed later writes, so I added an explicit mounted-state
invariant and made every diskfs operation fail closed after mount rejection.

## 44. I added safe file deletion and slot reuse

I added `unlink(path)` as syscall 21. VFS checks permissions, rejects directories,
and refuses to remove a node while any kernel VFS descriptor still references
it. Diskfs clears the persistent directory entry before VFS releases the node.

The Ring 3 disk test creates a temporary persistent file, closes and unlinks it,
proves it can no longer be opened, recreates the same name in the reclaimed
slot, and removes it again. The separate 1,300-byte file remains available for
the two-boot persistence proof.

## 45. I added persistent same-directory rename

I added syscall 22 and a VFS rename operation. It rejects missing sources,
existing destinations, cross-directory moves, and permission failures. Diskfs
updates the name in its complete directory-sector image and rolls back the
in-memory entry if the sector write fails; file data never moves.

The Ring 3 disk test renames a closed persistent file, proves the old path is
gone, reopens the new path, verifies all bytes, and finally unlinks it.

## 46. I added atomic rename replacement

Rename can now replace an existing closed file in the same directory. Diskfs
clears the old destination entry and applies the source's new name in one
directory-sector image, flushes that sector before reporting success, and makes
the displaced directory slot reusable. VFS rejects directory targets, mount-boundary
changes, and destinations that still have an open descriptor.

The Ring 3 test writes different bytes to source and destination, performs the
replacement, and verifies that the destination path now contains the source
bytes rather than its previous contents.

## 47. I replaced fixed extents with dynamic allocation

`SPLFS2` now derives each extent length from the file size and uses checked
first-fit placement across the partition. The directory is the authoritative
allocation map, so unlink and replacement reclaim sectors without maintaining
a second structure that could disagree after a crash.

Mount validation recomputes sector counts and rejects ranges that overlap other
files, touch metadata, overflow the partition, or disagree with file length.
The Ring 3 workflow exercises multi-sector growth, deletion, slot and extent
reuse, replacement, and persistence across two QEMU boots.

## 48. I added non-destructive corruption tests

I extended the host disk-image builder with deterministic overlapping-extent
and out-of-range `SPLFS2` directories. `make test-storage` now performs five
boots: two for persistence and three rejected probes for unknown, overlapping,
and escaping metadata.

For every rejected image, the test records a SHA-256 hash before QEMU starts,
requires diskfs to remain unavailable without a kernel panic, and compares the
entire image afterward. This proves both mount rejection and the fail-closed
behavior of later Ring 3 file attempts.

## 49. I tested allocation-table exhaustion

The Ring 3 storage test now fills every remaining `SPLFS2` directory entry,
requires one additional create to fail, unlinks all temporary entries, and
proves a new create succeeds afterward. This covers bounded failure propagation
and reclamation without changing the persistent multi-sector test file.

## 50. I added checksummed metadata

I introduced `SPLFS3`, which stores an FNV-1a checksum of the complete directory
sector in the superblock. Metadata operations flush the directory first, then
publish and flush the matching checksum. Mount rejects a directory whose bytes
do not match the recorded value.

The image builder now includes a fixture that changes one otherwise-unused
directory byte without updating the checksum. QEMU must reject it, avoid a
kernel panic, and leave its full-image SHA-256 unchanged.

## 51. I added checked allocation metadata

I introduced `SPLFS4`, reserving one sector for a 4,096-bit allocation bitmap.
The superblock now records fixed geometry, feature flags, clean/dirty state,
and separate FNV-1a checksums for the directory and bitmap. Mount reconstructs
allocation from every validated extent and rejects any bitmap disagreement.

Metadata commits persist a dirty marker before the bitmap and directory, then
publish a clean superblock with matching checksums. Failed commits disable the
mount. The storage suite now includes non-destructive bitmap-corruption and
dirty-transaction fixtures, and it clears old serial logs before every boot so
assertions cannot pass using stale output.

## 52. I added deterministic block write failures

The generic block layer can now fail writes to one selected device after a
bounded number of successful writes. The boot conformance path uses `ramblk0`
to prove that cache flush reports the injected error, retains dirty data, and
successfully drains it after the fault is cleared. The headless boot test
requires the new serial milestone, making this error path part of routine CI.

## 53. I tested interrupted filesystem commits

The RAM-backed diskfs conformance path now injects failures after successive
successful device writes while creating a file. It requires at least one
failed partial metadata commit to be rejected by a fresh mount, then clears the
fault, reformats the teaching filesystem, and proves clean mounting still
works. The headless QEMU test requires this interrupted-commit milestone.

## 54. I added conservative read-only recovery

`SPLFS4` now distinguishes a dirty but unchanged directory from unsafe partial
metadata. If the superblock's recorded directory checksum still matches and
every entry is structurally valid, mount reconstructs allocation state in
memory and enters read-only mode. All mutating diskfs operations and flushes
fail. If the directory changed or is malformed, mount remains unavailable.

The dirty-image QEMU test now requires the read-only recovery diagnostic, sees
the Ring 3 create attempt fail, and compares whole-image SHA-256 hashes before
and after boot to prove recovery wrote nothing.

## 55. I separated dirty metadata boundaries

The image builder now creates dirty filesystems with no metadata drift,
bitmap-only drift, and directory drift. QEMU recovers the first two read-only
because the old checksummed directory remains authoritative, rejects the third,
and leaves all three complete-image SHA-256 hashes unchanged.

## 56. I made userspace heap blocks reusable

The freestanding allocator now records aligned in-band block headers, reuses
freed blocks with first-fit search, splits oversized blocks, rejects repeated
or foreign frees by walking known headers, and coalesces adjacent free blocks.
The Ring 3 heap test proves address reuse and two-sided coalescing. The allocator
still grows through `brk` and does not return pages to the kernel.

## 57. I returned userspace heap pages

The `brk` implementation now supports shrinking. It removes complete user pages
above the requested break, invalidates their translations, frees empty page
tables, and returns physical pages to the allocator. When the userspace
allocator frees its final block, it contracts the break to that block's header.
The Ring 3 test grows a multi-page tail allocation and verifies the break moves
backward after `free` while earlier live allocations remain writable.

## 58. I completed the basic userspace allocator surface

The userspace runtime now includes overflow-checked `calloc` and checked
`realloc`. Resize consumes an adjacent free block when it fits; otherwise it
allocates a new block, copies the complete old payload, and frees the original.
The Ring 3 test verifies zero initialization and byte-for-byte preservation
while growing from 128 to 512 bytes.

## 59. I retained a bounded boot log

The serial diagnostic path now mirrors every emitted byte into a 4 KiB
overwrite-on-full ring. Snapshot reads return the newest available bytes in
chronological order without exposing internal storage. Device initialization
performs a readback check, and the headless boot suite requires its milestone.

## 60. I exposed checked diagnostic snapshots

Syscall 23 copies at most 512 of the newest boot-log bytes through a checked
writable user range and a bounded kernel staging buffer. It exposes neither
kernel pointers nor mutable log state. The Ring 3 heap test retrieves a snapshot
and verifies that it contains a SplintOS diagnostic marker.

## 61. I closed the orphan-zombie leak

Process exit already reparented live children to the kernel, but a child that
became a zombie before its parent exited retained a task slot forever. The
scheduler now makes those zombies reclaimable during reparenting. `runner`
deliberately leaves three children uncollected; the shell then launches its full
suite, proving the bounded task table remains usable.

## 62. I completed wait-for-any-child

The scheduler's wake path already treated child ID zero as a wildcard, but its
initial child lookup required an impossible literal ID zero and its immediate
zombie path returned zero instead of the collected process ID. Both paths now
return the actual child. `runner` spawns two concurrent children and collects
both through `wait(0)` before exercising orphan cleanup.

## 63. I added Ring 3 yield and sleep

Syscalls 17 and 18 expose voluntary scheduling and timer-backed sleeping. The
scheduler retains the syscall frame, marks the process sleeping, and wakes it
through the existing wrap-safe tick comparison. Durations above half the
32-bit counter range are rejected. `runner` verifies yield and a 100-tick sleep
by observing uptime advance.

## 64. I exposed descriptor seek

Syscall 19 routes seek through the process descriptor and shared open-file
layers into the existing bounded VFS operation. Devices and pipes reject it,
and offsets cannot pass the current end of file. `fdtest` writes a temporary
file, duplicates its descriptor, seeks through one reference, and reads the
expected bytes through the other to prove shared-offset semantics.

## 65. I added path metadata lookup

Syscall 24 resolves a bounded copied path and returns the same fixed-layout
name, type, size, mode, and owner record used by directory listings. The record
is assembled in kernel memory only after the destination range is validated.
`fdtest` verifies a newly written file reports file type and exact size before
unlinking it.

## 66. I added fixed system identity

Syscall 25 returns bounded 16-byte system, release, and machine fields. The
kernel validates the complete writable destination before copying the constant
record, so no internal pointer or unterminated host string crosses the ABI.
The Ring 3 uptime program verifies the SplintOS and i686 identifiers.

## 67. I added checked descriptor truncation

Syscall 26 resizes writable regular files. RAMFS reserves and zeroes growth;
disk files use the checked `SPLFS4` rewrite and commit path. Shrinking clamps
all open VFS offsets referencing the node. `fdtest` verifies shrink metadata,
while the persistent Ring 3 disk workflow grows a six-byte file to 600 bytes
and verifies every added byte is zero before reclaiming the extent.

## 68. I completed empty-directory lifecycle

Syscalls 27 and 28 expose VFS directory creation and removal. Removal refuses
root, mount points, non-directories, non-empty directories, and parent
permission failures before releasing the node. `fdtest` creates a temporary
directory, verifies its metadata type, removes it, and proves lookup fails.

## 69. I exposed checked permission changes

Syscall 29 copies a bounded path, rejects mode bits outside `0777`, and routes
changes through the VFS ownership-or-capability check and security audit. The
descriptor test changes a temporary file to `0600` and confirms the exact mode
through the fixed-layout metadata call.

## 70. I exposed read-only process identity

Syscall 30 returns the scheduler-owned numeric user ID without providing any
identity mutation path. The Ring 3 process-information program verifies the
normal boot suite runs under the expected root identity, while existing VFS
ownership and capability checks remain authoritative.

## 71. I tested true full-disk allocation

The RAM conformance path formats a deliberately constrained 66-sector
`SPLFS4` partition. Seven maximum-size extents leave one directory slot but
only seven sectors free, so an eighth eight-sector allocation fails because of
capacity rather than directory exhaustion. Unlinking one extent makes the same
allocation succeed. Mount also rejects geometry beyond the bitmap's explicit
4,096-sector coverage.

## 72. I added blocking descriptor polling

Syscall 31 accepts at most eight checked descriptor/event records and a bounded
timer-tick timeout. It reports console, file, serial, pipe-data/EOF, and pipe
capacity readiness. When nothing is ready, the scheduler retains the user
array and syscall frame in `TASK_POLL` and reevaluates on timer ticks without
busy-waiting. `pipetest` verifies immediate write readiness, absent read
readiness, and a blocking read poll awakened by child output.

## 73. I exposed the monotonic clock

Syscall 32 returns the scheduler tick counter together with its explicit 100 Hz
frequency. This avoids forcing userspace to infer units from whole-second
uptime. `runner` takes snapshots around a 100-tick sleep and uses wrapping
subtraction to prove at least the requested interval elapsed.

## 74. I added hostile syscall regression coverage

The Ring 3 descriptor test now deliberately supplies kernel-space source and
destination pointers, an unknown poll mask, a timer duration outside the
wrap-safe half range, and a heap break below its base. Each syscall must return
`-1`, after which the same process continues through descriptor, metadata,
permission, and directory tests. This catches validation regressions that a
simple crash-only assertion would miss.

## 75. I exposed descriptor-backed UDP

UDP sockets now live in the same process descriptor and reference-counted
open-file tables as files and pipes. Binding requires network-administration
capability; final close releases the kernel socket. Syscalls 33–35 use bounded
fixed endpoints and 512-byte kernel staging buffers for send and receive.
Sockets participate in poll for queued datagrams and write readiness. `fdtest`
binds port 40000, verifies readiness, broadcasts a three-byte datagram, and
closes the descriptor.

## 76. I added deterministic UDP loopback

UDP sent to `127.0.0.1` or the interface's own address is delivered directly to
a matching local socket while preserving source address and port. It uses the
same bounded queue and poll readiness as hardware receive. `fdtest` sends four
bytes between two process descriptors, waits for readiness, verifies the
payload and endpoint, and closes both sockets.

## 77. I queued UDP bursts without overwrite

Each UDP socket now has a four-datagram FIFO with independent payload lengths
and source endpoints. Receive removes the oldest entry, while a full queue
drops a new arrival instead of overwriting unread data. The loopback test sends
`one` and `two` before receiving and verifies FIFO ordering.

## 78. I added DHCP-backed next-hop routing

DHCP acknowledgements now retain options 1, 3, and 6 for subnet mask, default
gateway, and DNS server. IPv4 UDP transmission ARPs the destination only when
it is on-link; off-link traffic resolves the configured gateway while keeping
the original IP destination. Syscall 36 returns a fixed 16-byte configuration
snapshot, and Ring 3 verifies every fallback or DHCP field is populated.

## 79. I added bounded userspace DNS resolution

The Ring 3 networking library now constructs validated A-record queries for the
DHCP-provided DNS server, retries transmission while ARP resolution settles,
and waits through descriptor poll with a finite timeout. Its exported parser
checks the transaction ID, DNS flags, bounded labels and compression pointers,
question and answer extents, record class, type, and payload length. `fdtest`
uses a synthetic compressed response so boot verification covers parsing without
depending on an external resolver, while applications retain the real UDP query
path.

## 80. I hardened DNS compression boundaries

Compressed names are skipped without recursive pointer traversal, but their
14-bit targets must still identify a byte inside the received packet. The parser
now rejects out-of-packet targets explicitly. Ring 3 regression cases also prove
that truncated A data and mismatched transaction IDs fail safely, complementing
the valid compressed-answer fixture.

## 81. I verified UDP queue saturation

The Ring 3 descriptor test now sends five loopback datagrams into the four-slot
FIFO, verifies that the oldest four arrive in order, and confirms no fifth item
is readable. Local transmission reports the accepted byte count even when the
receiver drops for lack of space, preserving UDP's best-effort contract and
matching the hardware receive path.

## 82. I verified simultaneous UDP socket isolation

The descriptor regression now keeps three UDP sockets open, rejects a duplicate
local-port bind, and polls two receivers together. A datagram addressed to one
port marks only that descriptor readable while the alternate remains idle. This
closes the roadmap item for multiple UDP sockets and poll-based blocking or
zero-timeout operation.

## 83. I added ephemeral UDP port allocation

Opening UDP with local port zero now selects a collision-free port from the
49152–65535 dynamic range, wrapping and rescanning safely. DNS uses this kernel
selection instead of deriving a port from the process ID. Ring 3 sends from an
ephemeral socket and verifies the peer observes a source port in the dynamic
range.

## 84. I separated privileged and ordinary UDP binds

UDP descriptors no longer require network-administration authority merely to
act as clients or bind application ports. Only explicit ports below 1024 remain
capability-gated; port zero and ports 1024–65535 follow normal descriptor and
collision checks. This lets DNS and future clients run with reduced privilege
without weakening service-port policy.

## 85. I pinned DNS replies to the configured resolver

The userspace DNS client now rejects a received datagram unless its source is
port 53 at exactly the DHCP-provided DNS address. Transaction-ID and DNS-format
validation then run as before. This closes the straightforward same-host UDP
spoofing gap without adding protocol complexity to the kernel.

## 86. I validated IPv4 checksums and fragments

The receive path now verifies the complete variable-length IPv4 header checksum
before learning from a packet or dispatching it. It also rejects nonzero fragment
offsets and the more-fragments flag; silently treating fragments as complete
transport packets would be incorrect until a bounded reassembly design exists.
Existing version, header-size, frame-extent, and destination checks remain in the
same early validation gate.

## 87. I completed UDP checksum handling

Outbound UDP now computes a one's-complement checksum across the IPv4 pseudo
header, UDP header, and payload, translating a computed zero to the required
all-ones wire value. Receive validates every nonzero checksum before DHCP or
socket delivery, while accepting zero because IPv4 UDP explicitly uses it to
mean that the sender omitted the checksum.

## 88. I hardened ARP cache learning

ARP receive now validates Ethernet and IPv4 protocol identifiers, canonical
address lengths, request/reply opcodes, agreement between the ARP sender MAC and
the enclosing Ethernet source, and a target equal to the local interface before
learning. Zero-address probes can still be answered but are never cached. These
checks reduce malformed and irrelevant cache updates while retaining a compact
single-interface design.

## 89. I enforced DHCP negotiation state

DHCP now advances explicitly from discover to request to configured. Offers and
acknowledgements must carry the expected hardware type and length, magic cookie,
transaction ID, and client MAC. The request records the selected server, and an
acknowledgement is accepted only when its server identifier matches, preventing
unsolicited or cross-server packets from installing configuration.

## 90. I hardened ICMP echo replies

Echo handling now validates the one's-complement checksum across the entire ICMP
message before replying. It also requires the IPv4 destination to be the local
interface rather than broadcast, preventing the kernel from participating in
broadcast amplification while retaining ordinary diagnostic pings.

## 91. I added a clean filesystem unmount boundary

`diskfs_unmount` now flushes writable media before making the mount inaccessible;
a read-only recovery mount can detach without attempting writes. Boot conformance
writes and flushes a multi-sector file, unmounts, proves access fails while
offline, remounts the same partition, and verifies every byte. A future shutdown
interface can call this boundary after coordinating open descriptors.

## 92. I validated DHCP configuration values

Negotiation no longer installs arbitrary four-byte options. The offered address,
selected server, gateway, and DNS values must be usable unicast addresses, and
the subnet mask must be nonzero and contiguous. Missing or malformed optional
configuration retains the known QEMU-safe fallback rather than breaking routing.

## 93. I exposed a validated CMOS wall clock

Syscall 37 returns a stable RTC calendar snapshot without confusing it with the
monotonic scheduler clock. The bounded reader waits out update-in-progress,
retries across a seconds rollover, converts BCD or binary and 12- or 24-hour
formats, applies a conservative century fallback, and validates every field.
`fdtest` checks the complete Ring 3 structure under QEMU.

## Current result

SplintOS now boots as its own freestanding x86 kernel and provides graphics,
input, interrupts, memory allocation, kernel multitasking, a RAM filesystem, a
command shell, hardware discovery, Ethernet networking, basic security
controls, isolated Ring 3 execution, ELF32 program loading, process-owned file
descriptors, checked userspace file access, argument passing, child spawning,
blocking process waits, an event-driven Ring 3 shell, inherited standard
streams, reference-counted open files, blocking pipes, redirection, pipelines,
system-information commands, a page-backed userspace heap, and a complete
RAM-backed block/cache/partition/disk-filesystem path mounted at `/disk`.

It is still an educational operating system rather than a replacement for a
general-purpose desktop OS. VirtIO-backed `SPLFS4` now survives emulator
restarts; TCP is the next major application-networking step, alongside clean
storage shutdown and broader hardware support.

For feature details, build requirements, and the current support limits, see
the main [README](README.md).
