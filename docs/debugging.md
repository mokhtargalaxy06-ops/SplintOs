# Debugging SplintOS

SplintOS keeps serial diagnostics available throughout boot and builds the kernel
with frame pointers. The linked ELF at `build/splintos.bin` contains symbols for
GDB and panic-address resolution.

## Start a stopped virtual machine

Build the ISO and start QEMU's GDB server:

```sh
make debug
```

The `debug` target starts QEMU with `-s -S`: TCP port 1234 listens for GDB and
the virtual CPU remains stopped before executing the first instruction. Keep that
terminal open.

In another terminal, use a GDB capable of reading i386 ELF files:

```gdb
gdb build/splintos.bin
(gdb) set architecture i386
(gdb) target remote :1234
(gdb) break _start
(gdb) break kernel_main
(gdb) continue
```

When a prefixed cross-toolchain is used, prefer its matching debugger, for
example `i686-elf-gdb build/splintos.bin`.

## Useful breakpoints

```gdb
break memory_init
break hardware_init
break diskfs_init
break scheduler_init
break filesystem_init
break interrupts_init
break elf_load_process
break syscall_dispatch
break kernel_panic
```

Use temporary breakpoints for one boot:

```gdb
tbreak kernel_main
continue
```

Source-level stepping is limited by optimization. Use `nexti`, `stepi`, and
`disassemble /m function_name` when statements have been combined or reordered.

## Inspect CPU and process state

```gdb
info registers
p/x $cr0
p/x $cr2
p/x $cr3
p/x $cr4
x/16wx $esp
x/10i $eip
bt
```

To inspect the current interrupt frame after stopping in `interrupt_dispatch`:

```gdb
p/x *frame
p/x frame->vector
p/x frame->error_code
p/x frame->eip
```

Task and address-space tables are file-static. GDB can still resolve them from
the linked image:

```gdb
p current_index
p tasks[current_index]
p/x tasks[current_index].address_space
```

## Catch exceptions and panics

Set `break kernel_panic` before continuing. When it stops:

```gdb
p message
p frame == 0 ? 0 : frame->vector
p frame == 0 ? 0 : frame->eip
bt
```

The serial panic record prints a bounded list of return addresses after
`BACKTRACE:`. Resolve one or more addresses without a live VM:

```sh
addr2line -e build/splintos.bin -f -C 0xADDRESS
```

Because the kernel is currently linked at a fixed address and does not use
kernel ASLR, no relocation offset is required. Rebuild artifacts must match the
crashed image; otherwise addresses may name the wrong code.

## Debug Ring 3 transitions

Useful transition breakpoints are `interrupt_common`, `syscall_dispatch`,
`interrupt_dispatch`, and `scheduler_tick`. Inspect `frame->cs`: selector `0x1b`
indicates a saved Ring 3 context and `0x08` indicates Ring 0. `frame->useresp`
and `frame->ss` are meaningful only for a privilege transition.

For an ELF application, first stop in `elf_load_process`, then inspect the parsed
headers and newly allocated address space. User ELF artifacts under `build/user/`
retain their own symbols and can be inspected independently with `readelf`,
`objdump`, or a second GDB symbol file.

## Serial and boot-log evidence

Normal automated boot output is saved in:

- `build/boot-test.log`
- `build/missing-device-test.log`
- `build/storage-test-*.log`

Search the earliest missing milestone rather than only the final failure. A
kernel panic is always a test failure. For an interactive serial session use:

```sh
make run
```

The in-kernel boot-log ring keeps the newest bounded diagnostic bytes and can be
read through the checked Ring 3 log syscall.

## Postmortem checklist

1. Preserve the exact `build/splintos.bin`, serial log, QEMU command, and disk image.
2. Record the panic message, vector, error code, EIP, CR2, and backtrace addresses.
3. Resolve every kernel address with the matching ELF.
4. Hash persistent images before attempting recovery or another boot.
5. Reproduce with the smallest supported test profile.
6. Add a deterministic regression or fault-injection boundary before fixing the bug.
7. Run `make test-all` after the fix and retain the failing fixture when practical.

## Limitations

- There is no crash-dump file writer or remote kernel debugger protocol.
- Backtraces rely on valid frame chains and intentionally stop at linked-image bounds.
- Optimized code can still make local variables unavailable.
- SMP debugging is not applicable until the multicore phase.
