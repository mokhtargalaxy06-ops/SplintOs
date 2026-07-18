#!/bin/sh
set -eu

log32=${1:?usage: behavior-equivalence.sh LOG32 MISSING32 LOG64}
missing32=${2:?usage: behavior-equivalence.sh LOG32 MISSING32 LOG64}
log64=${3:?usage: behavior-equivalence.sh LOG32 MISSING32 LOG64}

require() {
    file=$1 pattern=$2 contract=$3
    grep -q "$pattern" "$file" || {
        echo "behavior-equivalence contract failed: $contract" >&2
        echo "missing '$pattern' in $file" >&2
        exit 1
    }
}

require "$log32" 'physical allocator ownership online' '32-bit physical ownership'
require "$log64" 'x86_64 physical allocator ownership online' '64-bit physical ownership'
require "$log32" 'kernel heap exhaustion and reuse online' '32-bit heap reuse'
require "$log64" 'x86_64 kernel heap exhaustion and reuse online' '64-bit heap reuse'
require "$log32" 'preemptive scheduler online' '32-bit timer preemption'
require "$log64" 'x86_64 PIC, PIT and preemptive scheduler online' '64-bit timer preemption'
require "$log32" 'PCI and ACPI discovery complete' '32-bit PCI/ACPI'
require "$log64" 'x86_64 PCI and ACPI discovery online' '64-bit PCI/ACPI'
require "$log32" 'generic block layer, cache and ramblk0 online' '32-bit bounded block I/O'
require "$log64" 'x86_64 bounded block layer online' '64-bit bounded block I/O'
require "$log32" 'PS/2 mouse online' '32-bit PS/2 input'
require "$log64" 'x86_64 PS/2 keyboard IRQ online' '64-bit keyboard input'
require "$log64" 'x86_64 PS/2 mouse IRQ online' '64-bit mouse input'
require "$log32" 'framebuffer compositor online' '32-bit framebuffer compositor'
require "$log64" 'x86_64 framebuffer compositor online' '64-bit framebuffer compositor'
require "$log32" 'disk: multi-sector VFS persistence online' '32-bit VFS persistence'
require "$log64.storage-second" 'x86_64 persistent VFS recovered' '64-bit VFS persistence'
require "$log32" 'RTL8139 DMA and IRQ online' '32-bit RTL8139'
require "$log64.network" 'x86_64 RTL8139 RX, TX DMA and IRQ online' '64-bit RTL8139'
require "$log64.network" 'x86_64 RTL8139 DHCP configured' '64-bit DHCP'
require "$missing32" 'RTL8139 unavailable; networking disabled' '32-bit missing NIC fallback'
require "$missing32" 'VirtIO block absent; using ramblk0' '32-bit missing storage fallback'
require "$log32" 'SplintOS Ring 3 shell online' 'preserved 32-bit userspace baseline'

if grep -q 'KERNEL PANIC\|fatal x86_64 exception' "$log32" "$missing32" \
    "$log64" "$log64.network" "$log64.storage" "$log64.storage-second"; then
    echo "behavior-equivalence profile contained a fatal kernel event" >&2
    exit 1
fi

echo "Mapped 32-bit/x86_64 foundation behavior equivalence verified"
