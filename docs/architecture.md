# SplintOS Architecture

## Overview

SplintOS is a freestanding 32-bit x86 monolithic kernel loaded by GRUB
Multiboot. Scheduling, memory, filesystems, drivers, networking, and the current
desktop execute in Ring 0. ELF32 applications execute in isolated Ring 3
address spaces and access services only through a syscall gate.
The Makefile also owns a parallel freestanding x86_64 compiler profile. Its
probe requires 64-bit pointers and emits an ELF64 x86-64 object; it does not yet
constitute a bootable long-mode kernel.
Architecture-owned `include/arch/x86/io.h` is the sole C definition point for
x86 port input/output instructions. Interrupt, PCI/ACPI, PS/2/serial, RTL8139,
and VirtIO modules consume that boundary instead of embedding assembly. Static
checks reject port-I/O instructions reintroduced into subsystem C sources.
CPU control primitives are likewise isolated in architecture-owned paths,
including `include/arch/x86/cpu.h`:
interrupt state, halt/relax, paging registers, TLB invalidation, fault address,
task-register loading, and the software reschedule interrupt. Static checks
reject inline assembly outside architecture paths, leaving subsystem C code
free of instruction-level x86 implementations.

## Boot sequence

```text
GRUB -> assembly entry -> kernel_main -> devices and serial
-> memory, paging, and heap -> PCI and ACPI -> graphics and networking
-> block cache, MBR, and diskfs -> scheduler, security, and VFS -> ELF processes
-> GDT, TSS, IDT, PIC, and PIT -> interrupt-driven runtime
```

Interrupts start only after their handlers and dependencies are ready. Serial
diagnostics remain available through boot and panic paths.

Kernel subsystem results use a small negative `kernel_result` vocabulary for
invalid requests, I/O failure, busy state, capacity exhaustion, timeout,
unsupported operations, and missing objects. The generic block layer is the
first converted boundary; callers that only need success/failure remain valid,
while recovery and diagnostics can preserve the specific cause.
Legacy VirtIO maps unavailable state, invalid transfer arithmetic, device I/O
status, unsupported requests, and exhausted completion polling to distinct
results. Sector-count multiplication is checked before constructing descriptor
byte lengths.
Partition discovery preserves block-read failures, reports `NOT_FOUND` when no
checked MBR entry exists, and returns success only after registering a bounded
partition. Partition I/O rejects null and out-of-range requests as `INVALID`
while forwarding exact cache and device failures unchanged.
Diskfs and VFS flush paths preserve `NOT_FOUND`, `UNSUPPORTED`, `INVALID`, and
lower-layer I/O or timeout results through the scheduler descriptor boundary.
The version 1 Ring 3 wrapper deliberately normalizes every failure to its
documented `-1`, so internal diagnostics improve without changing user ABI.

Short scheduler critical sections save EFLAGS, disable maskable interrupts,
and restore the caller's original interrupt-enable state. Code must not pair a
raw `cli` with an unconditional `sti`: doing so breaks nesting and can enable
interrupts inside an outer critical section. These helpers provide local
uniprocessor exclusion only; future SMP support still requires spinlocks plus
documented lock ordering.

Shared state that can be reached from both task and interrupt context uses an
IRQ-saving spinlock. Acquisition disables local interrupts before attempting
the atomic lock; release publishes writes before restoring the saved EFLAGS.
Locks are non-recursive, critical sections must remain bounded, and code must
not sleep while holding one. The diagnostic log is the first protected user of
this contract. Locks must be acquired in subsystem order and never from a lower
layer by callback into a higher layer.

PS/2 keyboard, PS/2 mouse, and COM1 receive state is owned by IRQ dispatch.
The idle loop does not call the device receive routine, preventing reentrant
updates to packet assembly and the console ring while interrupts are enabled.
Keyboard and mouse consumers perform their snapshot-and-clear operations with
local interrupts disabled, so an arriving IRQ cannot be erased or split across
the returned state.
The console character ring has IRQ-only producers. Both trusted recovery reads
and scheduler readiness checks save/disable interrupts around index inspection
and consumption. UDP queue mutation occurs only in IRQ or syscall context, and
scheduler wait-state mutation occurs only with interrupts disabled. The boot
log uses its IRQ-saving spinlock; these boundaries complete the current
uniprocessor shared-state audit.

