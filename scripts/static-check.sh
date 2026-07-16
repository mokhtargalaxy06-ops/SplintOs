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

for symbol in _start kernel_main interrupt_dispatch memory_init scheduler_init filesystem_init; do
    nm "$kernel" | grep -q " $symbol$" || {
        echo "required kernel symbol missing: $symbol" >&2
        exit 1
    }
done

echo "ELF format, entry point, and required symbols verified"
