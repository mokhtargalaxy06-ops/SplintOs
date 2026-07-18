# Prompt iterations: 2026-07-17

Twenty focused iterations were applied to the highest-risk interface boundary:
the Ring 3 system-call ABI. Each item was analyzed, implemented, and covered by
the normal clean build and static test path.

1. Declared ABI version 1.
2. Centralized all syscall numbers.
3. Centralized the maximum I/O transfer.
4. Centralized the maximum copied path.
5. Centralized spawn argument limits.
6. Centralized spawn action limits.
7. Made the kernel dispatcher consume the shared contract.
8. Made userspace wrappers consume the shared contract.
9. Removed numeric write-wrapper coupling.
10. Removed numeric process-wrapper coupling.
11. Removed numeric file-wrapper coupling.
12. Removed numeric memory/time-wrapper coupling.
13. Removed numeric storage-wrapper coupling.
14. Removed numeric network-wrapper coupling.
15. Checked directory-record layout at compile time.
16. Checked process, memory, identity, and clock layouts at compile time.
17. Checked polling and networking layouts at compile time.
18. Checked the pointer-bearing 32-bit spawn request layout.
19. Added generated header dependencies for kernel and userspace objects.
20. Documented ABI ownership and append-only compatibility.

## Assessment

SplintOS is approximately 45% of a feature-complete educational operating
system. It is in the stabilization and stable-interface milestone. Major work
remains in interrupt-safe locking, crash-boundary storage tests, scalable disk
allocation, terminal/job control, TCP and application protocols, broader
hardware support, and sustained fault testing. Roughly 45 to 70 focused
implementation steps remain. At an incremental educational pace, this is
approximately 12 to 24 months of part-time development. These estimates are
approximate and will change as architecture and hardware scope evolve.

## Iteration 21: nest-safe scheduler critical sections

Replaced unconditional interrupt re-enabling in sleep and exit paths with
EFLAGS save/restore helpers. This establishes the locking rule needed before
protecting shared scheduler and device queues. Verification: static build and
headless boot test. Estimated remaining work: 44 to 69 focused steps and
approximately 12 to 24 months of part-time development. This estimate remains
approximate.

## Iteration 65: process-group foundations

Added scheduler-owned process-group identifiers, inheritance for user process
trees, and append-only `getpgrp` and `setpgid` syscalls. Group changes are
limited to the caller or a direct child and to an existing same-identity group
or the target's own new group. Ring 3 regression coverage verifies inheritance,
self-leadership, rejoining, and rejection of nonexistent groups. Verification:
static build and normal headless boot. Estimated remaining work under the full
production-capable roadmap: 119 to 219 focused milestones and approximately 3
to 7 years of part-time development. This estimate is approximate.

## Iteration 64: explicit recovery-only SPLFS4 migration

Added a trusted recovery-console `migrate-disk` operation that converts only an
already validated, read-only SPLFS4 mount. It writes the zero-timestamp SPLFS5
directory through the existing dirty-to-clean checked commit sequence, and any
write failure disables the mount. A RAM-backed boot fixture performs migration,
remounts, and validates version 5 before restoring the conformance volume.
Verification: static build and normal headless boot. Estimated remaining work:
1 to 26 focused steps and approximately 1 to 9 months of part-time development.
This estimate is approximate.

## Iteration 63: non-destructive SPLFS4 compatibility

Added an isolated 44-byte legacy directory decoder, complete metadata and
allocation validation, and a strictly read-only mount policy for valid SPLFS4
media. A dedicated VirtIO fixture checks the whole-image hash before and after
boot and proves Ring 3 cannot create a file. Verification: static build and the
full storage persistence and recovery matrix. Estimated remaining work: 2 to
27 focused steps and approximately 1 to 10 months of part-time development.
This estimate is approximate.

## Iteration 62: compatible timestamp metadata syscall