## Address spaces and privilege

Kernel physical memory and devices are identity-mapped supervisor-only. Each
user process receives a separate page directory with explicit pages between
`0x40000000` and `0xC0000000`. The TSS supplies a Ring 0 stack during privilege
transitions, and the scheduler switches both the interrupt frame and `CR3`.

User faults terminate the offending task. Kernel faults enter the panic path.
Kernel invariant assertions use the same serial/VGA panic path and may report
without a synthetic interrupt frame. The scheduler asserts its selected slot
and address space; the physical allocator asserts free-page accounting. An
assertion is for impossible internal state, never ordinary user or device input.
Panic output includes the interrupt vector, error, control state, general
registers, page-fault address when applicable, and a bounded frame-pointer
backtrace restricted to the linked kernel image. Kernel builds retain frame
pointers so addresses can be resolved against `build/splintos.bin` in GDB.

Before initializing the physical allocator, the Multiboot memory-map walker
checks the map-address addition, minimum entry payload, per-entry step, and
exact end boundary. Usable ranges saturate at the 4 GiB physical limit without
overflowing their 64-bit base-plus-length calculation. Malformed maps fail
initialization with a serial diagnostic rather than driving an out-of-bounds
walker.
The containing Multiboot information address must be nonzero and leave room for
the fixed fields without crossing the 32-bit physical limit. Kernel and
bootloader reservation ceilings use widened arithmetic, so page rounding near
4 GiB cannot wrap a protected range to zero.
Recovery and graphics parsing occur only after successful memory-map
validation. Recovery scans at most 256 command-line bytes with checked pointer
extent. Graphics requires a nonzero 32-bit framebuffer, at least 3,200 bytes of
pitch, and a widened pitch-times-600 extent wholly below 4 GiB before drawing.
Memory initialization is a boot dependency barrier. Failure emits serial and
terminal diagnostics and returns to the assembly fail-stop loop before PCI,
storage, networking, scheduling, or interrupts are initialized. No dependent
subsystem can run against absent allocator or scheduler state.
Kernel heap alignment rejects sizes that would overflow before rounding.
RAMFS geometric growth also fails before a capacity doubling can wrap. The
boot allocator fixture includes a `SIZE_MAX` request and requires a clean
`NULL` result.
`kfree` accepts only the exact payload address of a currently allocated block
reachable from the heap list. Interior, stale-after-coalescing, out-of-range,
and duplicate frees are ignored without interpreting caller-controlled bytes as
allocator metadata. The boot fixture proves an interior free does not make the
held half-heap allocation reusable.
Physical pages have a separate allocator-ownership bitmap in addition to the
used/free map. Allocation establishes provenance; free requires and clears it
before returning the page. Reserved firmware, kernel, bootloader, and device
pages therefore cannot be released through `physical_page_free`, and duplicate
frees are idempotently rejected.
Usable-page totals increase only when a memory-map page transitions from used
to free. Overlapping Multiboot available ranges therefore cannot double-count
physical capacity even though the format permits multiple records.

## ELF loading

Userspace programs are separately compiled static ELF32 files. The build embeds
them as read-only kernel data, then RAMFS initialization installs them in
`/bin`. The loader validates format, architecture, headers, offsets, sizes,
addresses, permissions, and entry point before creating a process.

Current programs:

- `/bin/hello` proves the C startup runtime and standard output.
- `/bin/cat` opens and reads `/README` through file syscalls.
- `/bin/runner` tests child spawning and waiting.
- `/bin/sh` is the default Ring 3 interactive shell.
- `/bin/fdtest` verifies descriptor duplication and release.
- `/bin/echo` prints arguments through inherited standard output.
- `/bin/wc` consumes pipeline input and reports byte counts.
- `/bin/ls`, `/bin/mem`, `/bin/uptime`, and `/bin/ps` expose bounded system views.
- `/bin/heaptest` verifies the page-backed userspace break allocator.
- `/bin/disk` verifies ordinary VFS I/O and cache flush on `/disk`.

## Syscalls and descriptors

User programs call interrupt `0x80`. The current ABI provides:

