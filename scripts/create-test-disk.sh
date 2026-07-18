#!/bin/sh
set -eu

image=${1:?usage: create-test-disk.sh IMAGE}
mode=${2:-formatted}
rm -f "$image"
truncate -s 8M "$image"

# One Linux-type MBR partition: LBA 8, 128 sectors.
printf '\203' | dd of="$image" bs=1 seek=450 conv=notrunc status=none
printf '\010\000\000\000' | dd of="$image" bs=1 seek=454 conv=notrunc status=none
printf '\200\000\000\000' | dd of="$image" bs=1 seek=458 conv=notrunc status=none
printf '\125\252' | dd of="$image" bs=1 seek=510 conv=notrunc status=none

if [ "$mode" = "formatted" ] || [ "$mode" = "overlap" ] ||
   [ "$mode" = "out-of-range" ] || [ "$mode" = "checksum" ] ||
   [ "$mode" = "bitmap" ] || [ "$mode" = "dirty" ] ||
   [ "$mode" = "dirty-directory" ] || [ "$mode" = "dirty-bitmap" ]; then
    # SPLFS5 geometry, allocation-bitmap feature, clean state, and checksums.
    printf 'SPLFS5\000\000\005\000\000\000\010\000\000\000\010\000\000\000\001\000\000\000\002\000\000\000\003\000\000\000\001\000\000\000\000\000\000\000\305\005\167\115\322\200\223\065' |
        dd of="$image" bs=1 seek=4096 conv=notrunc status=none
    # Sectors 0..2 and all bits beyond the 128-sector partition are reserved.
    printf '\007' | dd of="$image" bs=1 seek=5120 conv=notrunc status=none
    dd if=/dev/zero bs=1 count=496 status=none | tr '\000' '\377' |
        dd of="$image" bs=1 seek=5136 conv=notrunc status=none
elif [ "$mode" = "legacy" ]; then
    # Empty, clean SPLFS4 volume with the same geometry and bitmap contract.
    printf 'SPLFS4\000\000\004\000\000\000\010\000\000\000\010\000\000\000\001\000\000\000\002\000\000\000\003\000\000\000\001\000\000\000\000\000\000\000\305\005\167\115\322\200\223\065' |
        dd of="$image" bs=1 seek=4096 conv=notrunc status=none
    printf '\007' | dd of="$image" bs=1 seek=5120 conv=notrunc status=none
    dd if=/dev/zero bs=1 count=496 status=none | tr '\000' '\377' |
        dd of="$image" bs=1 seek=5136 conv=notrunc status=none
elif [ "$mode" != "unknown" ]; then
    echo "unknown disk fixture mode: $mode" >&2
    exit 1
fi

# Directory sector starts at absolute LBA 9. SPLFS5 entries are 56 bytes.
if [ "$mode" = "overlap" ]; then
    printf 'alpha' | dd of="$image" bs=1 seek=4608 conv=notrunc status=none
    printf '\000\002\000\000\002\000\000\000\001\000\000\000' |
        dd of="$image" bs=1 seek=4640 conv=notrunc status=none
    printf 'beta' | dd of="$image" bs=1 seek=4664 conv=notrunc status=none
    printf '\000\002\000\000\002\000\000\000\001\000\000\000' |
        dd of="$image" bs=1 seek=4696 conv=notrunc status=none
elif [ "$mode" = "out-of-range" ]; then
    printf 'escape' | dd of="$image" bs=1 seek=4608 conv=notrunc status=none
    printf '\000\002\000\000\200\000\000\000\001\000\000\000' |
        dd of="$image" bs=1 seek=4640 conv=notrunc status=none
elif [ "$mode" = "checksum" ]; then
    printf '\001' | dd of="$image" bs=1 seek=5119 conv=notrunc status=none
elif [ "$mode" = "bitmap" ]; then
    printf '\017' | dd of="$image" bs=1 seek=5120 conv=notrunc status=none
elif [ "$mode" = "dirty" ]; then
    printf '\001\000\000\000' | dd of="$image" bs=1 seek=4132 conv=notrunc status=none
elif [ "$mode" = "dirty-directory" ]; then
    printf '\001\000\000\000' | dd of="$image" bs=1 seek=4132 conv=notrunc status=none
    printf '\001' | dd of="$image" bs=1 seek=5119 conv=notrunc status=none
elif [ "$mode" = "dirty-bitmap" ]; then
    printf '\001\000\000\000' | dd of="$image" bs=1 seek=4132 conv=notrunc status=none
    printf '\017' | dd of="$image" bs=1 seek=5120 conv=notrunc status=none
fi
