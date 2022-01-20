#!/usr/bin/env python3
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to manage the fingerprint system on Chrome OS."""

import argparse
import sys
import logging
import subprocess
import os


def readline(file_name: str) -> str:
    try:
        with open(file_name, 'r') as fd:
            return fd.readline().rstrip()
    except OSError:
        logging.warning('--------------- Error reading from %s', file_name)
        logging.warning('\t\tOSError: %s', sys.exc_info()[1].strerror)
        return ''


def writeline(file_name: str, data: str):
    try:
        with open(file_name, 'w') as fd:
            fd.write(data + '\n')
    except OSError:
        logging.warning('--------------- Error writing: %s to %s', data,
                        file_name)
        logging.warning('\t\tOSError: %s', sys.exc_info()[1].strerror)


def run_system_cmd(cmd, show_output=False) -> [int, str, str]:
    sys.stdout.flush()
    sys.stderr.flush()

    system_cmd = subprocess.Popen(cmd,
                                  stdout=None if show_output
                                  else subprocess.PIPE,
                                  stderr=None if show_output
                                  else subprocess.PIPE,
                                  shell=True,
                                  universal_newlines=True)
    stdout, stderr = system_cmd.communicate()
    return system_cmd.returncode, stdout, stderr


def klog(msg: str):
    logging.debug('fptool: %s', msg)
    writeline('/dev/kmsg', f'fptool: {msg}')


def cmd_flash(args: argparse.Namespace):
    """Flash the entire firmware FPMCU using the built-in bootloader.

    This requires the Chromebook to be in dev mode with hardware write protect
    disabled.
    """


def flash_init(parser):
    flash_parser = parser.add_parser('flash', help=cmd_flash.__doc__)
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument('-r', '--read', action='store_true')
    group.add_argument('--noread', dest='read', action='store_false',
                       default=False,
                       help='Read instead of write (Default: False)')
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument('-U', '--remove_flash_read_protect', action='store_true',
                       default=True)
    group.add_argument('--noremove_flash_read_protect',
                       dest='remove_flash_read_protect', action='store_false',
                       help='Remove flash read protection while performing '
                       'command (Default: True)')
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument('-u', '--remove_flash_write_protect',
                       action='store_true', default=True)
    group.add_argument('--noremove_flash_write_protect',
                       dest='remove_flash_write_protect', action='store_false',
                       help='Remove flash read protection while performing '
                       'command (Default: True)')
    flash_parser.add_argument('-R', '--retries', type=int, default=4,
                              help='Specify number of retries (default: '
                              '%(default)s)')
    flash_parser.add_argument('-B', '--baudrate', type=int, default=115200,
                              help='Specify UART baudrate (default: '
                              '%(default)s)')
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument('-H', '--hello', action='store_true')
    group.add_argument('--nohello', dest='hello', action='store_false',
                       default=False,
                       help='Only ping the bootloader (Default: %(default)s)')
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument('-s', '--services', default=True, action='store_true')
    group.add_argument('--noservices', dest='services', action='store_false',
                       default=False,
                       help='Stop and restart conflicting fingerprint services '
                       '(Default: True)')
    flash_parser.add_argument('binary', type=str, nargs='?',
                              help='Flash binary [ec.bin]')
    flash_parser.set_defaults(func=cmd_flash, connect_retries=6)
    log_level_choices = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
    flash_parser.add_argument('--log_level', '-l', choices=log_level_choices,
                              default='INFO')


def main() -> int:
    logging.basicConfig(level='INFO')
    # print out canonical path to differentiate between /usr/local/bin and
    # /usr/bin installs
    logging.info('Path: %s', os.path.realpath(sys.argv[0]))
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest='subcommand', title='subcommands')
    # This method of setting required is more compatible with older python.
    subparsers.required = True

    flash_init(subparsers)

    opts = parser.parse_args()

    return opts.func(opts)


if __name__ == '__main__':
    sys.exit(main())