```text
1 write(fd, buffer, length)   5 close(fd)
2 exit(status)                6 getpid()
3 open(path, flags)           7 spawn(path, argv, count)
4 read(fd, buffer, length)    8 wait(pid, status)
9 dup2(source, destination)
10 pipe(descriptors)
11 spawn_actions(request)
12 list(path, entries, capacity)
13 memory_info(info)          15 process_info(info)
14 uptime()                   16 brk(address)
17 yield()                    18 sleep(ticks)
19 seek(fd, offset)
20 fsync(fd)
21 unlink(path)
22 rename(old_path, new_path)
23 log_read(buffer, capacity)
24 stat(path, entry)
25 uname(identity)
26 truncate(fd, size)
27 mkdir(path)                28 rmdir(path)
29 chmod(path, mode)
30 getuid()
31 poll(entries, count, timeout_ticks)
32 clock_get(clock)
33 udp_open(local_port)
34 udp_send(fd, endpoint, data, length)
35 udp_receive(fd, endpoint, data, capacity)
36 network_config(configuration)
37 wall_clock(clock)
38 stat_timestamps(path, entry)
39 getpgrp()
40 setpgid(process, process_group)
```

Pointers are checked against the caller's page tables. Strings and data are
copied through bounded kernel buffers, so VFS never receives user pointers.
Ring 3 regression coverage passes kernel-space pointers to write and stat,
unknown poll-event bits, an ambiguous half-range timer deadline, and a break
below the heap base; every call must return an error while the process survives.
Path metadata uses the same fixed-width name, type, size, mode, and owner
record as directory listing and is staged in kernel memory before copy-out.
Both kernel and userspace declare its scalar fields with explicit-width types;
no persistent or copy-out record depends on the compiler's `size_t` width.
The append-only timestamp query returns a separate, exact 60-byte record with
32-bit creation, content-modification, and metadata-change epoch seconds. This
preserves the original 48-byte `stat` ABI while making `SPLFS5` clocks visible.
Directory creation inherits the VFS permission model. Removal rejects root,
mount points, non-directories, and directories containing any child node.
Permission changes accept only the low nine mode bits and require ownership or
the kernel permission-change capability.
Identity lookup is read-only and returns the scheduler-owned numeric user ID;
there is no unprivileged identity-changing syscall.
System identity is a fixed 48-byte record containing bounded system, release,
and machine strings; it never exposes compiler or bootloader-owned storage.

Each task owns a bounded descriptor table whose entries refer to shared,
reference-counted open-file objects. Objects represent console input, serial
output, or VFS files. Child creation retains standard streams; close, exit, and
faults release them. A VFS handle closes only on the final release. `dup2`
installs another reference and safely replaces an existing destination.
Seek is restricted to VFS file objects and to offsets no larger than the
current file size. Because duplicated descriptors reference the same open-file
object, they intentionally share one seek position.
Every public VFS operation enters a nest-safe local preemption guard before it
reads or mutates the global node and descriptor tables. This prevents the GUI
or recovery kernel context from being timer-preempted midway through an
operation and resumed after a Ring 3 syscall changed the same table. Internal
helpers remain unlocked and are reachable only through these guarded entry
points or during single-threaded boot initialization.
Truncate requires a writable regular-file descriptor. Shrinking clamps every
open VFS offset for the node; growth zero-fills new bytes. Disk-backed changes
use the same checked `SPLFS4` metadata commit path.
Writes reject `offset + count` overflow before disk limits, capacity growth,
buffer addressing, or descriptor advancement. One validated end offset is used
throughout the operation, preventing divergent arithmetic checks.

Descriptors `0`, `1`, and `2` are reserved for standard streams.
Descriptor `0` reads from the keyboard and serial queue. An empty read places
the process in `TASK_IO_WAIT`; device input wakes it without polling.

