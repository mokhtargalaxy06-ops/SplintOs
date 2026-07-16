# SplintOS Architecture

## Overview

SplintOS is a freestanding 32-bit x86 monolithic kernel loaded by GRUB
Multiboot. Scheduling, memory, filesystems, drivers, networking, and the current
desktop execute in Ring 0. ELF32 applications execute in isolated Ring 3
address spaces and access services only through a syscall gate.

## Boot sequence

```text
GRUB -> assembly entry -> kernel_main -> devices and serial
-> memory, paging, and heap -> PCI and ACPI -> graphics and networking
-> block cache, MBR, and diskfs -> scheduler, security, and VFS -> ELF processes
-> GDT, TSS, IDT, PIC, and PIT -> interrupt-driven runtime
```

Interrupts start only after their handlers and dependencies are ready. Serial
diagnostics remain available through boot and panic paths.

## Address spaces and privilege

Kernel physical memory and devices are identity-mapped supervisor-only. Each
user process receives a separate page directory with explicit pages between
`0x40000000` and `0xC0000000`. The TSS supplies a Ring 0 stack during privilege
transitions, and the scheduler switches both the interrupt frame and `CR3`.

User faults terminate the offending task. Kernel faults enter the panic path.

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
```

Pointers are checked against the caller's page tables. Strings and data are
copied through bounded kernel buffers, so VFS never receives user pointers.
Ring 3 regression coverage passes kernel-space pointers to write and stat,
unknown poll-event bits, an ambiguous half-range timer deadline, and a break
below the heap base; every call must return an error while the process survives.
Path metadata uses the same fixed-width name, type, size, mode, and owner
record as directory listing and is staged in kernel memory before copy-out.
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
Truncate requires a writable regular-file descriptor. Shrinking clamps every
open VFS offset for the node; growth zero-fills new bytes. Disk-backed changes
use the same checked `SPLFS4` metadata commit path.

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
type, size, arithmetic, and device bounds. `SPLFS4` uses a versioned superblock,
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

## Current gaps

- No environment or independent background process reaper
- No descriptor polling, signals, environment, or job control
- No shared memory or mapped-file support
- VirtIO requests poll synchronously; interrupts and concurrent requests are absent
- Only legacy/transitional VirtIO PCI is supported, not modern VirtIO capabilities
- `SPLFS4` is flat, limited to eight entries, one contiguous extent per file,
  and a bitmap covering at most 4,096 sectors
- GUI and most services remain in Ring 0; the kernel shell is recovery-only
- No TCP, TLS, HTTP, or general-purpose socket compatibility layer
- No SMP or 64-bit support

The next architecture change is safer and more capable diskfs metadata. See
[NEXT_STEP.md](../NEXT_STEP.md).
