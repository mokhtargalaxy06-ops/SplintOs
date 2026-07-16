#!/bin/sh
set -eu

iso=${1:?usage: storage-test.sh ISO IMAGE LOG_PREFIX}
image=${2:?usage: storage-test.sh ISO IMAGE LOG_PREFIX}
prefix=${3:?usage: storage-test.sh ISO IMAGE LOG_PREFIX}
qemu=${QEMU:-qemu-system-i386}

run_boot()
{
    log=$1
    status=0
    # QEMU may be a launcher plus executable, so intentional splitting is used.
    # shellcheck disable=SC2086
    timeout 8 $qemu \
        -boot d \
        -cdrom "$iso" \
        -drive "file=$image,if=none,format=raw,id=vdisk" \
        -device virtio-blk-pci,drive=vdisk,disable-modern=on \
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
    grep -q 'legacy VirtIO block device online' "$log"
    grep -q 'disk: multi-sector VFS persistence online' "$log"
    if grep -q 'KERNEL PANIC' "$log"; then
        echo "kernel panic detected during storage test" >&2
        exit 1
    fi
}

run_unknown_probe()
{
    unknown=$1
    log=$2
    status=0
    before=$(sha256sum "$unknown" | cut -d ' ' -f 1)
    # shellcheck disable=SC2086
    timeout 8 $qemu \
        -boot d \
        -cdrom "$iso" \
        -drive "file=$unknown,if=none,format=raw,id=unknown" \
        -device virtio-blk-pci,drive=unknown,disable-modern=on \
        -nic user,model=rtl8139 \
        -display none \
        -monitor none \
        -serial "file:$log" \
        -no-reboot \
        -no-shutdown || status=$?
    if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then exit "$status"; fi
    grep -q 'legacy VirtIO block device online' "$log"
    grep -q 'diskfs unavailable' "$log"
    after=$(sha256sum "$unknown" | cut -d ' ' -f 1)
    if [ "$before" != "$after" ]; then
        echo "unknown disk was modified during probe" >&2
        exit 1
    fi
}

"$(dirname "$0")/create-test-disk.sh" "$image"
run_boot "${prefix}-first.log"
grep -q 'existing diskfs mounted on virtblk0' "${prefix}-first.log"
run_boot "${prefix}-second.log"
grep -q 'existing diskfs mounted on virtblk0' "${prefix}-second.log"
unknown="${image}.unknown"
"$(dirname "$0")/create-test-disk.sh" "$unknown" unknown
run_unknown_probe "$unknown" "${prefix}-unknown.log"
overlap="${image}.overlap"
"$(dirname "$0")/create-test-disk.sh" "$overlap" overlap
run_unknown_probe "$overlap" "${prefix}-overlap.log"
outside="${image}.outside"
"$(dirname "$0")/create-test-disk.sh" "$outside" out-of-range
run_unknown_probe "$outside" "${prefix}-outside.log"
checksum="${image}.checksum"
"$(dirname "$0")/create-test-disk.sh" "$checksum" checksum
run_unknown_probe "$checksum" "${prefix}-checksum.log"
echo "VirtIO persistence and non-destructive corruption rejection verified"