Pipe endpoints are open-file objects backed by bounded 256-byte rings. Empty
reads and full writes enter scheduler wait states. Reads and writes wake peers,
final-writer close produces EOF, and final-reader close makes writes fail.
Ring 3 processes can also yield voluntarily or sleep for a bounded number of
100 Hz timer ticks. Deadlines use wrap-safe comparison, and durations above
half the 32-bit tick range are rejected.
The monotonic clock call returns both the wrapping tick value and its explicit
100 Hz frequency, allowing sub-second deadline calculations without assuming a
platform-specific unit.
The wall-clock call is deliberately separate. It takes a stable, bounded CMOS
RTC snapshot, supports BCD or binary and 12- or 24-hour hardware formats, and
returns validated calendar fields. It is read-only and does not provide elapsed
time guarantees.
Heap-break growth is transactional. If physical-page allocation or page-table
mapping fails after partial progress, every page added by that request is
unmapped and the previous byte-accurate break remains authoritative. A failed
`brk` therefore cannot silently consume address space behind the userspace
allocator's unchanged view.
Poll accepts at most eight checked descriptor records and a wrap-safe bounded
timeout. Console input, VFS objects, serial output, pipe data/EOF, and pipe
write capacity contribute readiness. A non-ready caller sleeps in `TASK_POLL`;
the timer path reevaluates its descriptors without busy-waiting.
UDP sockets now occupy normal process descriptors, inherit and close through
the shared open-file lifecycle, and participate in poll. Ports below 1024
require the network-administration capability; ephemeral and other unprivileged
ports do not. Send/receive use fixed endpoints, 512-byte kernel staging buffers,
and fully checked user ranges; receive is nonblocking after poll reports a
queued datagram.
DHCP acknowledgements now retain subnet mask, default gateway, and DNS server.
Outbound IPv4 chooses the destination itself on-link and the gateway off-link,
then resolves that next hop through ARP. Userspace receives a fixed 16-byte
snapshot containing address, subnet, gateway, and DNS configuration.
Inbound IPv4 validates version, header length, total frame extent, header
checksum, destination, and fragmentation flags before learning ARP state or
dispatching a protocol. Fragments are rejected until bounded reassembly exists.
UDP transmission includes the IPv4 pseudo-header checksum. Receive validates
every nonzero UDP checksum while continuing to accept zero, which IPv4 defines
as an explicitly omitted UDP checksum.
ARP accepts only Ethernet/IPv4 records with canonical address sizes, request or
reply opcodes, a sender MAC matching the enclosing Ethernet frame, and a target
equal to the local interface. Zero-address probes may receive a reply but are
never learned into the bounded cache.
DHCP advances through discover, request, and configured states. Replies must
match the transaction, Ethernet hardware identity, DHCP magic cookie, and client
MAC; an acknowledgement must also name the server selected by the accepted
offer.
Installed configuration additionally requires a usable offered address, a
contiguous nonzero subnet mask, and usable gateway and DNS addresses. Missing or
malformed optional values leave the documented QEMU-safe fallbacks intact.
ICMP echo replies are generated only for unicast requests to the interface after
validating the checksum across the complete ICMP message. Broadcast echo and
malformed messages are discarded.
Destination `127.0.0.1` and the interface's own address deliver directly to a
matching local socket while preserving source address and port.
Binding port zero selects an unused port from 49152–65535. The allocator scans
for collisions and wraps inside that range; client libraries therefore avoid
guessing process-derived source ports.
Each socket owns a four-datagram FIFO. Receive removes the oldest datagram;
when full, new arrivals are dropped instead of overwriting unread data. A local
send still reports its accepted byte count because UDP cannot promise receiver
delivery; this matches the observable contract of hardware transmission.
RTL8139 reset has a finite polling budget and reports initialization failure if
the device never clears reset. Receive dispatch processes at most 64 frames per
entry, preventing a stuck or malicious status register from monopolizing the
CPU while still draining ordinary bursts.
If that budget expires with unread frames, receive-OK remains unacknowledged.
The level-triggered PCI line is therefore delivered again after PIC EOI; work
is bounded per entry without stranding a backlog.
Receive-ring consumption is private to the RTL8139 interrupt path. The idle
loop never advances `rx_offset`, so foreground work cannot race an interrupt
while parsing or acknowledging the same frame.
The driver records the legacy IRQ line reported by PCI enumeration and ignores
dispatches from every other line. Unsupported line values fail initialization
instead of treating arbitrary PIC activity as network completion.
Legacy PIC IRQ7 and IRQ15 are checked against the controller in-service
register. Spurious IRQ7 receives no EOI; spurious IRQ15 acknowledges only the
master cascade, matching the 8259A protocol and avoiding phantom dispatch.
PIC initialization starts with every line masked, then enables only PIT,
keyboard, cascade, COM1, PS/2 mouse, and the validated RTL8139 line when that
driver initialized. Unsupported or absent devices therefore cannot create an
unhandled interrupt storm.
The Ring 3 networking library builds bounded DNS A queries and sends them to the
DHCP-provided resolver. Its response parser checks transaction ID, response and
error flags, every label or compression pointer boundary, question and answer
record bounds, class, type, and payload length. ARP warm-up retries and a finite
descriptor-poll timeout keep resolution bounded. Boot CI exercises the parser
with a synthetic compressed answer plus malformed pointer, truncation, and
transaction-ID cases, so correctness does not depend on external network
availability.
Before parsing a live response, the resolver also requires source port 53 and
an exact source-address match with the configured DNS server.

