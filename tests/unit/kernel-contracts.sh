#!/bin/sh
set -eu

iso=${1:?usage: kernel-contracts.sh ISO LOG}
log=${2:?usage: kernel-contracts.sh ISO LOG}
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)

"$root/tests/integration/boot.sh" "$iso" "$log"

for fixture in \
    'checked boot memory-map fixtures online' \
    'kernel heap exhaustion and reuse online' \
    'physical allocator ownership online' \
    'bounded boot log online' \
    'checked wall-clock conversion online' \
    'deterministic block write faults online' \
    'diskfs interrupted commit rejection online' \
    'diskfs full-disk reclamation online' \
    'diskfs explicit legacy migration online' \
    'pipe: blocking transfer and EOF online' \
    'heap: allocation reuse and coalescing online'; do
    grep -q "$fixture" "$log" || {
        echo "kernel contract fixture did not pass: $fixture" >&2
        exit 1
    }
done

echo "Deterministic kernel contract fixtures verified"
