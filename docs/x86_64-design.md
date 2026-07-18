# SplintOS x86_64 foundation

This document fixes the initial long-mode address and data-model contracts. They
are compile-checked by the parallel x86_64 profile before the new kernel boots.

## Address model

The first port uses four-level paging and 48-bit canonical addresses. Five-level
paging is deferred. Every address from boot data, userspace, firmware, PCI, DMA,
or an ELF image must be checked before arithmetic or dereference.

| Region | Start/end | Purpose |
|---|---:|---|
| Null/low guard | `0`–`0x3fffff` | Unmapped; catches null and low-pointer access |
| User mappings | `0x400000`–`0x00007fffffffffff` | Per-process executable, heap, mappings, and stacks |
| Initial user stack top | `0x00007ffffff00000` | Grows downward with an unmapped guard |
| Direct physical map | `0xffff800000000000` | Supervisor-only mapping of supported physical memory |
| Recursive page tables | `0xffffff0000000000` | Supervisor-only page-table inspection window |
| Kernel image | `0xffffffff80000000` | Higher-half kernel text, read-only data, data, and BSS |

The direct map does not imply that every possible physical address is safe RAM.
The physical allocator still derives usable ownership from validated firmware
ranges. MMIO mappings require explicit cache and access attributes.

Kernel stacks are initially 32 KiB with at least one unmapped 4 KiB guard page.
Large pages may bootstrap the kernel/direct map, but final permissions must split
text, read-only data, writable data, guards, and MMIO as required.

## C data model

The x86_64 kernel and userspace use LP64:

- pointers, `uintptr_t`, `size_t`, and `unsigned long` are 64 bits;
- `int` and `unsigned int` remain 32 bits;
- hardware, disk, network, and syscall records use explicit-width integers;
- physical and virtual addresses use `uint64_t` or typed wrappers, never `size_t`;
- conversions from a wider address/size to a device field require an explicit
  checked narrowing operation.

The existing syscall ABI v1 is an i386 contract. The long-mode ABI receives its
own calling convention and architecture-neutral record definitions; pointer-free
v1 records may be reused only when their asserted byte layout is identical.
Pointer-bearing records such as spawn requests require a new 64-bit definition.

## Initial protection policy

- User pages never set supervisor ownership and kernel pages never set user access.
- Writable mappings are non-executable once NX is enabled.
- Kernel text is executable/read-only; rodata is non-executable/read-only; data and
  BSS are writable/non-executable.
- User stacks, heaps, anonymous memory, and writable ELF segments are non-executable.
- Canonical-address and overflow checks precede page-table indexing.
- The low guard, stack guards, and gaps between major regions remain unmapped.

## Migration constraints

The 32-bit kernel stays independently buildable until the x86_64 profile passes
the complete baseline CI contract. Portable subsystem code must not gain raw CPU
instructions or assume pointer width. Architecture instructions remain under
`include/arch/` or `src/arch/`.

## Interrupt and scheduler foundation

The initial long-mode profile uses the legacy PIC and a 100 Hz PIT for a
deterministic QEMU baseline. IRQ entry saves all general registers and the full
64-bit hardware return frame. The timer dispatcher owns each runnable kernel
task's saved stack pointer and returns a selected frame through `iretq`. A boot
test requires two independent task stacks to run under timer preemption. APIC,
SMP, userspace switching, priorities, and blocking queues remain future work.

## Width and device-boundary audit

The long-mode sources were audited at the Phase 1 boundary:

| Boundary | Representation and checked narrowing |
|---|---|
| Virtual pointers | `uintptr_t`; IDT offsets are split into 16/16/32-bit architectural fields after shifts |
| Physical addresses | `uint64_t`; the allocator checks page alignment and its configured 16 GiB ceiling before a page-index cast |
| Multiboot v1 pointers | `uint32_t` by ABI; additions are rejected on overflow before conversion to `uintptr_t` |
| Memory-map records | packed fixed-width Multiboot layout; variable record sizes and end bounds are validated before access |
| Paging/MMIO arithmetic | `uint64_t`/`uintptr_t`; canonical and overflow checks precede indexing, while port I/O remains explicit 16-bit x86 I/O space |
| DMA | shared mappings validate the complete range against a device mask; incompatible buffers use allocator-owned low pages with direction-aware copy-in/copy-out |