Extended spawn requests carry a bounded descriptor-action array. The kernel
validates every source and destination before task creation, then applies child
`dup2` and close actions before the task becomes runnable. The Ring 3 shell uses
this mechanism for VFS output redirection and two-process pipelines.

## Storage stack

The driver-independent block API provides bounded sector read, write, flush,
sector-size, and capacity operations. A 16-entry write-back cache propagates
write and flush failures. `ramblk0` is the deterministic conformance fallback.

The legacy VirtIO PCI driver negotiates a narrow feature set, validates the I/O
BAR, queue size, and capacity, and uses an aligned split virtqueue. Each request
has a bounded descriptor chain, device status byte, completion check, and polling
deadline. QEMU disks register as `virtblk0`; diskfs prefers their valid MBR
partition while retaining RAM-backed recovery when no supported disk exists.
Mount state is explicit: after failed metadata validation, every diskfs
operation fails closed. Only the private RAM conformance device may be formatted
implicitly; an unknown VirtIO partition remains byte-for-byte unchanged.
The block layer also has a deterministic, device-scoped write-failure hook for
kernel conformance tests. Boot verifies that a failed cache writeback remains
dirty, reports failure, and succeeds after fault injection is cleared.

The partition layer accepts a narrow MBR subset after checking its signature,
type, size, arithmetic, and device bounds. `SPLFS5` uses a versioned superblock,
a fixed directory sector, a one-sector allocation bitmap, and up to eight
dynamically placed contiguous file extents. Files are bounded to 4 KiB. Sector
counts are derived from file size; checked first-fit allocation consults the
bitmap. Mount reconstructs the expected bitmap from the directory and rejects
any disagreement. It also rejects malformed names, duplicate entries, invalid sizes,
overlapping ranges, metadata-sector references, and out-of-partition extents.
The superblock records geometry, feature flags, clean/dirty transaction state,
and FNV-1a checksums for both metadata sectors. Updates persist a dirty marker,
then the bitmap and directory, then a clean superblock with new checksums. An
I/O failure disables the mount. On reboot, a dirty filesystem is recoverable
read-only only when the recorded directory checksum still matches and the
directory passes full structural validation. The kernel reconstructs its bitmap
in memory and rejects all writes, flushes, unlinks, and renames. A changed or
invalid directory still causes mount rejection. QEMU fixtures separately model
an unchanged dirty directory, bitmap-only drift, and directory drift; only the
states retaining the authoritative old directory recover read-only.
Clean unmount flushes writable media before making the mount inaccessible. Boot
conformance proves reads fail while offline, remounts the same partition, and
verifies the committed multi-sector payload; read-only recovery mounts detach
without issuing writes.
The RAM conformance path also formats a 66-sector geometry, fills seven 4 KiB
extents while one directory slot remains free, proves the eighth allocation
fails for lack of sectors, then unlinks one extent and reuses the capacity.
Mount rejects geometry beyond the bitmap's explicit 4,096-sector coverage.
The RAM-backed conformance path injects failures at successive block-write
boundaries until it observes a partial commit, proves remount rejects that
state, then reformats and verifies a clean mount before boot continues.
Host-generated corruption fixtures exercise overlap and bounds rejection in a
real VirtIO boot. Tests hash each rejected image before and after QEMU to prove
that failed probing and later Ring 3 activity cannot modify it.

