#!/bin/bash
# Restore clean ext4 test images from backup
IMGS=(
    "alpine-linux-riscv64-ext4fs.img"
    "sdcard-rv.img"
)

cd "$(dirname "$0")"

for IMG in "${IMGS[@]}"; do
    BAK="${IMG}.bak"

    if [ ! -f "$BAK" ]; then
        echo "Creating backup from current image: $BAK"
        cp "$IMG" "$BAK"
        echo "Backup created. Image is already clean."
        continue
    fi

    echo "Restoring $IMG from $BAK ..."
    cp "$BAK" "$IMG"
    echo "Done."
done