The remaining casts in `physical.c` follow explicit upper bounds (`MAX_PAGES` or
the validated 32-bit Multiboot extent). Portable driver structures have not yet
been linked into the 64-bit image, so this audit does not claim those drivers
are ported.

## Portable subsystem port

The Phase 1 subsystem profile now boots and tests PCI and ACPI discovery,
generic block bounds, legacy VirtIO persistence, a checksummed persistent VFS
record, RTL8139 receive/transmit and DHCP, PS/2 keyboard and mouse events, and a
Multiboot framebuffer with clipped dirty-region composition. Separate QEMU
profiles cover missing devices, live RTL8139 traffic, and consecutive boots on
the same disposable storage image. This closes the code-port boundary in step
12; complete baseline behavior comparison remains the explicit step 14 gate.

## DMA mapping contract

`x86_64_dma_map` borrows the caller buffer until `x86_64_dma_unmap`. A direct
mapping is returned only when every byte fits the device mask. Otherwise the
mapping owns one low physical page allocated below that mask. To-device and
bidirectional mappings copy into the bounce page before submission;
from-device and bidirectional mappings copy back during unmap. Unmap releases
exactly one allocator-owned page and invalidates the mapping. Requests larger
than one page, overflowing ranges, invalid directions, double unmaps, and pool
exhaustion fail without transferring ownership. RTL8139 and legacy VirtIO use
the same range validator before narrowing addresses into 32-bit device fields.

## Behavior-equivalence gate

`make test-equivalence` boots the preserved i386 normal and missing-device
profiles plus the x86_64 missing-device, RTL8139, and consecutive-storage
profiles. It maps observable contracts for allocator ownership, heap reuse,
timer preemption, PCI/ACPI, bounded block I/O, PS/2 input, framebuffer
composition, VFS persistence, RTL8139/DHCP, and device fallback. The gate also
requires the complete i386 Ring 3 shell baseline to remain available. It does
not treat that preservation check as an ELF64 userspace implementation; step
15 cannot retire i386 until the full phase verification gate passes on x86_64.

## Syscall ABI v1

The x86_64 ABI is versioned independently from the i386 `int 0x80` contract.
`RAX` selects a syscall and returns either a nonnegative result or a stable
negative 64-bit error. Arguments one through six use `RDI`, `RSI`, `RDX`,
`R10`, `R8`, and `R9`; `RCX` and `R11` are destroyed by the `syscall`
instruction. The remaining System V callee-saved registers are preserved.
Syscall meanings 1 through 40 retain their established semantic numbering;
41 queries ABI magic, version, record size, and syscall count.

Pointer-bearing records use explicit `uint64_t` addresses and version/size
headers. Scalar records use explicit widths, reserved extension fields, and
compile-time size assertions in `include/splint/abi64.h`. No public x86_64 ABI
layout depends on compiler pointer or `size_t` layout.

## Syscall entry and return

Long-mode system calls enable `EFER.SCE` and program `STAR`, `LSTAR`, and
`FMASK` for the kernel code selector `0x08`, user code selector `0x23`, and
user data selector `0x1b`. Entry uses `swapgs` to reach a kernel-owned GS
record, saves the user stack pointer, and switches to a dedicated 32 KiB
kernel syscall stack before calling C code. The entry frame preserves the
userspace arguments and the `RCX` user instruction pointer and `R11` flags
consumed by the processor.

Before `sysretq`, the return path requires both the instruction and stack
pointers to be canonical addresses in the configured lower-half user range.
It also clears privileged or unsafe status flags. A rejected return reports a
serial diagnostic and stops instead of allowing a noncanonical `sysretq` to
fault in kernel privilege. The MSR setup, ABI dispatch, unsupported-call error,
and hostile return-address rejection are covered by the boot conformance test;
an actual Ring 3 instruction transition will be exercised when Phase 2 adds a
per-process user address space.

## User-memory access

Kernel code does not directly trust syscall pointers. The x86_64 user-copy
helpers first validate the entire range against the lower-half user boundary,
then walk the active four-level page tables page by page. Copy-from-user
requires present user mappings; copy-to-user additionally requires writable
permission at every paging level. No bytes are accessed unless the complete
range passes. Empty copies are defined as successful and perform no access.

Boot regression coverage submits null, supervisor, unmapped, overflowing, and
noncanonical addresses in both directions and verifies that rejected reads do
not modify their kernel destination. Positive mapped-page coverage is owned by
the per-process address-space work in the next roadmap step.