The VFS mounts this filesystem at `/disk`; its files use the same descriptor
path as RAMFS. Syscall 20 provides descriptor-level `fsync`; storage internals
are not exposed as a parallel userspace file API.

`unlink(path)` resolves through VFS permissions and refuses directories or
files that still have an open kernel descriptor. Disk-backed deletion clears
the directory entry before the VFS node disappears, making its extent
reusable without exposing stale data through a filename.

`rename(old, new)` is currently restricted to the same VFS directory. A closed
file destination may be replaced. Diskfs clears that destination and renames
the source in one complete directory image, then flushes the metadata sector
before returning success. Source data extents never move during rename.

## Userspace runtime

`user/crt0.S` calls C `main` and forwards its result to `exit`.
`user/libc/syscall.S` owns the register ABI, and
`user/include/splint/syscall.h` exposes it to freestanding C programs. User
programs link neither kernel objects nor host libraries.

The ELF loader constructs a bounded `argc`/`argv` stack. A spawned process gets
a fresh address space and records its parent. `wait` blocks the parent in the
scheduler; exit preserves zombie status long enough for collection and wakes a
matching waiter. Parent exit reparents live children to the kernel and makes
already-zombie children immediately reclaimable instead of leaking task slots.
Passing process ID zero to `wait` collects any child and returns that child's
actual ID, whether it was already a zombie or exits after the parent blocks.

Each user process starts with a break at `0x80000000`. The `brk` syscall can
grow it toward `0x90000000` with zeroed user-writable pages. The current
userspace allocator stores checked in-band block headers, reuses first-fit free
blocks, splits oversized blocks, and coalesces adjacent free blocks. A free tail
block shrinks the process break; the kernel unmaps and returns every complete
page above the new break while retaining any partial final page.
The same runtime provides overflow-checked zero allocation and resize. Realloc
grows into an adjacent free block when possible or preserves the old bytes in a
new allocation; invalid allocator pointers are rejected.

## Diagnostics

Every character sent through the serial diagnostic path is retained in a
4 KiB overwrite-on-full ring. Readers receive the newest bounded snapshot in
chronological order. Boot performs a readback conformance check; a controlled
userspace syscall returns at most 512 recent bytes through a checked writable
buffer and never exposes the ring or kernel pointers.

## Boot modes

The default GRUB entry starts the Ring 3 shell. A second entry passes the
`recovery` Multiboot command line, skips normal ELF startup, and enables the
trusted kernel console.
That console alone provides `migrate-disk`, the explicit operation for converting
a fully validated read-only SPLFS4 mount to SPLFS5 metadata.

Process groups are kernel-owned scheduler metadata. The first user process in a
kernel lineage becomes a group leader; descendants inherit that group. A process
may move itself or a direct child into its own group or an existing same-identity
group. Nonexistent groups, unrelated processes, and cross-identity changes fail.
This establishes identifiers for later foreground-terminal and signal routing.

## Current gaps

- No environment or independent background process reaper
- No signals, environment, foreground-terminal selection, or complete job control
- No shared memory or mapped-file support
- VirtIO requests poll synchronously; interrupts and concurrent requests are absent
- Only legacy/transitional VirtIO PCI is supported, not modern VirtIO capabilities
- `SPLFS5` is flat, limited to eight entries, one contiguous extent per file,
  and a bitmap covering at most 4,096 sectors
- GUI and most services remain in Ring 0; the kernel shell is recovery-only
- No TCP, TLS, HTTP, or general-purpose socket compatibility layer
- No SMP or 64-bit support

The next architecture change is safer and more capable diskfs metadata. See
[NEXT_STEP.md](../NEXT_STEP.md).
# System-call ABI ownership

The stable Ring 3 call numbers and global argument bounds live in
`include/splint/abi.h`. Both the kernel dispatcher and the preprocessed
userspace assembly wrappers include that file, preventing a wrapper and its
handler from silently assigning different meanings to the same `int 0x80`
number. Existing numbers are append-only within ABI version 1.

Pointer-free records crossing the privilege boundary have compile-time size
assertions on both sides. Pointer-bearing records are fixed to the 32-bit ABI;
the kernel copies and validates their fields before use. Generated Makefile
dependencies ensure a header-only ABI edit rebuilds every affected object.
