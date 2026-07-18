#!/bin/sh
set -eu

kernel=${1:?usage: static-check.sh KERNEL}

command -v readelf >/dev/null 2>&1 || {
    echo "readelf is required for static checks" >&2
    exit 1
}
command -v nm >/dev/null 2>&1 || {
    echo "nm is required for static checks" >&2
    exit 1
}

readelf -h "$kernel" | grep -q 'Class:.*ELF32'
readelf -h "$kernel" | grep -q 'Machine:.*Intel 80386'
header_entry=$(readelf -h "$kernel" | awk '/Entry point address:/ {print $4}')
symbol_entry=$(nm -n "$kernel" | awk '$3 == "_start" {print $1}' | sed 's/^0*//')
symbol_entry=0x${symbol_entry:-0}
if [ "$header_entry" != "$symbol_entry" ]; then
    echo "ELF entry $header_entry does not match _start at $symbol_entry" >&2
    exit 1
fi

if nm -u "$kernel" | grep -q .; then
    echo "kernel contains unresolved symbols:" >&2
    nm -u "$kernel" >&2
    exit 1
fi

source_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if command -v rg >/dev/null 2>&1; then
    raw_interrupts=$(rg -n '"(cli|sti)' "$source_root/src" -g '*.c' |
        grep -Ev '/src/(interrupts|security)\.c:|/src/arch/' || true)
    if [ -n "$raw_interrupts" ]; then
        echo "raw interrupt control outside approved architecture paths:" >&2
        echo "$raw_interrupts" >&2
        exit 1
    fi
    raw_port_io=$(rg -n '"(in|out)[bwl] ' "$source_root/src" -g '*.c' |
        grep -v '/src/arch/' || true)
    if [ -n "$raw_port_io" ]; then
        echo "raw x86 port I/O outside arch/x86/io.h:" >&2
        echo "$raw_port_io" >&2
        exit 1
    fi
    inline_assembly=$(rg -n '__asm__' "$source_root/src" "$source_root/include" \
        -g '*.c' -g '*.h' | grep -Ev '/(include|src)/arch/' || true)
    if [ -n "$inline_assembly" ]; then
        echo "inline assembly outside architecture headers:" >&2
        echo "$inline_assembly" >&2
        exit 1
    fi
fi

for symbol in _start kernel_main interrupt_dispatch memory_init scheduler_init filesystem_init; do
    nm "$kernel" | grep -q " $symbol$" || {
        echo "required kernel symbol missing: $symbol" >&2
        exit 1
    }
done

echo "ELF format, entry point, and required symbols verified"
