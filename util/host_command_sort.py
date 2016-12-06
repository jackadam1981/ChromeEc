#!/usr/bin/env python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Sort the host commands in ec.RW.bin or ec.RO.bin.

  Example:
    util/host_command_sort.py ./build/kevin/ec.RW.flat \
        ./build/kevin/ec.RW.elf ./build/kevin/ec.RW.smap
"""

import os
import re
import shutil
import struct
import sys
import tempfile

def GetSmapLineNum(smap_file, string):
    """ Return the line number where a string was found.

    Search for a string in the smap file and return the
    line number it was found on.

    Args:
        smap_file: ec.RO.smap or ec.RW.smap
        string: String to search for in file

    Returns:
        The line number where the string was found or -1
        if the string wasn't found.
    """
    line_num = 0
    smap_file.seek(0, 0)
    for line in smap_file.readlines():
        line_num += 1
        if line.find(string) >= 0:
            return line_num
    return -1

def GetSmapOffset(smap_file):
    """ Returns the vectors location found in the smap file

    Search for "vectors" in the smap file and return its
    value

    Args:
        smap_file: ec.RO.smap or ec.RW.smap

    Returns:
        The vectors location if found or -1 if not
        found.
    """

    smap_file.seek(0, 0)
    for line in smap_file:
        if re.search("vectors", line):
            line_split = line.split()
            return int(line_split[0], 16)
    return -1

def GetHostCmdSize(hc_list):
    """ Returns the size, in bytes, of a host command

    Args:
        hc_list: List of tuples in the following format:
        (absolute offset, host command name)

    Returns:
        The the size of a host command or 0 if the size
        could not be dettermed
    """

    itr = 0
    hc_size = 0
    while itr < len(hc_list):
        hc1 = hc_list[itr]
        hc2 = hc_list[itr+1]
        if hc2[0] > hc1[0]:
            hc_size = hc2[0] - hc1[0]
            return hc_size
    return 0

def BreakApartBin(ec_flat, tmpdir, hc_start, hc_len, header):
    """ Break apart the ec binary into three sections

    Args:
        ec_flat: ec binary to break apart
        tmpdir: temp. directory to store the three sections
        hc_start: absolute offset to start of host command section
        hc_len: length of host command section
        header: size of header
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

def SortHostCommands(hc_list, tmpdir, hc_size):
    """ Sort the host commands

    Args:
        hc_list: List of tuples in the following format:
        (absolute offset, host command name)
        tmpdir: temp. directory to store the three sections
        hc_size: size of a single host command
    """

    itr = 0
    offset = 0
    cmd_array = []
    while itr < len(hc_list):
        cmd = "dd if=%s/%s of=%s/%s bs=1 count=%s skip=%s 2>/dev/null" % \
                (tmpdir, "flat_section_mid", tmpdir, hc_list[itr][1], \
                 hc_size, offset)
        os.system(cmd)

        hc_section = "%s/%s" % (tmpdir, hc_list[itr][1])
        hc_binary = open(hc_section, "rb")
        try:
            hc_binary.seek(4, 0)
            hcmd = struct.unpack('i', hc_binary.read(4))[0]
            cmd_array.append((hcmd, hc_list[itr][1]))
        finally:
            hc_binary.close()
        itr += 1
        offset += hc_size

    sorted_hc_list = sorted(cmd_array, key=lambda tup: tup[0])
    return sorted_hc_list

def ReassembleBin(ec_flat, tmpdir, sorted_hc_list):
    """ Reassemble the ec binary

    Args:
        ec_flat: path and name of the ec binary
        tmpdir: temp. directory to work in
        sorted_hc_list: List of sorted tuples in
        the following format: (absolute offset, host command name)
    """

    itr = 0
    cmd = "cp %s/%s %s/%s" % \
        (tmpdir, sorted_hc_list[itr][1], tmpdir, "sorted_flat_section_mid")
    os.system(cmd)
    itr += 1
    while itr < len(sorted_hc_list):
        cmd = "cat %s/%s >> %s/%s" % \
            (tmpdir, sorted_hc_list[itr][1], tmpdir,
             "sorted_flat_section_mid")
        os.system(cmd)
        itr += 1

    cmd1 = "cp %s/%s %s" % (tmpdir, "flat_section_start", ec_flat)
    cmd2 = "cat %s/%s >> %s" % (tmpdir, "sorted_flat_section_mid", ec_flat)
    cmd3 = "cat %s/%s >> %s" % (tmpdir, "flat_section_end", ec_flat)
    os.system(cmd1)
    os.system(cmd2)
    os.system(cmd3)


def main(argc, argv):
    """ Sort the host command section in the ec binary.

    The binary file is broken into 3 section, where the mid
    section contains the host commands.
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

    The mid section is furthor broken in to host commands
    and reassembled in sorted order.

    """

    assert (argc > 3), \
        "Usage: host_command_sort.py file.flat file.elf file.smap"

    ec_flat = argv[1]
    ec_smap = argv[3]

    smap_file = open(ec_smap, "r")

    base_address = GetSmapOffset(smap_file)
    if base_address < 0:
        sys.exit(0)

    hcmds_offset = GetSmapLineNum(smap_file, "__hcmds")
    if hcmds_offset < 0:
        sys.exit(0)

    hcmds_end_offset = GetSmapLineNum(smap_file, "__hcmds_end")
    if hcmds_end_offset < 0:
        sys.exit(0)

    if hcmds_end_offset - hcmds_offset < 3:
        sys.exit(0)

    header = GetSmapLineNum(smap_file, "fw_header")
    if header < 0:
        header = 0
    else:
        header = 64

    smap_file.seek(0, 0)
    lines = smap_file.readlines()
    hc_offset_name = []
    while hcmds_offset < hcmds_end_offset:
        line = lines[hcmds_offset].split()
        if line[2].startswith("__host_cmd_EC_CMD"):
            hc_offset_name.append((int(line[0], 16) - base_address, line[2]))
        hcmds_offset += 1

    smap_file.close()

    hc_size = GetHostCmdSize(hc_offset_name)
    if hc_size == 0:
        sys.exit(0)

    tmpdir = tempfile.mkdtemp()

    BreakApartBin(ec_flat, tmpdir, hc_offset_name[0][0], len(hc_offset_name) * hc_size, header)

    sorted_cmd_array = SortHostCommands(hc_offset_name, tmpdir, hc_size)

    ReassembleBin(ec_flat, tmpdir, sorted_cmd_array)

    shutil.rmtree(tmpdir)

if __name__ == "__main__":
    main(len(sys.argv), sys.argv)
