#!/bin/sh
set -eu

iso=${1:?usage: boot-test.sh ISO LOG}
log=${2:?usage: boot-test.sh ISO LOG}
rm -f "$log"

status=0
qemu=${QEMU:-qemu-system-i386}
# QEMU may be a launcher plus executable, so intentional word splitting is used.
# shellcheck disable=SC2086
timeout 8 $qemu \
    -cdrom "$iso" \
    -nic user,model=rtl8139 \
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
    'PCI and ACPI discovery complete' \
    'preemptive scheduler online' \
    'application runtime online' \
    'GDT, IDT, PIC and PIT initialized'; do
    grep -q "$message" "$log" || {
        echo "boot log did not contain: $message" >&2
        sed -n '1,160p' "$log" >&2
        exit 1
    }
done

if grep -q 'KERNEL PANIC' "$log"; then
    echo "kernel panic detected during boot" >&2
    exit 1
fi

echo "Headless QEMU boot and serial milestones verified"
