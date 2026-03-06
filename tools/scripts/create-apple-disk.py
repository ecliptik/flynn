#!/usr/bin/env python3
"""Create a properly formatted Apple SCSI disk image with DDM, partition map, and HFS."""

import struct
import subprocess
import sys
import os

BLOCK_SIZE = 512

def write_ddm(f, total_blocks, num_drivers=0):
    """Write Driver Descriptor Map at block 0."""
    ddm = bytearray(BLOCK_SIZE)
    # Signature "ER"
    struct.pack_into('>H', ddm, 0, 0x4552)
    # Block size
    struct.pack_into('>H', ddm, 2, BLOCK_SIZE)
    # Total blocks
    struct.pack_into('>I', ddm, 4, total_blocks)
    # Device type and ID (not important for emulators)
    struct.pack_into('>H', ddm, 8, 0)
    struct.pack_into('>H', ddm, 10, 0)
    # Driver count
    struct.pack_into('>I', ddm, 12, num_drivers)
    # No driver entries for now (emulator handles SCSI directly)
    f.write(ddm)

def write_partition_entry(f, total_partitions, start_block, block_count, name, ptype, status=0x33):
    """Write an Apple Partition Map entry."""
    entry = bytearray(BLOCK_SIZE)
    # Signature "PM"
    struct.pack_into('>H', entry, 0, 0x504D)
    # Reserved
    struct.pack_into('>H', entry, 2, 0)
    # Total partition entries
    struct.pack_into('>I', entry, 4, total_partitions)
    # Starting physical block
    struct.pack_into('>I', entry, 8, start_block)
    # Block count
    struct.pack_into('>I', entry, 12, block_count)
    # Partition name (32 bytes, null-terminated)
    name_bytes = name.encode('ascii')[:31]
    entry[16:16+len(name_bytes)] = name_bytes
    # Partition type (32 bytes, null-terminated)
    type_bytes = ptype.encode('ascii')[:31]
    entry[48:48+len(type_bytes)] = type_bytes
    # Data area start (logical block within partition)
    struct.pack_into('>I', entry, 80, 0)
    # Data area block count
    struct.pack_into('>I', entry, 84, block_count)
    # Status flags: valid, allocated, in use, allow read, allow write
    struct.pack_into('>I', entry, 88, status)
    f.write(entry)

def main():
    output = sys.argv[1] if len(sys.argv) > 1 else '/home/claude/git/flynn/diskimages/snow-sys608.img'
    size_mb = 40
    total_blocks = (size_mb * 1024 * 1024) // BLOCK_SIZE  # 81920

    # Partition layout:
    # Block 0: DDM
    # Blocks 1-3: Partition Map (3 entries)
    # Blocks 4-63: Free (padding, could be driver space)
    # Blocks 64-81919: HFS partition (rest of disk)

    pm_blocks = 3  # partition map entries
    hfs_start = 64
    hfs_blocks = total_blocks - hfs_start

    print(f"Creating Apple disk image: {output}")
    print(f"  Total: {total_blocks} blocks ({size_mb} MB)")
    print(f"  HFS partition: blocks {hfs_start}-{total_blocks-1} ({hfs_blocks} blocks, {hfs_blocks*512//1024//1024} MB)")

    with open(output, 'wb') as f:
        # Block 0: DDM
        write_ddm(f, total_blocks, num_drivers=0)

        # Block 1: Partition map entry for the partition map itself
        write_partition_entry(f, pm_blocks, 1, pm_blocks,
                            "Apple", "Apple_partition_map", status=0x03)

        # Block 2: Padding/free space entry (blocks 4-63)
        write_partition_entry(f, pm_blocks, pm_blocks + 1, hfs_start - pm_blocks - 1,
                            "Extra", "Apple_Free", status=0x00)

        # Block 3: HFS data partition
        write_partition_entry(f, pm_blocks, hfs_start, hfs_blocks,
                            "Macintosh HD", "Apple_HFS", status=0x40000033)

        # Fill rest of image with zeros
        remaining = total_blocks - 4  # Already wrote 4 blocks
        f.write(b'\x00' * (remaining * BLOCK_SIZE))

    print(f"  Written {os.path.getsize(output)} bytes")

    # Now format the HFS partition using hformat
    # hformat needs to work on the partition within the image
    # We'll use dd to extract, format, and re-insert
    hfs_offset = hfs_start * BLOCK_SIZE
    hfs_size = hfs_blocks * BLOCK_SIZE

    # Extract HFS partition to temp file
    temp_hfs = '/tmp/temp_hfs_part.img'
    with open(output, 'rb') as f:
        f.seek(hfs_offset)
        with open(temp_hfs, 'wb') as t:
            t.write(f.read(hfs_size))

    # Format with hformat
    print(f"  Formatting HFS partition ({hfs_blocks * 512 // 1024 // 1024} MB)...")
    result = subprocess.run(['hformat', '-l', 'Macintosh HD', temp_hfs],
                          capture_output=True, text=True)
    print(f"  hformat: {result.stdout.strip()}")
    if result.returncode != 0:
        print(f"  hformat error: {result.stderr}")
        return 1

    # Write formatted HFS partition back
    with open(output, 'r+b') as f:
        f.seek(hfs_offset)
        with open(temp_hfs, 'rb') as t:
            f.write(t.read())

    os.unlink(temp_hfs)
    print("  Done!")
    return 0

if __name__ == '__main__':
    sys.exit(main())
