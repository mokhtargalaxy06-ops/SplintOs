# SplintOS Documentation

This directory contains the maintained technical documentation for SplintOS.
Use the following reading order:

1. [Project overview](../README.md) — features, requirements, build, and run commands.
2. [Architecture](architecture.md) — boot, privilege, processes, syscalls, VFS, and storage.
3. [Current milestone](../NEXT_STEP.md) — the next bounded implementation target.
4. [Long-term roadmap](../NEXT_STEPS.md) — phases toward a useful educational OS.
5. [Build history](../HOW_I_BUILT_SPLINTOS.md) — chronological implementation record.
6. [Hardware support](hardware-support.md) — tested devices and explicit limitations.
7. [Coding style](coding-style.md) — source and interface conventions.
8. [Contributing](../CONTRIBUTING.md) — required development and verification workflow.

## Documentation roles

- `README.md` is the authoritative user-facing status and build guide.
- `docs/architecture.md` is the authoritative design description.
- `NEXT_STEP.md` contains only the immediate milestone.
- `NEXT_STEPS.md` contains the complete roadmap and exit criteria.
- `HOW_I_BUILT_SPLINTOS.md` is historical; older filesystem versions remain there
  intentionally to explain the evolution to the current format.
- `docs/hardware-support.md` must list only hardware exercised in QEMU or on a
  named physical machine.

When code changes, update the smallest authoritative document that owns the
changed fact. Do not copy a full roadmap into the README.

## Verification contract

Before documenting a feature as complete, run:

```sh
make test
make test-boot
make test-storage
git diff --check
```

`make test-storage` requires QEMU and verifies persistent VirtIO I/O across two
boots plus non-destructive rejection of unknown and corrupted disk images.
