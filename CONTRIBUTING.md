# Contributing to SplintOS

SplintOS welcomes focused, testable contributions. Open an issue before a
large architectural change so interfaces can be agreed before implementation.

## Development workflow

1. Build with `make clean test`.
2. If QEMU, GRUB utilities, and xorriso are installed, run `make test-boot`.
3. Keep hardware-specific code behind a small interface in `include/`.
4. Treat all packet, firmware, filesystem, and user-provided lengths as
   untrusted.
5. Update `README.md` and `docs/hardware-support.md` when behavior changes.

Warnings are errors. Contributions must not introduce unresolved symbols,
hosted-library dependencies, silent privilege bypasses, or claims of hardware
support that have not been exercised in an emulator or on named hardware.

By contributing, you agree that your contribution is licensed under the MIT
License in this repository.