Threaded persisted clocks through diskfs and VFS nodes, updated them after
create, write, truncate, rename, and metadata changes, and added append-only
syscall 38 with matching 60-byte kernel and Ring 3 layout assertions. The
original 48-byte stat ABI remains unchanged. Ring 3 storage coverage verifies
the persisted size and nonzero ordered clocks. Verification: static build,
headless boot, and full storage matrix. Estimated remaining work: 3 to 28
focused steps and approximately 1 to 10 months of part-time development. This
estimate is approximate.

## Iteration 61: native SPLFS5 timestamp entries

Newly formatted and mounted volumes now use version 5 and exact 56-byte
timestamped entries. Create initializes all clocks; write preserves birth time
and advances content/change time; rename advances change time. Host corruption
fixtures use the new entry stride. Verification: static build and full storage
matrix. Estimated remaining work: 4 to 29 focused steps and approximately 2 to
10 months of part-time development. This estimate is approximate.

## Iteration 60: checked wall-clock epoch conversion

Added validated Gregorian RTC conversion to unsigned epoch seconds with widened
arithmetic and deterministic epoch, millennium, leap-day, and invalid-century
fixtures required by boot CI. Verification: static build and headless boot.
Estimated remaining work: 5 to 30 focused steps and approximately 2 to 10
months of part-time development. This estimate is approximate.

## Iteration 59: SPLFS5 timestamp specification

Specified 56-byte entries with 32-bit `btime`, `mtime`, and `ctime`, exact
operation semantics, zero-as-untrusted-clock behavior, and an explicit
fail-closed `SPLFS4` recovery migration. Verification: layout arithmetic and
documentation link audit. Estimated remaining work: 6 to 31 focused steps and
approximately 2 to 10 months of part-time development. This estimate is
approximate.

## Iteration 58: completed uniprocessor shared-state audit

Console character take and readiness checks now disable local interrupts around
ring indices, covering recovery-context consumers as well as syscalls. The
documented audit now accounts for scheduler waits, boot logs, input, UDP, and
VFS state. Verification: static build and headless boot test. Estimated
remaining work: 7 to 32 focused steps and approximately 2 to 11 months of
part-time development. This estimate is approximate.

## Iteration 57: structured flush propagation

Diskfs, VFS, and scheduler descriptor flushes now retain invalid, unavailable,
read-only, I/O, and timeout causes. The syscall boundary normalizes failures to
the stable version 1 `-1` contract, with a hostile invalid-fsync regression.
Verification: static build and headless boot test. Estimated remaining work: 8
to 33 focused steps and approximately 3 to 11 months of part-time development.
This estimate is approximate.

## Iteration 54: architecture-owned x86 port I/O

Moved duplicated port-I/O assembly from interrupt, device, PCI, RTL8139, and
VirtIO modules into `arch/x86/io.h`. Static checks reject future subsystem-local
port instructions. Verification: static build and headless boot test. Estimated
remaining work: 11 to 36 focused steps and approximately 4 to 12 months of
part-time development. This estimate is approximate.

## Iteration 53: structured partition failures

Partition discovery now returns exact block-read failures, `NOT_FOUND` for no
valid checked entry, and success after registration. Partition I/O distinguishes
invalid extents while preserving lower-layer failures. Verification: static
build, headless boot, and storage integration. Estimated remaining work: 12 to
37 focused steps and approximately 4 to 13 months of part-time development.
This estimate is approximate.

## Iteration 56: aggregate verification gate

Added `make test-all` to run static kernel/userspace checks, normal QEMU boot,
missing-device boot, and the complete persistent/corrupt storage matrix. The
documentation contract now uses this single gate. Verification: `make test-all`
and `git diff --check`. Estimated remaining work: 9 to 34 focused steps and
approximately 3 to 11 months of part-time development. This estimate is
approximate.

## Iteration 52: structured VirtIO failures

Legacy VirtIO now distinguishes unavailable state, invalid byte arithmetic,
device I/O status, unsupported operations, and completion timeout. Transfer
byte multiplication is overflow-checked. Verification: static build, normal
headless boot, and persistent storage integration. Estimated remaining work: 13
to 38 focused steps and approximately 4 to 13 months of part-time development.
This estimate is approximate.

