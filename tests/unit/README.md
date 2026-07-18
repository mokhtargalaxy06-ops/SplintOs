# Kernel contract fixtures

These fixtures run inside the freestanding kernel where its allocator, page tables,
device model, and interrupt assumptions are real. `kernel-contracts.sh` boots the
normal QEMU profile and requires explicit milestones for:

- malformed and overlapping Multiboot memory maps;
- heap overflow, exhaustion, reuse, invalid frees, and page ownership;
- calendar conversion including leap-year rejection;
- bounded diagnostic-ring behavior;
- block-cache failure propagation;
- interrupted filesystem commits, full-space reclamation, and legacy migration;
- bounded pipe wakeup/EOF semantics; and
- Ring 3 allocator splitting and coalescing.

Parser corruption and persistent crash boundaries additionally run under
`tests/integration/storage.sh` because they require independently hashed disk images.
