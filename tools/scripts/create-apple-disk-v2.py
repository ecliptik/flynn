#!/usr/bin/env python3
"""Create a properly formatted Apple SCSI disk image with DDM, driver, partition map, and HFS."""

import struct
import subprocess
import sys
import os

BLOCK_SIZE = 512

def main():
    output = '/home/claude/git/telnet-m68k/diskimages/snow-sys608.img'
    driver_file = '/tmp/hdsc/driver_scsi_128.bin'
    size_mb = 40
    total_blocks = (size_mb * 1024 * 1024) // BLOCK_SIZE  # 81920

    # Read the SCSI driver
    with open(driver_file, 'rb') as f:
        driver_data = f.read()
    driver_blocks = (len(driver_data) + BLOCK_SIZE - 1) // BLOCK_SIZE  # Round up
    print(f"SCSI driver: {len(driver_data)} bytes ({driver_blocks} blocks)")

    # Partition layout:
    # Block 0: DDM (Driver Descriptor Map)
    # Blocks 1-4: Partition Map (4 entries: map itself, driver, free, HFS)
    # Blocks 5-14: Padding (ensure driver starts at block 64 for alignment)
    # Blocks 64-73: Apple_Driver43 partition (driver, 10 blocks = 5120 bytes)
    # Blocks 74-81919: Apple_HFS partition

    pm_entries = 4  # partition map entries
    pm_start = 1
    driver_start = 64
    driver_part_blocks = max(driver_blocks, 10)  # At least 10 blocks
    hfs_start = driver_start + driver_part_blocks
    hfs_blocks = total_blocks - hfs_start

    print(f"Creating Apple disk image: {output}")
    print(f"  Total: {total_blocks} blocks ({size_mb} MB)")
    print(f"  Partition map: blocks {pm_start}-{pm_start + pm_entries - 1}")
    print(f"  Driver: blocks {driver_start}-{driver_start + driver_part_blocks - 1}")
    print(f"  HFS: blocks {hfs_start}-{total_blocks - 1} ({hfs_blocks} blocks)")

    image = bytearray(total_blocks * BLOCK_SIZE)

    # === Block 0: DDM ===
    ddm = image[0:BLOCK_SIZE]
    struct.pack_into('>H', ddm, 0, 0x4552)     # Signature "ER"
    struct.pack_into('>H', ddm, 2, BLOCK_SIZE)  # Block size
    struct.pack_into('>I', ddm, 4, total_blocks) # Total blocks
    struct.pack_into('>H', ddm, 8, 1)           # Device type
    struct.pack_into('>H', ddm, 10, 1)          # Device ID
    struct.pack_into('>I', ddm, 12, 1)          # Number of driver entries

    # Driver descriptor entry (8 bytes each, starting at offset 16)
    # ddBlock (4 bytes): starting block of driver
    # ddSize (2 bytes): size of driver in blocks
    # ddType (2 bytes): operating system type (1 = Macintosh)
    struct.pack_into('>I', ddm, 16, driver_start)      # Driver start block
    struct.pack_into('>H', ddm, 20, driver_part_blocks) # Driver size in blocks
    struct.pack_into('>H', ddm, 22, 1)                  # Type 1 = Macintosh

    # === Partition Map Entries ===
    def write_pm(block_idx, start, count, name, ptype, status=0x33,
                 data_start=0, data_count=None):
        """Write a partition map entry at the given block index."""
        offset = block_idx * BLOCK_SIZE
        pm = image[offset:offset + BLOCK_SIZE]
        struct.pack_into('>H', pm, 0, 0x504D)     # Signature "PM"
        struct.pack_into('>H', pm, 2, 0)           # Reserved
        struct.pack_into('>I', pm, 4, pm_entries)  # Total partitions
        struct.pack_into('>I', pm, 8, start)       # Physical start block
        struct.pack_into('>I', pm, 12, count)      # Block count
        # Partition name (32 bytes)
        name_b = name.encode('ascii')[:31]
        pm[16:16+len(name_b)] = name_b
        # Partition type (32 bytes)
        type_b = ptype.encode('ascii')[:31]
        pm[48:48+len(type_b)] = type_b
        # Data area within partition
        struct.pack_into('>I', pm, 80, data_start)
        struct.pack_into('>I', pm, 84, data_count if data_count is not None else count)
        # Status
        struct.pack_into('>I', pm, 88, status)
        # Boot info (for driver partition)
        # pmBootSize (at offset 92) = size of boot code in bytes
        # pmBootAddr (at offset 96) = load address
        # pmBootEntry (at offset 104) = entry point (offset from load address)

    # Entry 1: Partition map itself
    write_pm(1, pm_start, pm_entries, "Apple", "Apple_partition_map", status=0x03)

    # Entry 2: Driver partition
    # Status 0x7F = valid, allocated, in use, readable, writable, boot, position independent
    write_pm(2, driver_start, driver_part_blocks, "Macintosh",
             "Apple_Driver43", status=0x7F,
             data_start=0, data_count=driver_part_blocks)
    # Set boot code size and load address for the driver
    pm_offset = 2 * BLOCK_SIZE
    struct.pack_into('>I', image, pm_offset + 92, len(driver_data))  # pmBootSize
    struct.pack_into('>I', image, pm_offset + 96, 0)                 # pmBootAddr (loaded by ROM)
    struct.pack_into('>I', image, pm_offset + 100, 0)                # reserved
    struct.pack_into('>I', image, pm_offset + 104, 0)                # pmBootEntry
    struct.pack_into('>I', image, pm_offset + 108, 0)                # reserved
    struct.pack_into('>I', image, pm_offset + 112, len(driver_data)) # pmBootCksum (or size)

    # Entry 3: Free space between partition map and driver
    free_start = pm_start + pm_entries
    free_count = driver_start - free_start
    write_pm(3, free_start, free_count, "Extra", "Apple_Free", status=0x00)

    # Entry 4: HFS data partition
    write_pm(4, hfs_start, hfs_blocks, "Macintosh HD", "Apple_HFS",
             status=0x40000033)

    # === Write the SCSI driver to the driver partition ===
    drv_offset = driver_start * BLOCK_SIZE
    image[drv_offset:drv_offset + len(driver_data)] = driver_data

    # === Write the image ===
    with open(output, 'wb') as f:
        f.write(image)
    print(f"Written {os.path.getsize(output)} bytes")

    # === Format the HFS partition ===
    hfs_offset = hfs_start * BLOCK_SIZE
    hfs_size = hfs_blocks * BLOCK_SIZE

    temp_hfs = '/tmp/temp_hfs_part.img'
    with open(temp_hfs, 'wb') as f:
        f.write(b'\x00' * hfs_size)

    print(f"Formatting HFS partition ({hfs_blocks * 512 // 1024 // 1024} MB)...")
    result = subprocess.run(['hformat', '-l', 'Macintosh HD', temp_hfs],
                          capture_output=True, text=True)
    print(f"hformat: {result.stdout.strip()}")
    if result.returncode != 0:
        print(f"hformat error: {result.stderr}")
        return 1

    # Write formatted HFS back into the image
    with open(temp_hfs, 'rb') as f:
        hfs_data = f.read()
    with open(output, 'r+b') as f:
        f.seek(hfs_offset)
        f.write(hfs_data)

    os.unlink(temp_hfs)
    print("Done!")
    return 0

if __name__ == '__main__':
    sys.exit(main())
