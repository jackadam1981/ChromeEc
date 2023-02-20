#!/usr/bin/python

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# from __future__ import with_statement
import sys
import binascii
import os
import serial
import serial.tools.list_ports
import time
import struct
import array

start_add = 0x0
data_length = 0x80000
operation = 1  # Default operation is WR
prompt_text = ""
menu_select = 1

newBuf = []


def utility():

    try:
        # Copy PgmHdrFile.bin to Update_PgmHdrFile.bin
        read_bin_file = open("pgm_hdr.bin", "rb")
    except:
        print("Cant' open ", read_bin_file, ": Please check the file\n")
        os._exit(1)

    try:
        write_bin_file = open("flash_pgm_hdr.bin", "wb")
    except:
        print("Cant' open ", read_bin_file, ": Please check the file\n")
        os._exit(1)

    bstrings = read_bin_file.readlines()
    write_bin_file.writelines(bstrings)

    read_bin_file.close()

    # Update SPI_UTILITY_COMMAND
    if operation == 1:  # Program
        byte6 = 0x01
    elif operation == 2:  # Read
        byte6 = 0x08
    elif operation == 3:  # Erase
        byte6 = 0x02
    elif operation == 4:  # Partial Erase
        byte6 = 0x10
    elif operation == 5:  # Verify
        byte6 = 0x04

    write_bin_file.seek(0x06, 0)
    newBuf = (byte6).to_bytes(1, "big")
    write_bin_file.write(newBuf)

    # UPDATE FLASH_START_ADDRESS
    write_bin_file.seek(0x08, 0)
    newBuf = start_add.to_bytes(4, "big")
    write_bin_file.write(newBuf)

    # UPDATE DATA_LENGTH
    write_bin_file.seek(0x0C, 0)
    newBuf = data_length.to_bytes(4, "big")
    write_bin_file.write(newBuf)


def main():

    global start_add, data_length, operation, prompt_text, menu_select

    print(
        "*****************************************************************************"
    )
    print("** gen_pgm_hdr Utility - Verion 0.2 - 02/15/23")
    print(
        "*****************************************************************************"
    )
    print(
        "** This utility allows you to configure the SPI operation needed and"
    )
    print("** generate flash_pgm_hdr.bin header for use ")
    print("** with crisis_rcvry_util.py")
    print("** ")

    print(
        "***************************************************************************\n"
    )
    menu_select = 2
    operation = 1
    if menu_select == 2:
        utility()
        print("\nUtility ran.  Check folder for flash_pgm_hdr.bin.")


if __name__ == "__main__":
    main()
