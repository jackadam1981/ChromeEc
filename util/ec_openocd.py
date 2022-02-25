#!/usr/bin/env python3

# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=logging-fstring-interpolation

import argparse;
from telnetlib import Telnet
import os
import sys
import subprocess
from subprocess import PIPE, STDOUT
import time
import socket

"""
Flashes and debugs the EC through openocd
"""

class BoardInfo():
    def __init__(self, gdb_variant, num_breakpoints, num_watchpoints):
        self.gdb_variant = gdb_variant
        self.num_breakpoints = num_breakpoints
        self.num_watchpoints = num_watchpoints

# Debuggers for each board, OpenOCD currently only supports GDB
boards = {
    'skyrim': BoardInfo('arm-none-eabi-gdb', 6, 4)
}

def create_openocd_args(interface, board):
    if not board in boards:
        raise RuntimeError(f'Unsupported board {board}')

    board_info = boards[board]
    args = [
        'openocd',
        '-f', f'interface/{interface}.cfg',
        '-c', 'add_script_search_dir openocd',
        '-f', f'board/{board}.cfg',
    ]

    return args

def create_gdb_args(board, port, executable):
    if not board in boards:
        raise RuntimeError(f'Unsupported board {board}')

    board_info = boards[board]
    args = [
        board_info.gdb_variant,
        executable,
        # GDB can't autodetect these according to OpenOCD
        '-ex', f'set remote hardware-breakpoint-limit {board_info.num_breakpoints}',
        '-ex', f'set remote hardware-watchpoint-limit {board_info.num_watchpoints}',

        # Connect to OpenOCD
        '-ex', f'target extended-remote localhost:{port}',
    ]

    return args

def flash(interface, board, image, verify):
    print(f'Flashing image {image}')
    # Run OpenOCD and pipe its output to stdout
    # OpenOCD will shutdown after the flashing is completed
    args = create_openocd_args(interface, board)
    args += ['-c', f'init; flash_target {image} {int(verify)}; shutdown']

    subprocess.run(args, stdout = sys.stdout, stderr = STDOUT)


def debug(interface, board, port, executable):
    # Start OpenOCD in the background
    openocd_args = create_openocd_args(interface, board)
    openocd_args += ['-c', f'gdb_port {port}']

    openocd = subprocess.Popen(openocd_args, encoding = 'utf-8', stdout = PIPE, stderr = STDOUT)

    # Wait for OpenOCD to start, it'll open a port for GDB connections
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    connected = False
    for i in range(0, 10):
        print('Waiting for OpenOCD to start...')
        connected = sock.connect_ex(('localhost', port))
        if connected:
            break

        time.sleep(1000)

    if not connected:
        print(f'Failed to connect to OpenOCD on port {port}')
        return

    sock.close()

    gdb_args = create_gdb_args(board, port, executable)
    # Start GDB
    gdb = subprocess.run(gdb_args, stdout = sys.stdout, stderr = STDOUT, stdin = sys.stdin)

    # Wait for OpenOCD to shutdown
    print("Waiting for OpenOCD to finish...")
    try:
        openocd.wait(timeout=3)
    except:
        # We're going to kill OpenOCD anyway
        # So we don't care if it hasn't exited
        pass

    killed = False
    if openocd.poll() == None:
        # OpenOCD didn't shutdown, kill it
        openocd.kill()
        killed = True

    if openocd.returncode != 0 or killed:
        print("OpenOCD failed to shutdown cleanly: ")
    print(openocd.stdout.read())

def get_flash_file(board):
        return os.path.abspath(f'../build/zephyr/{board}/output/zephyr.bin')

def get_executable_file(board):
        return os.path.abspath(f'../build/zephyr/{board}/output/zephyr.ro.elf')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--board', '-b',
        required=True
    )

    parser.add_argument(
        '--interface', '-i',
        help='The JTAG interface to use',
        default='jlink'
    )

    parser.add_argument(
        '--file', '-f',
        help='The file to use, see each sub-command for what the file is used for',
        default=''
    )

    sub_parsers = parser.add_subparsers(help='sub-command -h for specific help')
    flash_parser = sub_parsers.add_parser('flash', help='Flashes an image to the target EC, \
        FILE selects the image to flash, defaults to the zephyr image')
    flash_parser.set_defaults(command='flash')
    flash_parser.add_argument(
        '--verify', '-v',
        help='Verify flash after writing image, defaults to true',
        default=True
    )

    debug_parser = sub_parsers.add_parser('debug', help='Debugs the target EC through GDB, \
        FILE selects the executable to load debug info from, defaults to using the zephyr RO executable')
    debug_parser.set_defaults(command='debug')
    debug_parser.add_argument(
        '--port', '-p',
        help='The port for GDB to connect to',
        type=int,
        default=3333
    )

    args = parser.parse_args()
    # Get the image path if we were given one
    target_file = ''
    if args.file != '':
        target_file = os.path.abspath(args.file)

    if args.command == 'flash':
        image_file = get_flash_file(args.board) if target_file == '' else target_file
        flash(args.interface, args.board, image_file, args.verify)
    elif args.command == 'debug':
        executable_file = get_executable_file(args.board) if target_file == '' else target_file
        debug(args.interface, args.board, args.port, executable_file)
    else:
        parser.print_usage()

if __name__ == '__main__':
    main()