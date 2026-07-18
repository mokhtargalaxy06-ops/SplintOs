#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_tool=${MAKE:-make}
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

build_and_hash()
{
    output=$1
    "$build_tool" -C "$root" clean >/dev/null
    "$build_tool" -C "$root" all check-user >/dev/null
    find "$root/build" -type f \( -name 'splintos.bin' -o -name '*.elf' \) \
        -print | LC_ALL=C sort | while IFS= read -r artifact; do
            sha256sum "$artifact" | sed "s|$root/||"
        done > "$output"
}

build_and_hash "$temporary/first"
build_and_hash "$temporary/second"
if ! cmp -s "$temporary/first" "$temporary/second"; then
    echo "clean builds produced different kernel or userspace artifacts" >&2
    diff -u "$temporary/first" "$temporary/second" >&2 || true
    exit 1
fi
echo "Kernel and userspace clean builds are reproducible"
