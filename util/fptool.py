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
import glob
from typing import List
import re


class ExitCode:
    """Exit Codes."""
    EXIT_ARGUMENT = 3
    EXIT_CONFIG = 4
    EXIT_PRECONDITION = 5
    EXIT_RUNTIME = 6


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


def assert_wp_is_disabled():
    """Exit with failure if the device is write protected"""
    rc, wp_enabled_str, _stderr = run_system_cmd('crossystem wpsw_cur')

    if rc != 0:
        logging.error('Failed to get hardware write protect status')
        sys.exit(ExitCode.EXIT_PRECONDITION)

    # wp_enabled_str:
    #   '0' - Disabled
    #   '1' - Enabled
    if int(wp_enabled_str) != 0:
        logging.error('Please make sure hardware write protect is disabled.')
        logging.error('See https://www.chromium.org/chromium-os/'
                      'firmware-porting-guide/firmware-ec-write-protection')
        sys.exit(ExitCode.EXIT_PRECONDITION)


def read_modalias(dev_type: str) -> dict:
    """Read the modalias files according to the device type"""
    devs = glob.glob(f'/sys/bus/{dev_type}/devices/*/modalias')
    modalias_list = {}
    for dev in devs:
        modalias = readline(dev)
        if not modalias:
            continue
        modalias_list[os.path.basename(os.path.dirname(dev))] = modalias
    return modalias_list


def get_devid(dev_type: str, dev_str: str) -> str:
    """Read the modalias list and extract the device ID

    For SPI:
        cat /sys/bus/spi/devices/spi-PR0001:01/modalias
            of:NcrfpTCgoogle,cros-ec-spi
    For UART:
        cat /sys/bus/serial/devices/serial0-0/modalias
            of:NcrfpTCgoogle,cros-ec-uart
    For Strongbad SPI:
        cat /sys/bus/spi/devices/spi10.0/modalias
            spi:cros-ec-spi
    """
    modalias_list = read_modalias(dev_type)
    if not modalias_list:
        return ''

    for dev, modalias in modalias_list.items():
        # For most devices modalias is 'of:NcrfpTCgoogle,cros-ec-< spi | uart>'
        if modalias.split(',')[-1] == dev_str:
            return dev
        # For strongbad and herobrine, the modalias is: 'spi:cros-ec-spi'
        # TODO(b/179533783): Fix this script to look for non-ACPI modalias
        if modalias.split(':')[-1] == dev_str:
            return dev

    return ''


def get_spiid() -> str:
    """Get the spiid for the fingerprint sensor based on the modalias string.

       see: https://crbug.com/955117
    """
    return get_devid('spi', 'cros-ec-spi')


def get_uartid() -> str:
    """Get the uartid for the fingerprint sensor based on the modalias."""
    return get_devid('serial', 'cros-ec-uart')


def get_uart_dev_name(device_id: str) -> str:
    """Find the UART device associated with the device ID.

    e.g. Zork
        Device ID: serial0-0
        Device association:
            /sys/bus/platform/drivers/dw-apb-uart/AMD0020:01/serial0/serial0-0/
        Device Name: AMD0020:01
    """
    path = '/sys/bus/platform/drivers/dw-apb-uart/'
    dirs = glob.glob(f'{path}*/*/{device_id}/')
    if not dirs:
        logging.warning('Failed to locate device for: %s', device_id)
        return ''
    if len(dirs) > 1:
        logging.warning('Device for %s is ambiguous', device_id)
        return ''

    return os.path.basename(os.path.dirname(os.path.dirname(dirs[0][:-1])))


def read_lsbval() -> List[str]:
    return open('/etc/lsb-release').readlines()


def lsbval(key: str) -> str:
    """Reads /etc/lsb-release to find the board's name

    If there was a way to get the board name from cros_config, it's possible
    that it could fail in the following cases:

    1) We're running on a non-unibuild device (the only one with FP is
        nocturne)
    2) We're running on a proto device during bringup and the cros_config
        settings haven't yet been setup.

    In all cases we can fall back to /etc/lsb-release. It's not recommended
    to do this, but we don't have any other options in this case.

    lsbval should not be used by anything except get_platform_name.
    """
    # Code adapted from:
    #   https://chromium.googlesource.com/chromiumos/docs/+/HEAD/lsb-release.md
    lines = [x.strip() for x in read_lsbval()]
    return dict([(line.split('=', 1)[0].strip(), line.split('=', 1)[1].strip())
                 for line in lines if line and not line.startswith('#')])[key]


def get_platform_name() -> str:
    """Get the reference design name.

    Get the underlying board (reference design) that we're running on
    (not the FPMCU or sensor).
    This may be an extended platform name, like nami-kernelnext, hatch-arc-r,
    or hatch-borealis.
    """
    # We used to use "cros_config /identity platform-name", but that is specific
    # to mosys and does not actually provide the board name in all cases.
    # cros_config intentionally does not provide a way to get the board
    # name: b/156650654.

    logging.info('Getting platform name from /etc/lsb-release.')
    return lsbval('CHROMEOS_RELEASE_BOARD')


def get_platform_base_name(platform_name: str) -> str:
    """Given a full platform name, extract the base platform.

    Tests are also run on modified images, like hatch-arc-r, hatch-borealis,
    or hatch-kernelnext. These devices still have fingerprint and are
    expected to pass tests. The full platform name reflects these
    modifications and might be needed to apply an alternative configuration
    (kernelnext). Other modified tests (arc-r) just need to default to the
    base platform config, which is identified by this function.
    See b/186697064.

    Examples:
        * platform_base_name "hatch-kernelnext" --> "hatch"
        * platform_base_name "hatch-arc-r"      --> "hatch"
        * platform_base_name "hatch-borealis"   --> "hatch"
        * platform_base_name "hatch"            --> "hatch"
    """
    return platform_name.split('-')[0]


def get_default_firmware() -> str:
    rc, board, _stderr = run_system_cmd('cros_config /fingerprint board')
    if rc != 0:
        logging.warning('Failed to identify fingerprint board name')
        board = ''
    # If cros_config returns "", that is okay assuming there is only
    # one firmware file on disk.
    if not board:
        board = ''
    firmware_file_names = glob.glob(f'/opt/google/biod/fw/{board}*.bin')
    if len(firmware_file_names) == 0:
        logging.error('Failed to identify the default fingerprint firmware')
        sys.exit(ExitCode.EXIT_CONFIG)
    if len(firmware_file_names) != 1:
        logging.error('Multiple fingerprint firmwares found')
        sys.exit(ExitCode.EXIT_CONFIG)
    return firmware_file_names[0]


def proc_open_files(*args) -> List[str]:
    """Find processes that have the named file, active or deleted, open.

    Deleted files are important because unbinding/rebinding cros-ec
    with biod/timberslide running will result in the processes holding open
    a deleted version of the files. Issues can arise if the process continue
    to interact with the deleted files (e.g. kernel panic) while the raw
    driver is being used in flash_fp_mcu. The lsof and fuser tools can't
    seem to identify usages of the deleted named file directly, without
    listing all files. This takes a large amount of time on Chromebooks,
    thus we need this custom search routine.
    """
    pids = []
    procs = glob.glob('/proc/*/fd/*')
    for proc in procs:
        if os.access(proc, os.F_OK):
            link = os.readlink(proc)
            for pattern in args:
                if re.search(pattern, link):
                    pid_str = proc.split('/')[2]
                    device_file_name = link
                    pids.append(f'PID {pid_str} -> {device_file_name}')
                    break
    return pids


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
