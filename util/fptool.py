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
import re


# Exit codes
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


def run_system_cmd(cmd, show_output=False, check=True) -> [int, str, str]:
    sys.stdout.flush()
    sys.stderr.flush()
    if show_output:
        cmd = cmd.split()
        system_cmd = subprocess.run(cmd, check=check)
        return system_cmd.returncode, None, None

    system_cmd = subprocess.Popen(cmd,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE,
                                  shell=True,
                                  universal_newlines=True)
    stdout, stderr = system_cmd.communicate()
    return system_cmd.returncode, stdout, stderr


def klog(msg):
    writeline('/dev/kmsg', 'fptool: ' + msg)


def assert_wp_is_disabled():
    rc, wp_enabled, _stderr = run_system_cmd('crossystem wpsw_cur')

    if rc != 0:
        logging.error('Failed to get hardware write protect status')
        sys.exit(ExitCode.EXIT_PRECONDITION)

    if wp_enabled != '0':
        logging.error('Please make sure hardware write protect is disabled.')
        logging.error('See https://www.chromium.org/chromium-os/'
                      'firmware-porting-guide/firmware-ec-write-protection')
        sys.exit(ExitCode.EXIT_PRECONDITION)


def get_devid(dev_type, dev_str) -> str:
    devs = glob.glob(f'/sys/bus/{dev_type}/devices/*')
    for dev in devs:
        modalias = readline(dev + '/modalias')
        # For most devices modalias is 'of:NcrfpTCgoogle,cros-ec-< spi | uart>'
        if modalias and modalias.split(',')[-1] == dev_str:
            return os.path.basename(dev)
        # For strongbad and herobrine, the modalias is: 'spi:cros-ec-spi'
        # TODO(b/179533783): Fix this script to look for non-ACPI modalias
        if modalias and modalias.split(':')[-1] == dev_str:
            return os.path.basename(dev)

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


def read_gpio_ranges() -> list:
    """Parse the GPIO ranges from "/sys/kernel/debug/pinctrl/*/gpio-ranges"

    Used when gpios are referenced by index
    The result is a list of dictionaries:
    ranges:
        gpio            pin             idx
        first   last    first   last    first   last

        gpio[first] is the base index for the gpios range
        pin[first] - pin[last] is the sequential index of the gpio in the range
        idx[first] - idx[last] is the sequential global index of the gpio

    The list is sorted in ascending order of the base index (gpio[first])
    """
    ranges = []
    path = glob.glob('/sys/kernel/debug/pinctrl/*/gpio-ranges')
    for filename in path:
        try:
            with open(filename, 'r') as fp:
                # Skip the 'GPIO ranges handled:' line
                line = fp.readline()
                for line in fp.readlines():
                    d = {}
                    s = line.split(' ')
                    d['device'] = s[1]
                    d['gpio'] = {}
                    d['gpio']['first'] = int(re.sub('[^0-9]', '', s[3]))
                    d['gpio']['last'] = int(re.sub('[^0-9]', '', s[5]))
                    d['pin'] = {}
                    d['pin']['first'] = int(re.sub('[^0-9]', '', s[7]))
                    d['pin']['last'] = int(re.sub('[^0-9]', '', s[9]))
                    ranges.append(d)
        except OSError:
            logging.warning('------------------ Error reading from %s',
                            filename)
            logging.warning('\t\tOSError: %s', sys.exc_info()[1].strerror)

    ranges = sorted(ranges, key=lambda x: x['gpio']['first'])
    prev = {}
    for r in ranges:
        r['idx'] = {}
        if not prev:
            start = 0
        elif r['pin']['first'] <= prev['pin']['last']:
            start = prev['idx']['last'] + 1
        else:
            start = prev['idx']['first'] - prev['pin']['first']
        r['idx']['first'] = r['pin']['first'] + start
        r['idx']['last'] = r['pin']['last'] + start
        prev = r

    return ranges


def read_gpiochips() -> dict:
    """Parse the base and label of each gpio chip.

    Used when gpios are referenced by name
    The result is a dictionary of labels (device names) and bases
    bases
        device1: base1
        device2: base2
    """
    bases = {}
    detect = {}
    rc, stdout, _stderr = run_system_cmd('gpiodetect')
    if rc != 0 or not stdout:
        return {}
    for device in stdout.split('\n'):
        args = device.split()
        if not args:
            break
        detect[args[1][1:-1]] = args[0]
    paths = glob.glob('/sys/class/gpio/gpiochip*/')
    for path in paths:
        label = readline(path + 'label')
        base = readline(path + 'base')
        bases[detect[label]] = base
    return bases


