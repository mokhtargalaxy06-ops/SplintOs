# The Next Step for SplintOS

SplintOS has protected Ring 3 execution, a versioned syscall ABI, checked
user-copy boundaries, bounded scheduling and descriptor waits, persistent
VirtIO storage, and deterministic QEMU recovery tests. The long roadmap is in
[NEXT_STEPS.md](NEXT_STEPS.md); verified contracts are in
[docs/architecture.md](docs/architecture.md) and [docs/ownership.md](docs/ownership.md).

## Current milestone: close stabilization, then version disk metadata

Finish the remaining Phase 1 evidence before changing the persistent format.
This keeps storage-format work from depending on ambiguous failure or
concurrency behavior.

### Stabilization evidence

- Complete the scheduler/device shared-state audit.
- Propagate structured failures through diskfs and VFS flush operations.
- Add one aggregate target for static, normal, missing-device, and storage tests.
- Keep serial milestones and architecture-boundary checks authoritative.

### `SPLFS5` timestamps

- Specify fixed-width creation, modification, and metadata-change timestamps.
- [x] Mount fully validated `SPLFS4` media strictly read-only without changing it.
- [x] Add an explicit recovery-only `SPLFS4` to `SPLFS5` migration operation.
- Update create, write, truncate, rename, chmod, and unlink semantics.
- Extend corruption images and two-boot persistence assertions.

## Completion criteria

- `make test-all` covers every supported headless profile.
- Disk and VFS callers retain useful structured failure causes internally.
- No audited shared queue has an unprotected task/IRQ mutation path.
- [x] `SPLFS4` is mounted under a documented, hash-verified read-only policy.
- `SPLFS5` timestamps survive reboot and interrupted writes fail closed.

## After this milestone

1. Terminal line discipline and job-control foundations.
2. TCP with bounded state and malformed-packet tests.
3. Secure randomness, NX, guard pages, and stronger process hardening.
4. User-space display/input services and application isolation.
5. Installation, rollback, stress, fuzz, and hardware qualification.
