#!/bin/sh
set -eu
iso=${1:?usage: x86_64-boot.sh ISO LOG}
log=${2:?usage: x86_64-boot.sh ISO LOG}
qemu=${QEMU64:-qemu-system-x86_64}
rm -f "$log"
status=0
# shellcheck disable=SC2086
timeout 5 $qemu -cpu qemu64 -cdrom "$iso" -nic none -display none -monitor none \
    -serial "file:$log" -no-reboot -no-shutdown || status=$?
if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then exit "$status"; fi
for milestone in \
    'x86_64 CPUID, PAE, long mode and NX verified' \
    'x86_64 temporary paging and trampoline online' \
    'x86_64 GDT, TSS, RSP0 and IST stacks online' \
    'x86_64 IDT and exception return online' \
    'x86_64 four-level paging owner online' \
    'x86_64 physical allocator ownership online' \
    'x86_64 kernel heap exhaustion and reuse online' \
    'x86_64 canonical layout online' \
    'x86_64 syscall ABI v1 contract online' \
    'x86_64 syscall entry and safe return online' \
    'x86_64 hostile user pointers rejected' \
    'x86_64 PIC, PIT and preemptive scheduler online' \
    'x86_64 PCI and ACPI discovery online' \
    'x86_64 bounded block layer online' \
    'x86_64 low DMA and bounce buffers online' \
    'x86_64 bounded input queue online' \
    'x86_64 PS/2 keyboard IRQ online' \
    'x86_64 PS/2 mouse IRQ online' \
    'x86_64 bounded VFS online' \
    'x86_64 bounded UDP core online' \
    'x86_64 framebuffer compositor online'; do
    grep -q "$milestone" "$log" || {
        echo "x86_64 boot log did not contain: $milestone" >&2
        sed -n '1,120p' "$log" >&2
        exit 1
    }
done
network_log="$log.network"
rm -f "$network_log"
status=0
timeout 5 $qemu -cpu qemu64 -cdrom "$iso" -nic user,model=rtl8139 \
    -display none -monitor none -serial "file:$network_log" -no-reboot \
    -no-shutdown || status=$?
if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then exit "$status"; fi
grep -q 'x86_64 RTL8139 RX, TX DMA and IRQ online' "$network_log" || {
    echo "x86_64 RTL8139 hardware profile failed" >&2
    sed -n '1,160p' "$network_log" >&2
    exit 1
}
grep -q 'x86_64 RTL8139 DHCP configured' "$network_log" || {
    echo "x86_64 RTL8139 DHCP profile failed" >&2
    sed -n '1,180p' "$network_log" >&2
    exit 1
}
disk_log="$log.storage"
disk_image="$log.storage.img"
rm -f "$disk_log" "$disk_image"
truncate -s 4M "$disk_image"
status=0
timeout 5 $qemu -cpu qemu64 -cdrom "$iso" -nic none -display none -monitor none \
    -serial "file:$disk_log" -no-reboot -no-shutdown \
    -drive if=none,id=spldisk,file="$disk_image",format=raw \
    -device virtio-blk-pci,drive=spldisk,disable-modern=on || status=$?
if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then exit "$status"; fi
grep -q 'x86_64 persistent VirtIO block online' "$disk_log" || {
    echo "x86_64 VirtIO storage profile failed" >&2
    sed -n '1,180p' "$disk_log" >&2
    exit 1
}
grep -q 'x86_64 persistent VFS created' "$disk_log" || {
    echo "x86_64 persistent VFS creation failed" >&2
    sed -n '1,180p' "$disk_log" >&2
    exit 1
}
disk_log_second="$log.storage-second"
rm -f "$disk_log_second"
status=0
timeout 5 $qemu -cpu qemu64 -cdrom "$iso" -nic none -display none -monitor none \
    -serial "file:$disk_log_second" -no-reboot -no-shutdown \
    -drive if=none,id=spldisk,file="$disk_image",format=raw \
    -device virtio-blk-pci,drive=spldisk,disable-modern=on || status=$?
if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then exit "$status"; fi
grep -q 'x86_64 persistent VFS recovered' "$disk_log_second" || {
    echo "x86_64 persistent VFS second-boot recovery failed" >&2
    sed -n '1,180p' "$disk_log_second" >&2
    exit 1
}
echo "x86_64 long-mode foundation boot verified"
