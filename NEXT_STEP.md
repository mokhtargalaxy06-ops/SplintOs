# The Next Step for SplintOS

SplintOS now persists Ring 3 files across QEMU restarts through legacy VirtIO,
the generic block cache, a checked MBR partition, `SPLFS3`, and the `/disk` VFS
mount. The long roadmap is in [NEXT_STEPS.md](NEXT_STEPS.md), and the design is
in [docs/architecture.md](docs/architecture.md).

## Storage work completed

1. Defined bounded block read, write, flush, capacity, and sector-size APIs.
2. Added deterministic `ramblk0` conformance coverage.
3. Added a 16-entry write-back cache with error propagation.
4. Added checked MBR partition discovery.
5. Added the versioned, checksummed, multi-sector `SPLFS3` teaching filesystem.
6. Mounted persistent files at `/disk` through normal VFS descriptors.
7. Added descriptor-level `fsync` and removed the raw storage ABI.
8. Added a bounded legacy VirtIO PCI block driver.
9. Added `make test-storage`, which proves format, Ring 3 write, `fsync`, and
   existing-filesystem mount over two QEMU boots of the same image.

## Current limitations

- `SPLFS3` supports eight files, a flat directory, and 4 KiB per dynamic extent.
- Dynamic allocation, unlink, and atomic replacement work; fragmentation handling is basic.
- Formatting is explicit for hardware disks; unknown partitions remain unchanged.
- VirtIO uses one synchronous polling request and supports legacy PCI only.
- There is no clean shutdown or unmount sequence.
- The userspace allocator cannot free or shrink memory.

## Next milestone: scalable and failure-tested allocation

The highest-value next step is extending the checked dynamic allocator with
full-disk, fragmentation, checksum, and crash-behavior coverage. Overlap and
partition-bound corruption are already covered by hashed QEMU tests.
Directory exhaustion and post-unlink reuse are also covered in Ring 3.

### 1. Extend the versioned on-disk layout

Define the next superblock with explicit geometry, feature flags, checksum,
and clean/dirty state. Keep all fields fixed-width and validate arithmetic
before using any sector number or count.

### 2. Add a scalable free-sector bitmap

Reserve documented metadata sectors and track data allocation in a bounded
bitmap. On mount, cross-check every allocated extent, reject overlaps, and
ensure metadata sectors can never be assigned to files.

### 3. Support bounded multi-sector extents

Allow files larger than one sector through a deliberately small extent table.
Handle partial-sector reads and writes, sparse gaps, append, and truncation
without allocating unbounded kernel buffers.

### 4. Add unlink and atomic replacement

Expose VFS operations that update data and metadata in a safe order. A failed
operation must leave either the old file or the new file reachable, and leaked
space must be detectable during mount.

### 5. Add an explicit formatter

Add a recovery-only formatter with a clear target-device confirmation. Normal
boot already refuses to overwrite an unknown partition; retain that invariant.

### 6. Expand corruption and reboot tests

Generate invalid superblocks, duplicate extents, bad checksums, full disks, and
out-of-range metadata. Add two-boot tests for files spanning several sectors,
truncation, replacement, and full-disk error propagation.

## Completion criteria

- A Ring 3 program can fill, reclaim, and reuse dynamically allocated storage.
- Full-disk and allocation failures reach userspace without partial metadata.
- Unknown or corrupted partitions are never formatted automatically.
- Mount validation rejects duplicate, overlapping, and out-of-range extents.
- Automated tests prove multi-sector persistence across QEMU restarts.
- Recovery mode remains usable without a disk.

## After the filesystem milestone

1. Convert VirtIO completion from polling to interrupts and queue multiple I/O.
2. Add clean shutdown, unmount, and cache-drain coordination.
3. Expand the userspace allocator and C library.
4. Add descriptor polling and service supervision.
5. Expose sockets to Ring 3 and implement DNS/TCP.