## Iteration 51: structured block-layer results

Added a shared kernel result vocabulary and converted the generic block layer
to distinguish invalid requests, unsupported writes, device-table exhaustion,
and I/O failures while preserving zero/nonzero compatibility. Verification:
static build, block fault conformance, and headless boot test. Estimated
remaining work: 14 to 39 focused steps and approximately 4 to 13 months of
part-time development. This estimate is approximate.

## Iteration 50: framed and frameless kernel assertions

Added a kernel assertion primitive, null-frame-safe panic reporting, scheduler
selection invariants, and physical-page accounting invariants. Assertions are
reserved for impossible internal state. Verification: static build and
headless boot test. Estimated remaining work: 15 to 40 focused steps and
approximately 5 to 14 months of part-time development. This estimate is
approximate.

## Iteration 49: kernel ownership and lifetime contract

Added an authoritative ownership document covering physical pages, address
spaces, heap blocks, tasks, descriptor references, pipes, VFS buffers, block
cache entries, filesystem extents, device buffers, and UDP queues. Verification:
documentation link audit, static build, and headless boot test. Estimated
remaining work: 16 to 41 focused steps and approximately 5 to 14 months of
part-time development. This estimate is approximate.

## Iteration 48: hostile VFS extent regression

The Ring 3 descriptor test now requests a maximum-size truncate, requires a
clean error, and verifies the original six-byte metadata remains unchanged
before continuing normal I/O. Verification: user ELF checks and headless boot
test. Estimated remaining work: 17 to 42 focused steps and approximately 5 to
14 months of part-time development. This estimate is approximate.

## Iteration 47: checked VFS write extents

VFS writes now reject offset-plus-count overflow and reuse one validated end
offset for disk bounds, RAMFS growth, copying, size changes, and descriptor
advancement. Verification: static build and headless boot test. Estimated
remaining work: 18 to 43 focused steps and approximately 5 to 15 months of
part-time development. This estimate is approximate.

## Iteration 44: bounded recovery and framebuffer boot data

Recovery and GUI parsing now require a validated boot-memory foundation. The
command line is bounded to 256 checked-address bytes, and framebuffer pitch and
the full 600-row physical extent must fit below 4 GiB. Verification: static
build and headless boot test. Estimated remaining work: 21 to 46 focused steps
and approximately 6 to 15 months of part-time development. This estimate is
approximate.

## Iteration 39: physical-page ownership provenance

Added a separate allocation-provenance bitmap. Physical frees now require an
exact page currently owned by the allocator, preventing reserved-page and
duplicate-free corruption. Verification: static build and headless boot test.
Estimated remaining work: 26 to 51 focused steps and approximately 7 to 17
months of part-time development. This estimate is approximate.

## Iteration 46: bounded network backlog continuation

RTL8139 RX acknowledgment is now deferred when the 64-frame budget expires
with unread data, causing PCI INTx to be delivered again after EOI. Bursts are
bounded without being stranded. Verification: static build and headless boot
test. Estimated remaining work: 19 to 44 focused steps and approximately 6 to
15 months of part-time development. This estimate is approximate.

## Iteration 43: checked boot-info and reservation extents

Rejected null or end-overflowing Multiboot information addresses before field
access and widened reserved-range ceiling arithmetic. Near-4-GiB reservations
cannot wrap to page zero. Verification: static build and headless boot test.
Estimated remaining work: 22 to 47 focused steps and approximately 6 to 16
months of part-time development. This estimate is approximate.

## Iteration 38: checked kernel frees

`kfree` now requires an exact allocated payload in the live heap list, rejecting
interior, stale, duplicate, and out-of-range pointers. The boot fixture proves
an interior free does not release a held half-heap block. Verification: static
build and headless boot test. Estimated remaining work: 27 to 52 focused steps
and approximately 8 to 18 months of part-time development. This estimate is
approximate.

## Iteration 37: allocator arithmetic overflow

