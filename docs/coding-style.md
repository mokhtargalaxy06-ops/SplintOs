# Coding style

- Use freestanding C11 and GNU x86 assembly only where hardware requires it.
- Compile cleanly with `-Wall -Wextra -Werror`.
- Maintain deterministic kernel and userspace outputs; verify with
  `make test-reproducible` after build-system or toolchain changes.
- Prefer fixed-width integer types for hardware and on-wire structures.
- Mark externally defined binary structures `packed`, then validate lengths
  before accessing fields.
- Keep port I/O and volatile MMIO access in driver modules.
- Return explicit errors; never continue after a failed invariant silently.
- Avoid allocation and blocking work in interrupt context.
- Document ownership, locking, byte order, and physical/virtual address rules.
- Keep public APIs in `include/` and internal helpers `static`.

# Repository layout

- `src/` contains kernel implementation and architecture entry assembly.
- `src/arch/x86_64/` contains the parallel long-mode implementation while the
  verified i386 baseline remains buildable.
- `include/` contains kernel interfaces; instruction-level code belongs under
  `include/arch/<architecture>/`.
- `user/` contains the freestanding runtime, public userspace headers, and programs.
- `tests/` owns supported static and integration verification entry points.
- `scripts/` contains reusable build, fixture, and QEMU helpers used by tests.
- `docs/` contains maintained design and support contracts; generated output belongs
  only under `build/`.
# Interrupt state

Do not embed `cli` or `sti` in subsystem C code. Use the helpers in
`interrupts.h`, restore the caller's saved state, and keep the protected region
bounded. Direct instructions are reserved for interrupt initialization and
fatal stop paths; the static build check enforces this boundary.
