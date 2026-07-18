# Kernel object ownership and lifetime

SplintOS uses fixed tables for identities and explicit ownership for dynamic
storage. A pointer never implies ownership by itself. The rules below are part
of each subsystem's interface contract.

## Physical and virtual memory

- `physical_page_alloc` transfers one page to its caller and records allocator
  provenance. Exactly one successful `physical_page_free` returns it.
- Reserved pages have no allocator provenance and cannot be freed.
- `address_space_create` owns its directory. Successful user mappings transfer
  their physical page to that address space.
- `address_space_unmap_user` releases the mapped page and an empty page table.
  `address_space_destroy` releases all remaining user pages, page tables, and
  the directory. The shared kernel directory is never destroyable.
- `kmalloc` returns one heap block. Only its exact payload pointer may be passed
  once to `kfree`; interior, duplicate, stale, and foreign pointers are ignored.

## Processes and descriptors

- A task slot owns its 16 KiB kernel stack until the terminated slot is reused.
- A process task additionally owns its address space. The ELF loader owns a new
  address space until task creation succeeds; successful creation clears the
  loader's ownership. Reaping or slot reuse destroys process memory.
- Task descriptor entries retain an `open_file` table entry. Duplication and
  inheritance add references; close, exit, fault, or replacement releases one.
- The final open-file reference closes its VFS handle, UDP socket, or pipe end.
- A pipe table slot exists while at least one reader or writer end remains. The
  final endpoints release the fixed slot and its buffered bytes.
- Wait, console, pipe, and poll buffer addresses are borrowed from a blocked
  process. They are valid only while its address space and wait state remain;
  wakeup clears the stored address before the process can exit or reuse it.

## Filesystems and storage

- The VFS node table owns RAM-file buffers allocated by `file_reserve`.
  Replacement transfers content to the new buffer and frees the old one;
  unlink frees the final buffer. Disk-backed nodes own metadata only.
- The global VFS descriptor table owns no file data. A descriptor borrows its
  node while marked used; unlink rejects nodes with live descriptors.
- Block devices are statically owned by their drivers. Registration borrows the
  device pointer for the kernel lifetime.
- Cache entries own fixed in-cache sector arrays, not device memory. Flush
  clears dirty ownership only after the driver confirms the write.
- `SPLFS5` directory and bitmap buffers are fixed subsystem state. On-disk
  extents are owned by exactly one validated directory entry or are free.

## Devices and networking

- Input, boot-log, RTL8139, and VirtIO buffers are static and live for the
  kernel lifetime. Consumers copy data; no pointer ownership crosses the API.
- A UDP table slot owns its bounded datagram queue from `udp_open` until the
  final descriptor calls `udp_close`. Send and receive buffers are borrowed
  only for the duration of the checked call.
- Hardware register and framebuffer pointers are borrowed physical mappings;
  drivers never free them.
- An x86_64 DMA mapping borrows its original CPU buffer until unmap. A direct
  mapping owns no memory. A bounced mapping exclusively owns one allocator
  page below the device mask and one fixed mapping slot; unmap performs any
  direction-required copy-back, frees that page, releases the slot, and
  invalidates the mapping.

## Synchronization rule

Ownership transfer and reference-count changes occur with local interrupts
disabled, through an IRQ-saving spinlock, or during single-threaded boot. No
code may sleep while holding an IRQ-saving lock. Future SMP work must preserve
these ownership rules while adding cross-CPU lock ordering.
