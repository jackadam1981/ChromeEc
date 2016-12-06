#!/usr/bin/env python
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Sort the host commands in ec.RW.bin or ec.RO.bin.

  Example:
    util/host_command_sort.py ./build/kevin/ec.RW.flat \
        ./build/kevin/ec.RW.elf ./build/kevin/ec.RW.smap
"""

import binascii
import os
import re
import shutil
import struct
import sys
import tempfile

DEBUG = False

def debug_print( string ):
    if DEBUG:
        print string

def get_smap_line_num( smap_file, string ):
    line_num = 0;
    smap_file.seek(0, 0)
    for line in smap_file.readlines():
        line_num += 1
        if line.find(string) >= 0:
            return line_num
    return -1

def get_smap_offset( smap_file ):
    smap_file.seek(0, 0)
    for line in smap_file:
        if re.search("vectors", line):
            line_split = line.split()
            return int(line_split[0], 16)
    return -1

def main(argc, argv):
    assert (argc > 3), \
        "Usage: host_command_sort.py file.flat file.elf file.smap"

    ec_flat = argv[1]
    ec_elf = argv[2]
    ec_smap = argv[3]

    smap_file = open(ec_smap, "r")

    base_address = get_smap_offset(smap_file)
    if base_address < 0:
        sys.exit(0)

    hcmds_offset = get_smap_line_num(smap_file, "__hcmds")
    if hcmds_offset < 0:
        sys.exit(0)

    hcmds_end_offset = get_smap_line_num(smap_file, "__hcmds_end")
    if hcmds_end_offset < 0:
        sys.exit(0)

    """ We need at least 2 host commands to sort """
    if hcmds_end_offset - hcmds_offset < 3:
        debug_print ("Found 0 Host Commands")
        sys.exit(0)

    """ ec.RO.flat might have a header """
    header = get_smap_line_num(smap_file, "fw_header")
    if header < 0:
        header = 0
    else:
        header = 64

    """ Read all host commands into an array """
    smap_file.seek(0, 0)
    lines = smap_file.readlines()
    hc_offset_name = [];
    while (hcmds_offset < hcmds_end_offset):
        """
        lines[0] - address of host command relative to base address
        lines[1] - R, meaning read only
        lines[2] - name of host command prefixed with __host_cmd_
        """
        line = lines[hcmds_offset].split()
        if line[2].startswith("__host_cmd_EC_CMD"):
            hc_offset_name.append((int(line[0], 16) - base_address, line[2]))
        hcmds_offset += 1

    smap_file.close()


    """ Find size of a single host command """
    num = len(hc_offset_name)
    itr = 0
    hc_size = 0
    while (itr < num):
        hc1 = hc_offset_name[itr]
        hc2 = hc_offset_name[itr+1]
        if hc2[0] > hc1[0]:
            hc_size = hc2[0] - hc1[0]
            break

    if hc_size == 0:
        sys.exit(0)

    hc_start = hc_offset_name[0][0]
    hc_end = hc_offset_name[num-1][0]
    hc_len = num * hc_size
    tmpdir = tempfile.mkdtemp()

    """
    System commands that break the flat file into three section.
        FLAT FILE
        ***********
        *         *
        * start   *
        *         *
        ***********
        *         *
        * mid     * This section contains the host commands
        *         *
        ***********
        *         *
        * end     *
        *         *
        ***********
    """
    cmd1 = "dd if=%s of=%s/%s bs=1 count=%s 2>/dev/null" % \
            (ec_flat, tmpdir, "flat_section_start", hc_start + header)
    cmd2 = "dd if=%s of=%s/%s bs=1 count=%s skip=%s 2>/dev/null" % \
            (ec_flat, tmpdir, "flat_section_mid", hc_len, hc_start + header)
    cmd3 = "dd if=%s of=%s/%s bs=1 skip=%s 2>/dev/null" % \
            (ec_flat, tmpdir, "flat_section_end", hc_start + header + hc_len)

    os.system(cmd1)
    os.system(cmd2)
    os.system(cmd3)

    """
    Create a file for each host command containing its bytes and create an
    array that matches host commands to their names. The array is then
    sorted by host command.
    """
    itr = 0
    offset = 0
    cmd_array = [];
    while (itr < num):
        cmd = "dd if=%s/%s of=%s/%s bs=1 count=%s skip=%s 2>/dev/null" % \
                (tmpdir, "flat_section_mid", tmpdir, hc_offset_name[itr][1], \
                 hc_size, offset)
        os.system(cmd)

        bf = "%s/%s" % (tmpdir, hc_offset_name[itr][1])
        f = open(bf, "rb")
        try:
            f.seek(4,0)
            hcmd = struct.unpack('i', f.read(4))[0]
            cmd_array.append((hcmd, hc_offset_name[itr][1]))
        finally:
            f.close()
        itr += 1
        offset += hc_size

    """ Sort the host commands """
    sorted_cmd_array = sorted(cmd_array, key=lambda tup: tup[0])

    """ Put host commands back together in sorted order """
    itr = 0
    cmd = "cp %s/%s %s/%s" % \
        (tmpdir, sorted_cmd_array[itr][1], tmpdir, "sorted_flat_section_mid")
    os.system(cmd)
    itr += 1
    while (itr < len(sorted_cmd_array)):
        cmd = "cat %s/%s >> %s/%s" % \
            (tmpdir, sorted_cmd_array[itr][1], tmpdir,
             "sorted_flat_section_mid")
        os.system(cmd)
        itr += 1

    string = "Found %s Host Commands" % (len(sorted_cmd_array))
    debug_print(string)
    itr = 0
    while (itr < len(sorted_cmd_array)):
        debug_print(sorted_cmd_array[itr])
        itr += 1

    """ update the flat file """
    cmd1 = "cp %s/%s %s" % (tmpdir, "flat_section_start", ec_flat)
    cmd2 = "cat %s/%s >> %s" % (tmpdir, "sorted_flat_section_mid", ec_flat)
    cmd3 = "cat %s/%s >> %s" % (tmpdir, "flat_section_end", ec_flat)
    os.system(cmd1)
    os.system(cmd2)
    os.system(cmd3)

    """ All Done """
    shutil.rmtree(tmpdir)

if __name__ == "__main__":
    main(len(sys.argv), sys.argv)
