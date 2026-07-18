#!/bin/sh
set -eu

iso=${1:?usage: recovery.sh ISO LOG}
log=${2:?usage: recovery.sh ISO LOG}
qemu=${QEMU:-qemu-system-i386}
rm -f "$log"
status=0
# shellcheck disable=SC2086
timeout 8 $qemu -cdrom "$iso" -nic none -display none -monitor none \
    -serial "file:$log" -no-reboot -no-shutdown || status=$?
if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then
    echo "QEMU exited unexpectedly with status $status" >&2
    exit "$status"
fi
for milestone in \
    'SplintOS: recovery console selected' \
    'SplintOS command shell' \
    'Type help for commands.' \
    'splint> '; do
    grep -q "$milestone" "$log" || {
        echo "recovery log did not contain: $milestone" >&2
        sed -n '1,180p' "$log" >&2
        exit 1
    }
done
if grep -q 'SplintOS Ring 3 shell online' "$log"; then
    echo "normal Ring 3 startup ran during recovery boot" >&2
    exit 1
fi
if grep -q 'KERNEL PANIC' "$log"; then
    echo "kernel panic detected during recovery boot" >&2
    exit 1
fi
echo "Recovery-mode headless boot verified"
