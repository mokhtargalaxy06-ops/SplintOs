#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
for path in src src/arch/x86_64 include include/arch/x86 user user/libc user/programs scripts tests docs grub; do
    if [ ! -d "$root/$path" ]; then
        echo "required repository directory missing: $path" >&2
        exit 1
    fi
done
for path in Makefile linker.ld user/linker.ld; do
    if [ ! -f "$root/$path" ]; then
        echo "required repository file missing: $path" >&2
        exit 1
    fi
done
echo "Repository layout verified"