Kernel allocation now rejects alignment overflow, RAMFS capacity growth fails
before doubling wraps, and the boot fixture requires `kmalloc(SIZE_MAX)` to
return `NULL`. Verification: static build and headless boot test. Estimated
remaining work: 28 to 53 focused steps and approximately 8 to 18 months of
part-time development. This estimate is approximate.

## Iteration 36: missing-storage fallback coverage

Legacy VirtIO absence and unsupported BARs now produce explicit diagnostics
and retain the volatile `ramblk0` fallback. The missing-device QEMU profile
requires that message and the full Ring 3 boot. Verification: static build and
missing-device headless boot. Estimated remaining work: 29 to 54 focused steps
and approximately 8 to 18 months of part-time development. This estimate is
approximate.

## Iteration 35: missing-network boot coverage

Added a `test-missing-devices` QEMU profile with `-nic none`. The kernel emits a
bounded disabled-network diagnostic and must still satisfy the complete Ring 3
boot milestone set. Verification: static build, normal headless boot, and
missing-device headless boot. Estimated remaining work: 30 to 55 focused steps
and approximately 8 to 19 months of part-time development. This estimate is
approximate.

## Iteration 32: checked Multiboot memory maps

Added a validation pass for memory-map address overflow, undersized records,
truncated steps, and exact map termination. Usable 64-bit ranges now saturate
safely at 4 GiB. Verification: static build and headless boot test. Estimated
remaining work: 33 to 58 focused steps and approximately 9 to 20 months of
part-time development. This estimate is approximate.

## Iteration 33: malformed memory-map regression fixtures

Added boot-time fixtures proving acceptance of one valid Multiboot record and
rejection of undersized, truncated, and overflowing layouts. The headless test
requires the new success marker. Verification: static build and headless boot
test. Estimated remaining work: 32 to 57 focused steps and approximately 9 to
19 months of part-time development. This estimate is approximate.

## Iteration 34: kernel heap failure and recovery

The boot self-test now exhausts the fixed kernel heap with page-sized blocks,
requires clean allocation failure, releases every block, and proves a coalesced
half-heap allocation succeeds afterward. Verification: static build and
headless boot test. Estimated remaining work: 31 to 56 focused steps and
approximately 9 to 19 months of part-time development. This estimate is
approximate.

## Iteration 28: spurious PIC interrupt handling

Added 8259A in-service checks for IRQ7 and IRQ15. Phantom IRQ7 is ignored
without EOI, while phantom IRQ15 acknowledges only the real master cascade.
Verification: static build and headless boot test. Estimated remaining work: 37
to 62 focused steps and approximately 10 to 21 months of part-time development.
This estimate is approximate.

## Iteration 42: unique physical-memory accounting

Available-page totals now increment only on a real bitmap transition, so
overlapping Multiboot ranges cannot inflate reported memory while the allocator
tracks only one page. Verification: static build and headless boot test.
Estimated remaining work: 23 to 48 focused steps and approximately 7 to 16
months of part-time development. This estimate is approximate.

## Iteration 30: preemption-safe VFS boundaries

Split all public VFS calls into guarded entry points and internal unlocked
implementations. Global node and descriptor tables can no longer be changed by
a Ring 3 syscall while a timer-preempted GUI or recovery operation is midway
through the same state. Verification: static build and headless boot test.
Estimated remaining work: 35 to 60 focused steps and approximately 10 to 20
months of part-time development. This estimate is approximate.

## Iteration 31: enforced interrupt-control boundary

Centralized the scheduler's explicit enable operation and extended static
checks to reject raw `cli` or `sti` in subsystem C files. Only architecture
initialization and fatal-stop paths remain exempt. Verification: static build
and headless boot test. Estimated remaining work: 34 to 59 focused steps and
approximately 9 to 20 months of part-time development. This estimate is
approximate.

## Iteration 29: explicit PIC ownership masks

Replaced the all-lines-enabled PIC policy with a mask containing only timer,
keyboard, cascade, COM1, mouse, and the validated RTL8139 IRQ when present.
Absent devices can no longer generate an unhandled interrupt storm.
Verification: static build and headless boot test. Estimated remaining work: 36
to 61 focused steps and approximately 10 to 21 months of part-time development.
This estimate is approximate.

