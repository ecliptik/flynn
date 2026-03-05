#!/usr/bin/env python3
"""Extract the Apple SCSI driver from Apple HD SC Setup (MacBinary format)."""

import struct
import sys

def parse_macbinary(filename):
    """Parse a MacBinary file and return data fork, resource fork."""
    with open(filename, 'rb') as f:
        header = f.read(128)

    # MacBinary header fields
    version = header[0]
    name_len = header[1]
    name = header[2:2+name_len].decode('mac-roman', errors='replace')
    file_type = header[65:69].decode('ascii', errors='replace')
    file_creator = header[69:73].decode('ascii', errors='replace')

    data_fork_len = struct.unpack('>I', header[83:87])[0]
    rsrc_fork_len = struct.unpack('>I', header[87:91])[0]

    print(f"File: {name}")
    print(f"Type: {file_type}, Creator: {file_creator}")
    print(f"Data fork: {data_fork_len} bytes")
    print(f"Resource fork: {rsrc_fork_len} bytes")

    with open(filename, 'rb') as f:
        f.seek(128)
        data_fork = f.read(data_fork_len)
        # Pad to 128-byte boundary
        padding = (128 - (data_fork_len % 128)) % 128
        f.seek(padding, 1)
        rsrc_fork = f.read(rsrc_fork_len)

    return data_fork, rsrc_fork

def parse_resource_fork(rsrc):
    """Parse an Apple resource fork and list all resources."""
    if len(rsrc) < 16:
        print("Resource fork too small")
        return {}

    # Resource fork header
    data_offset = struct.unpack('>I', rsrc[0:4])[0]
    map_offset = struct.unpack('>I', rsrc[4:8])[0]
    data_length = struct.unpack('>I', rsrc[8:12])[0]
    map_length = struct.unpack('>I', rsrc[12:16])[0]

    print(f"\nResource fork header:")
    print(f"  Data offset: {data_offset}, length: {data_length}")
    print(f"  Map offset: {map_offset}, length: {map_length}")

    # Resource map
    map_data = rsrc[map_offset:]
    if len(map_data) < 28:
        print("Resource map too small")
        return {}

    # Skip copy of header (16 bytes) + next resource map handle (4 bytes) + file ref (2 bytes)
    map_attrs = struct.unpack('>H', map_data[22:24])[0]
    type_list_offset = struct.unpack('>H', map_data[24:26])[0]
    name_list_offset = struct.unpack('>H', map_data[26:28])[0]

    print(f"  Map attrs: 0x{map_attrs:04X}")
    print(f"  Type list offset: {type_list_offset}")
    print(f"  Name list offset: {name_list_offset}")

    # Type list starts at map_offset + type_list_offset
    type_list = map_data[type_list_offset:]
    num_types = struct.unpack('>h', type_list[0:2])[0] + 1  # -1 based count

    print(f"\n  Number of resource types: {num_types}")

    resources = {}
    for i in range(num_types):
        offset = 2 + i * 8
        rtype = type_list[offset:offset+4].decode('mac-roman', errors='replace')
        num_resources = struct.unpack('>h', type_list[offset+4:offset+6])[0] + 1
        ref_list_offset = struct.unpack('>H', type_list[offset+6:offset+8])[0]

        resources[rtype] = []
        print(f"\n  Type '{rtype}': {num_resources} resource(s)")

        # Reference list
        for j in range(num_resources):
            ref_offset = ref_list_offset + j * 12
            ref_data = type_list[ref_offset:]
            if len(ref_data) < 12:
                break

            res_id = struct.unpack('>h', ref_data[0:2])[0]
            name_offset = struct.unpack('>h', ref_data[2:4])[0]
            attrs_and_data = struct.unpack('>I', ref_data[4:8])[0]
            res_attrs = (attrs_and_data >> 24) & 0xFF
            res_data_offset = attrs_and_data & 0x00FFFFFF

            # Get actual data from data section
            abs_data_offset = data_offset + res_data_offset
            if abs_data_offset + 4 <= len(rsrc):
                res_data_len = struct.unpack('>I', rsrc[abs_data_offset:abs_data_offset+4])[0]
                res_data = rsrc[abs_data_offset+4:abs_data_offset+4+res_data_len]
            else:
                res_data_len = 0
                res_data = b''

            # Get name if present
            name = ""
            if name_offset != -1:
                abs_name_offset = map_offset + name_list_offset + name_offset
                if abs_name_offset < len(rsrc):
                    nlen = rsrc[abs_name_offset]
                    name = rsrc[abs_name_offset+1:abs_name_offset+1+nlen].decode('mac-roman', errors='replace')

            resources[rtype].append({
                'id': res_id,
                'name': name,
                'size': res_data_len,
                'data': res_data,
                'attrs': res_attrs,
            })
            print(f"    ID {res_id}: {res_data_len} bytes, name='{name}', attrs=0x{res_attrs:02X}")
            if res_data_len > 0 and res_data_len < 50:
                print(f"      Data: {res_data[:50].hex()}")

    return resources

def main():
    macbin_file = '/tmp/hdsc/Apple_HD_SC_Setup.bin'

    print("=== Parsing MacBinary ===")
    data_fork, rsrc_fork = parse_macbinary(macbin_file)

    print("\n=== Parsing Resource Fork ===")
    resources = parse_resource_fork(rsrc_fork)

    # Look for SCSI driver resources
    print("\n\n=== Looking for SCSI driver ===")
    driver_types = ['DRVR', 'drv ', 'drv%', 'scsi', 'SCSI', 'drvr', 'wdgt', 'boot']
    for dtype in driver_types:
        if dtype in resources:
            print(f"Found potential driver type '{dtype}':")
            for res in resources[dtype]:
                print(f"  ID {res['id']}: {res['size']} bytes, name='{res['name']}'")
                if res['size'] > 1000:
                    # Save large resources as potential drivers
                    outfile = f"/tmp/hdsc/driver_{dtype}_{res['id']}.bin"
                    with open(outfile, 'wb') as f:
                        f.write(res['data'])
                    print(f"  Saved to {outfile}")

    # Also dump all large resources
    print("\n=== All resources > 4KB ===")
    for rtype, res_list in resources.items():
        for res in res_list:
            if res['size'] > 4096:
                print(f"  Type '{rtype}' ID {res['id']}: {res['size']} bytes, name='{res['name']}'")
                outfile = f"/tmp/hdsc/resource_{rtype.replace('/', '_')}_{res['id']}.bin"
                with open(outfile, 'wb') as f:
                    f.write(res['data'])
                print(f"  Saved to {outfile}")

if __name__ == '__main__':
    main()
