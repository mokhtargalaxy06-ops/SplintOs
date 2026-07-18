#!/bin/sh
set -eu

iso=${1:?usage: boot-test.sh ISO LOG}
log=${2:?usage: boot-test.sh ISO LOG [rtl8139|none]}
network=${3:-rtl8139}
rm -f "$log"

case "$network" in
    rtl8139) nic_args='-nic user,model=rtl8139' ;;
    none) nic_args='-nic none' ;;
    *) echo "unsupported boot-test network profile: $network" >&2; exit 2 ;;
esac

status=0
qemu=${QEMU:-qemu-system-i386}
# QEMU may be a launcher plus executable, so intentional word splitting is used.
# shellcheck disable=SC2086
timeout 8 $qemu \
    -cdrom "$iso" \
    $nic_args \
    -display none \
    -monitor none \
    -serial "file:$log" \
    -no-reboot \
    -no-shutdown || status=$?

if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then
    echo "QEMU exited unexpectedly with status $status" >&2
    exit "$status"
fi

for message in \
    'physical allocator, paging and heap online' \
    'checked boot memory-map fixtures online' \
    'kernel heap exhaustion and reuse online' \
    'physical allocator ownership online' \
    'bounded boot log online' \
    'checked wall-clock conversion online' \
    'PCI and ACPI discovery complete' \
    'generic block layer, cache and ramblk0 online' \
    'deterministic block write faults online' \
    'validated MBR partition discovered' \
    'diskfs format, flush and remount online' \
    'diskfs interrupted commit rejection online' \
    'diskfs full-disk reclamation online' \
    'diskfs explicit legacy migration online' \
    'preemptive scheduler online' \
    'application runtime online' \
    'GDT, TSS, IDT, PIC and PIT initialized' \
    'ELF loader online' \
    'loaded /bin/hello ELF32 process' \
    'loaded /bin/sh ELF32 process' \
    'loaded /bin/cat ELF32 process' \
    'hello from ELF user space' \
    'SplintOS in-memory filesystem is online.' \
    'SplintOS Ring 3 shell online' \
    'shell: startup command status=0' \
    'dup2: shared descriptor online' \
    'inherited standard output' \
    'child output crossed a pipe' \
    'pipe: blocking transfer and EOF online' \
    'redirected output' \
    'shell: redirection status=0' \
    'wc: bytes=15' \
    'shell: pipeline status=0' \
    'heaptest' \
    'memory: total=' \
    'uptime: ' \
    'processes: active=' \
    'heap: allocation reuse and coalescing online' \
    'disk: multi-sector VFS persistence online' \
    'user> ' \
    'ELF user process exited status=0'; do
    grep -q "$message" "$log" || {
        echo "boot log did not contain: $message" >&2
        sed -n '1,160p' "$log" >&2
        exit 1
    }
done

if [ "$network" = none ]; then
    grep -q 'RTL8139 unavailable; networking disabled' "$log" || {
        echo "missing-device boot did not report disabled networking" >&2
        exit 1
    }
    grep -q 'VirtIO block absent; using ramblk0' "$log" || {
        echo "missing-device boot did not report volatile storage fallback" >&2
        exit 1
    }
fi

if [ "$(grep -c 'ELF user process exited status=0' "$log")" -ne 23 ]; then
    echo "the finite ELF user processes did not exit successfully" >&2
    sed -n '1,160p' "$log" >&2
    exit 1
fi

if grep -q 'KERNEL PANIC' "$log"; then
    echo "kernel panic detected during boot" >&2
    exit 1
fi

echo "Headless QEMU boot and serial milestones verified"
