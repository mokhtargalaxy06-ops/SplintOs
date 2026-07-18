# SplintOS Documentation

This directory contains the maintained technical documentation for SplintOS.
Use the following reading order:

1. [Project overview](../README.md) — features, requirements, build, and run commands.
2. [Architecture](architecture.md) — boot, privilege, processes, syscalls, VFS, and storage.
3. [Subsystem inventory](subsystem-inventory.md) — baseline components, dependencies,
   ABI ownership, verification profiles, and known gaps.
4. [Current milestone](../NEXT_STEP.md) — the next bounded implementation target.
5. [Long-term roadmap](../NEXT_STEPS.md) — phases toward a useful educational OS.
6. [Production roadmap steps](../RoadMapSteps.md) — ordered implementation and verification gates.
7. [Build history](../HOW_I_BUILT_SPLINTOS.md) — chronological implementation record.
8. [Hardware support](hardware-support.md) — tested devices and explicit limitations.
9. [Coding style](coding-style.md) — source and interface conventions.
10. [Debugging](debugging.md) — QEMU/GDB startup, symbols, panic analysis, and postmortem workflow.
11. [x86_64 foundation](x86_64-design.md) — long-mode address map and data-model contracts.
12. [Contributing](../CONTRIBUTING.md) — required development and verification workflow.
13. [Ownership and lifetime](ownership.md) — allocation, transfer, borrowing, and release rules.
14. [SPLFS5 format](splfs5.md) — timestamp layout, semantics, and migration policy.
15. [Screenshots](screenshots.md) — current desktop and every interactive window.

## Documentation roles

- `README.md` is the authoritative user-facing status and build guide.
- `docs/architecture.md` is the authoritative design description.
- `NEXT_STEP.md` contains only the immediate milestone.
- `NEXT_STEPS.md` contains the complete roadmap and exit criteria.
- `HOW_I_BUILT_SPLINTOS.md` is historical; older filesystem versions remain there
  intentionally to explain the evolution to the current format.
- `docs/hardware-support.md` must list only hardware exercised in QEMU or on a
  named physical machine.
- `docs/ownership.md` defines the lifetime contract for dynamic kernel objects.

When code changes, update the smallest authoritative document that owns the
changed fact. Do not copy a full roadmap into the README.

## Verification contract

Before documenting a feature as complete, run:

```sh
make test-all
git diff --check
```

Release and CI validation uses `make ci`, which starts clean, compares two clean
artifact sets, and then runs every supported profile.

`make test-all` requires QEMU. It includes static kernel/userspace checks,
normal and missing-device boots, persistent VirtIO I/O, and non-destructive
rejection of unknown and corrupted disk images.
