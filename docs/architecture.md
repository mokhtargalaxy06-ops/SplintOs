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
20 fsync(fd)
21 unlink(path)
22 rename(old_path, new_path)
```

Pointers are checked against the caller's page tables. Strings and data are
copied through bounded kernel buffers, so VFS never receives user pointers.

Each task owns a bounded descriptor table whose entries refer to shared,
reference-counted open-file objects. Objects represent console input, serial
output, or VFS files. Child creation retains standard streams; close, exit, and
faults release them. A VFS handle closes only on the final release. `dup2`
installs another reference and safely replaces an existing destination.

Descriptors `0`, `1`, and `2` are reserved for standard streams.
Descriptor `0` reads from the keyboard and serial queue. An empty read places
the process in `TASK_IO_WAIT`; device input wakes it without polling.

Pipe endpoints are open-file objects backed by bounded 256-byte rings. Empty
reads and full writes enter scheduler wait states. Reads and writes wake peers,
final-writer close produces EOF, and final-reader close makes writes fail.

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

The partition layer accepts a narrow MBR subset after checking its signature,
type, size, arithmetic, and device bounds. `SPLFS3` uses a versioned superblock,
a fixed directory sector, and up to eight dynamically placed contiguous file
extents. Files are bounded to 4 KiB. Sector counts are derived from file size;
first-fit allocation treats the validated directory as the authoritative free
space map. Mount rejects malformed names, duplicate entries, invalid sizes,
overlapping ranges, metadata-sector references, and out-of-partition extents.
The superblock records an FNV-1a checksum of the complete directory sector.
Metadata updates flush the directory before publishing and flushing its new
checksum; mount rejects mismatches.
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
matching waiter.

Each user process starts with a break at `0x80000000`. The `brk` syscall can
grow it toward `0x90000000` with zeroed user-writable pages. The current
userspace allocator is a monotonic bump allocator; shrinking and `free` are not
implemented.

## Boot modes

The default GRUB entry starts the Ring 3 shell. A second entry passes the
`recovery` Multiboot command line, skips normal ELF startup, and enables the
trusted kernel console.

## Current gaps

- No environment or independent background process reaper
- No descriptor polling, signals, environment, or job control
- No heap shrinking, `free`, or shared memory
- VirtIO requests poll synchronously; interrupts and concurrent requests are absent
- Only legacy/transitional VirtIO PCI is supported, not modern VirtIO capabilities
- `SPLFS3` is flat, limited to eight entries and one contiguous extent per file
- GUI and most services remain in Ring 0; the kernel shell is recovery-only
- No TCP, DNS, or userspace sockets
- No SMP or 64-bit support

The next architecture change is safer and more capable diskfs metadata. See
[NEXT_STEP.md](../NEXT_STEP.md).
