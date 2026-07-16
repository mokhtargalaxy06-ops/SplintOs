# Coding style

- Use freestanding C11 and GNU x86 assembly only where hardware requires it.
- Compile cleanly with `-Wall -Wextra -Werror`.
- Prefer fixed-width integer types for hardware and on-wire structures.
- Mark externally defined binary structures `packed`, then validate lengths
  before accessing fields.
- Keep port I/O and volatile MMIO access in driver modules.
- Return explicit errors; never continue after a failed invariant silently.
- Avoid allocation and blocking work in interrupt context.
- Document ownership, locking, byte order, and physical/virtual address rules.
- Keep public APIs in `include/` and internal helpers `static`.
