#!/bin/bash
# makefloppy-mtools.sh <source_dir> <output.img>

SRC_DIR="$1"
IMG="$2"

# Create empty 1.44MB image (1474560 bytes)
mformat -f 1440 -C -i "$IMG" ::

# Copy files from source directory into the image
mcopy -i "$IMG" -s "$SRC_DIR"/* ::
echo "Floppy image $IMG created from $SRC_DIR using mtools"

