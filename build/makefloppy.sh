#!/bin/bash
# makefloppy.sh - Create a DOS FAT12 floppy image from a local directory using mtools

# Usage: ./makefloppy.sh /path/to/local/dir floppy.img

set -e

if [ $# -lt 2 ]; then
  echo "Usage: $0 <source_directory> <floppy_image>"
  exit 1
fi

SRC_DIR="$1"
IMG="$2"

# 1. Create empty 1.44MB floppy image
dd if=/dev/zero of="$IMG" bs=1024 count=1440

# 2. Format with FAT12
mkfs.fat -F 12 "$IMG"

# 3. Copy files into floppy using mtools
# mtools doesn't need mounting; we pass -i <image>
mcopy -i "$IMG" -s "$SRC_DIR"/* ::

echo "Floppy image $IMG created with contents of $SRC_DIR"