def get_gpio_by_index(ranges: list, idx: int) -> int:
    """Convert gpio index to its absolute location in the device.

    The absolute location is calculated as follows:
        Absolute location = gpio index - first index in the range + base
    """
    if idx < 0:
        return -1
    for g_range in ranges:
        if idx > g_range['idx']['last']:
            continue
        if idx < g_range['idx']['first']:
            continue
        return g_range['gpio']['first'] + idx - g_range['idx']['first']

    logging.error('GPIO pin index %d does not belong to any supported range',
                  idx)
    logging.error('Supported ranges:')
    for g_range in ranges:
        logging.error('\t%d - %d', g_range['idx']['first'],
                      g_range['idx']['last'])
        sys.exit(ExitCode.EXIT_RUNTIME)


def get_gpio_by_name(bases: dict, name: str) -> int:
    """Convert gpio's name to its absolute location in the device.

    Utilizes libgpiod APIs 'gpiofind' and 'gpiodetect'
    """
    rc, stdout, _stderr = run_system_cmd('gpiofind ' + name)
    if not bases or rc != 0:
        logging.error('Failed to find GPIO %s', name)
        sys.exit(ExitCode.EXIT_RUNTIME)
    gpio = stdout.split()
    # e.g.
    #   gpiofind FP_RST_L
    #   gpiochip0 22
    #
    # Gpio location is: 22 + bases['gpiochip0']
    return int(gpio[1]) + int(bases[gpio[0]])


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

    # If there was a way to get the board name from cros_config, it's possible
    # that it could fail in the following cases:
    #
    # 1) We're running on a non-unibuild device (the only one with FP is
    #    nocturne)
    # 2) We're running on a proto device during bringup and the cros_config
    #    settings haven't yet been setup.
    #
    # In all cases we can fall back to /etc/lsb-release. It's not recommended
    # to do this, but we don't have any other options in this case.

    # lsbval should not be used by anything except get_platform_name.
    # See https://crbug.com/98462.
    def lsbval(key) -> str:
        try:
            with open('/etc/lsb-release', 'r') as fp:
                for line in fp.readlines():
                    line = line.rstrip()
                    keyval = line.split('=')
                    if key == keyval[0]:
                        return keyval[1]
                logging.warning('Failed to find %s in /etc/lsb-release', key)
        except OSError:
            logging.warning('--------------- Error reading /etc/lsb-release')
            logging.warning('\t\tOSError: %s', sys.exc_info()[1].strerror)

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


def get_default_fw() -> str:
    rc, board, _stderr = run_system_cmd('cros_config /fingerprint board')
    if rc != 0:
        logging.warning('Failed to identify fingerprint board name')
    # If cros_config returns "", that is okay assuming there is only
    # one firmware file on disk.
    if not board:
        board = ''
    fws = glob.glob('/opt/google/biod/fw/' + board + '*.bin')
    if len(fws) == 0:
        logging.error('Failed to identify the default fingerprint fw name')
        sys.exit(ExitCode.EXIT_CONFIG)
    if len(fws) != 1:
        logging.error('Multiple fingerprint fw names')
        sys.exit(ExitCode.EXIT_CONFIG)
    return fws[0]


def proc_open_files(*args) -> list:
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
                    pids.append('PID {} -> {}'.format(proc.split('/')[2], link))
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
    group.add_argument('--noread', action='store_true',
                       help='Read instead of write (Default: False)')
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument('-U', '--remove_flash_read_protect', action='store_true',
                       default=True)
    group.add_argument('--noremove_flash_read_protect', action='store_true',
                       help='Remove flash read protection while performing '
                       'command (Default: True)')
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument('-u', '--remove_flash_write_protect',
                       action='store_true', default=True)
    group.add_argument('--noremove_flash_write_protect', action='store_true',
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
    group.add_argument('--nohello', action='store_true',
                       help='Only ping the bootloader (Default: %(default)s)')
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument('-s', '--services', default=True, action='store_true')
    group.add_argument('--noservices', action='store_true',
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
