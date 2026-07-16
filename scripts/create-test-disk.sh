#!/bin/sh
set -eu

image=${1:?usage: create-test-disk.sh IMAGE}
mode=${2:-formatted}
rm -f "$image"
truncate -s 8M "$image"

# One Linux-type MBR partition: LBA 8, 128 sectors. SPLFS1 formats it in QEMU.
printf '\203' | dd of="$image" bs=1 seek=450 conv=notrunc status=none
printf '\010\000\000\000' | dd of="$image" bs=1 seek=454 conv=notrunc status=none
printf '\200\000\000\000' | dd of="$image" bs=1 seek=458 conv=notrunc status=none
printf '\125\252' | dd of="$image" bs=1 seek=510 conv=notrunc status=none

if [ "$mode" = "formatted" ] || [ "$mode" = "overlap" ] ||
   [ "$mode" = "out-of-range" ] || [ "$mode" = "checksum" ]; then
    # SPLFS3 superblock and FNV-1a checksum of the zeroed directory sector.
    printf 'SPLFS3\000\000\003\000\000\000\010\000\000\000\010\000\000\000\002\000\000\000\305\005\167\115' |
        dd of="$image" bs=1 seek=4096 conv=notrunc status=none
elif [ "$mode" != "unknown" ]; then
    echo "unknown disk fixture mode: $mode" >&2
    exit 1
fi

# Directory sector starts at absolute LBA 9. Entries are 44 bytes:
# name[32], size, first sector, sector count.
if [ "$mode" = "overlap" ]; then
    printf 'alpha' | dd of="$image" bs=1 seek=4608 conv=notrunc status=none
    printf '\000\002\000\000\002\000\000\000\001\000\000\000' |
        dd of="$image" bs=1 seek=4640 conv=notrunc status=none
    printf 'beta' | dd of="$image" bs=1 seek=4652 conv=notrunc status=none
    printf '\000\002\000\000\002\000\000\000\001\000\000\000' |
        dd of="$image" bs=1 seek=4684 conv=notrunc status=none
elif [ "$mode" = "out-of-range" ]; then
    printf 'escape' | dd of="$image" bs=1 seek=4608 conv=notrunc status=none
    printf '\000\002\000\000\200\000\000\000\001\000\000\000' |
        dd of="$image" bs=1 seek=4640 conv=notrunc status=none
elif [ "$mode" = "checksum" ]; then
    printf '\001' | dd of="$image" bs=1 seek=5119 conv=notrunc status=none
fi
