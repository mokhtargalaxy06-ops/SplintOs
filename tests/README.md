# SplintOS tests

This tree owns supported verification entry points:

- `static/kernel.sh` runs build-artifact and source-boundary checks.
- `integration/boot.sh` runs normal and missing-device headless boots.
- `integration/recovery.sh` proves the trusted recovery profile skips Ring 3 startup.
- `integration/x86_64-boot.sh` proves feature checks and entry into long mode.
- `integration/behavior-equivalence.sh` maps verified i386 foundation behavior
  to the x86_64 missing-device, network, graphics, input, and persistent-storage
  profiles while asserting that the complete i386 Ring 3 baseline is preserved.
- `integration/storage.sh` runs persistent, corruption, and recovery profiles.
- `unit/kernel-contracts.sh` requires deterministic in-kernel fixture milestones.
- `check-layout.sh` verifies the repository's required top-level structure.
- `documentation-links.sh` rejects broken local links in maintained Markdown.
- `reproducible-build.sh` compares hashes from two clean kernel/userspace builds.

Low-level reusable QEMU and fixture helpers remain in `scripts/`. Tests may call
those helpers, but the Makefile and CI should enter verification through this tree.
`make ci` begins clean, proves reproducibility, and runs every supported profile.