## Iteration 26: IRQ-owned network receive

Removed foreground RTL8139 receive polling and made its helper private to the
interrupt path. This prevents concurrent frame parsing and `rx_offset` updates.
Verification: static build and headless boot test. Estimated remaining work: 39
to 64 focused steps and approximately 11 to 22 months of part-time development.
This estimate is approximate.

## Iteration 27: validated RTL8139 IRQ routing

Bound network interrupt handling to the validated legacy IRQ line reported by
PCI enumeration. Other PIC lines no longer access RTL8139 registers, and an
unsupported PCI line rejects initialization. Verification: static build and
headless boot test. Estimated remaining work: 38 to 63 focused steps and
approximately 10 to 22 months of part-time development. This estimate is
approximate.

## Iteration 22: IRQ-owned input polling

Removed the main-loop `devices_poll` fallback because it could be interrupted
by IRQ1, IRQ4, or IRQ12 while modifying the same packet and console state.
Input receive processing is now owned by interrupt dispatch. Verification:
static build and headless boot test. Estimated remaining work: 43 to 68 focused
steps and approximately 12 to 24 months of part-time development. This estimate
is approximate.

## Iteration 25: bounded RTL8139 waits

Replaced the unbounded RTL8139 reset wait with a reported initialization
timeout and capped each receive dispatch at 64 frames. A stuck device can no
longer trap boot or monopolize interrupt context. Verification: static build
and headless QEMU boot test. Estimated remaining work: 40 to 65 focused steps
and approximately 11 to 23 months of part-time development. This estimate is
approximate.

## Iteration 24: interrupt-safe spinlocks

Added an IRQ-saving atomic spinlock with acquire/release ordering and documented
non-recursion, bounded critical sections, and no-sleep rules. Diagnostic log
writes and snapshots now share this lock. Verification: static build and
headless boot test. Estimated remaining work: 41 to 66 focused steps and
approximately 11 to 23 months of part-time development. This estimate is
approximate.

## Iteration 23: atomic input snapshots

Protected keyboard and mouse snapshot-and-clear operations against their IRQ
producers. A key can no longer be erased by an interrupt between read and
clear, and mouse deltas cannot be returned from a torn update. Verification:
static build and headless boot test. Estimated remaining work: 42 to 67 focused
steps and approximately 12 to 24 months of part-time development. This estimate
is approximate.

## Iteration 40: physical allocator ownership fixtures

Added boot fixtures that attempt an unowned-page free, perform an owned
allocation/free cycle, and repeat the free. Exact free-page accounting must
remain unchanged after both rejected operations. Verification: static build
and headless boot test. Estimated remaining work: 25 to 50 focused steps and
approximately 7 to 17 months of part-time development. This estimate is
approximate.

## Iteration 41: transactional userspace break growth

Failed `brk` growth now unmaps every page added by the request and preserves
the original break rather than committing partial progress behind userspace's
back. ELF failure cleanup was audited and already releases its address space.
Verification: static build and headless boot test. Estimated remaining work: 24
to 49 focused steps and approximately 7 to 16 months of part-time development.
This estimate is approximate.

## Iteration 45: memory dependency fail-stop

Memory initialization failure now reports diagnostics and returns directly to
the assembly halt loop. Heap-dependent devices and an uninitialized scheduler
can no longer be reached. Verification: static build and normal headless boot.
Estimated remaining work: 20 to 45 focused steps and approximately 6 to 15
months of part-time development. This estimate is approximate.

## Iteration 55: roadmap reconciliation

Replaced the obsolete immediate-storage plan with the verified next sequence:
finish stabilization evidence and structured propagation, add aggregate CI,
then introduce explicitly migrated `SPLFS5` timestamps. The long-roadmap
snapshot no longer calls completed allocation work active. Verification:
documentation link audit and consistency review. Estimated remaining work: 10
to 35 focused steps and approximately 3 to 12 months of part-time development.
This estimate is approximate.
