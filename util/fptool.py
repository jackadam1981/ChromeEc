#!/usr/bin/env python3
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to manage the fingerprint system on Chrome OS."""

import argparse
import os
import shutil
import subprocess
import sys
import glob
import re
import datetime
import stat
import time
import logging

# Exit codes
class ExitCode:
    EXIT_ARGUMENT = 3
    EXIT_CONFIG = 4
    EXIT_PRECONDITION = 5
    EXIT_RUNTIME = 6

class FPGpios:
    NRST = -1
    BOOT0 = -1
    PWREN = -1
    def __init__(self, NRST, BOOT0, PWREN = -1):
        self.NRST = NRST
        self.BOOT0 = BOOT0
        self.PWREN = PWREN

def readline(file_name: str):
    try:
        with open(file_name, 'r') as fd:
            return fd.readline().rstrip()
    except OSError:
        logging.warning(f"--------------- Error reading from {file_name}")
        logging.warning(f"\t\tOSError: {sys.exc_info()[1].strerror}")
        return ""

def writeline(file_name: str, data: str):
    try:
        with open(file_name, 'w') as fd:
            fd.write(data + '\n')
    except OSError:
        logging.warning(f"--------------- Error writing: {data} to {file_name}")
        logging.warning(f"\t\tOSError: {sys.exc_info()[1].strerror}")

def run_system_cmd(cmd, show_output=False):
    sys.stdout.flush()
    sys.stderr.flush()
    if show_output:
        cmd = cmd.split()
        system_cmd = subprocess.run(cmd)
        return system_cmd.returncode

    system_cmd = subprocess.Popen(cmd,
            stdout = subprocess.PIPE,
            stderr = subprocess.PIPE,
            shell = True,
            universal_newlines = True)
    stdout, stderr = system_cmd.communicate()
    return system_cmd.returncode, stdout, stderr

def klog(msg):
    writeline("/dev/kmsg", "fptool: " + msg)

def assert_wp_is_disabled():
    rc, wp_enabled, stderr = run_system_cmd("crossystem wpsw_cur")

    if rc != 0:
        logging.error("Failed to get hardware write protect status")
        sys.exit(ExitCode.EXIT_PRECONDITION)

    if wp_enabled != '0':
        logging.error("Please make sure hardware write protect is disabled.")
        logging.error("See https://www.chromium.org/chromium-os/"
            "firmware-porting-guide/firmware-ec-write-protection")
        sys.exit(ExitCode.EXIT_PRECONDITION)

def get_devid(dev_type, dev_str):
    devs = glob.glob(f"/sys/bus/{dev_type}/devices/*")
    for dev in devs:
        modalias = readline(dev + "/modalias")
        # For most devices modalias is "of:NcrfpTCgoogle,cros-ec-< spi | uart>"
        if modalias and modalias.split(',')[-1] == dev_str:
            return os.path.basename(dev)
        # For strongbad and herobrine, the modalias is: "spi:cros-ec-spi"
        # TODO(b/179533783): Fix this script to look for non-ACPI modalias
        if modalias and modalias.split(':')[-1] == dev_str:
            return os.path.basename(dev)
    else:
        return ""

# Get the spiid for the fingerprint sensor based on the modalias
# string: https://crbug.com/955117
def get_spiid():
    return get_devid("spi", "cros-ec-spi")

# Get the uartid for the fingerprint sensor based on the modalias
def get_uartid():
    return get_devid("serial", "cros-ec-uart")

# Find the UART device associated with the device ID
# e.g. Zork
#       Device ID: serial0-0
#       Device association:
#         /sys/bus/platform/drivers/dw-apb-uart/AMD0020:01/serial0/serial0-0/
#       Device Name: AMD0020:01
def get_uart_dev_name(deviceid: str) -> str:
    path = "/sys/bus/platform/drivers/dw-apb-uart/"
    dirs = glob.glob(f"{path}*/*/{deviceid}/" )
    if not dirs:
        logging.warning(f"Failed to locate device for: {deviceid}")
        return ""
    if len(dirs) > 1:
        logging.warning(f"Device for {deviceid} is ambiguous")
        return ""

    return os.path.basename(os.path.dirname(os.path.dirname(dirs[0][:-1])))

# Parse the GPIO ranges from "/sys/kernel/debug/pinctrl/*/gpio-ranges"
# Used when gpios are referenced by index
#
# The result is a list of dictionaries:
# ranges
#       gpio            pin             idx
#       first   last    first   last    first   last
#
# gpio[first] is the base index for the gpios range
# pin[first] - pin[last] is the sequential index of the gpio in the range
# idx[first] - idx[last] is the sequential global index of the gpio
#
# The list is sorted in ascending order of the base index (gpio[first])

def read_gpio_ranges():
    ranges = []
    path = glob.glob('/sys/kernel/debug/pinctrl/*/gpio-ranges')
    for filename in path:
        try:
            with open(filename, 'r') as fp:
                # Skip the "GPIO ranges handled:" line
                line = fp.readline()
                for line in fp.readlines():
                    d = {}
                    s = line.split(' ')
                    d['device'] = s[1]
                    d['gpio'] = {}
                    d['gpio']['first'] = int(re.sub("[^0-9]", "", s[3]))
                    d['gpio']['last'] = int(re.sub("[^0-9]", "", s[5]))
                    d['pin'] = {}
                    d['pin']['first'] = int(re.sub("[^0-9]", "", s[7]))
                    d['pin']['last'] = int(re.sub("[^0-9]", "", s[9]))
                    ranges.append(d)
        except OSError:
            logging.warning(f"------------------ Error reading from {filename}")
            logging.warning(f"\t\tOSError: {sys.exc_info()[1].strerror}")
    ranges = sorted(ranges, key = lambda x: x['gpio']['first'])
    prev = []
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

# Parse the base and label of each gpio chip
# Used when gpios are referenced by name
#
# The result is a dictionary of labels (device names) and bases
# bases
#       device1: base1
#       device2: base2

def read_gpiochips():
    bases = {}
    detect = {}
    rc, stdout, stderr = run_system_cmd("gpiodetect");
    if rc != 0 or not stdout:
        return {}
    for device in stdout.split('\n'):
        args = device.split()
        if not args:
            break
        detect[args[1][1:-1]] = args[0]
    paths = glob.glob('/sys/class/gpio/gpiochip*/')
    for path in paths:
        label = readline(path + "label")
        base = readline(path + "base")
        bases[detect[label]] = base
    return bases

# Convert gpio index to its absolute location in the device
# The absolute location is calculated as follows:
#
#       Absolute location = gpio index - first index in the range + base
#
def get_gpio_by_index(ranges: list, idx: int) -> int:
    if idx < 0:
        return -1
    for g_range in ranges:
        if idx > g_range['idx']['last']:
            continue
        if idx < g_range['idx']['first']:
            continue
        return g_range['gpio']['first'] + idx - g_range['idx']['first']

    else:
        logging.error(f"GPIO pin index {idx} does not belong to any supported"
            "range")
        logging.error("Supported ranges:")
        for g_range in ranges:
            logging.error(f"\t{g_range['idx']['first']} - "
                f"{g_range['idx']['last']}")
        sys.exit(ExitCode.EXIT_RUNTIME)

    return gpios

# Convert gpio's name to its absolute location in the device
# Utilizes libgpiod APIs 'gpiofind' and 'gpiodetect'
def get_gpio_by_name(bases: dict, name: str) -> int:
    rc, stdout, stderr = run_system_cmd("gpiofind " + name)
    if not bases or rc != 0:
        logging.error(f"Failed to find GPIO {name}")
        sys.exit(ExitCode.EXIT_RUNTIME)
    gpio = stdout.split()
    # e.g.
    #   gpiofind FP_RST_L
    #   gpiochip0 22
    #
    # Gpio location is: 22 + bases['gpiochip0']
    return int(gpio[1]) + int(bases[gpio[0]])

def get_gpio(ranges, bases, gpio):
    if isinstance(gpio, int):
        return get_gpio_by_index(ranges, gpio)
    return get_gpio_by_name(bases, gpio)

def get_gpios(gpios: FPGpios) -> FPGpios:
    bases = read_gpiochips()
    ranges = read_gpio_ranges()
    for key in vars(gpios):
        vars(gpios)[key] = get_gpio(ranges, bases, vars(gpios)[key])
    return gpios

# Usage: gpio <unexport|export|in|out|0|1|get> <signal> [signal...]
def gpio(cmd, *signals) -> list:

    # ==============================================================
    # Internal commands
    def export(cmd: str, signal: int):
        klog(f"Set gpio {signal} to {cmd}")
        writeline("/sys/class/gpio/" + cmd, str(signal))

    def direction(cmd: str, signal: int):
        klog(f"Set gpio {signal} direction to {cmd}")
        writeline(f"/sys/class/gpio/gpio{signal}/direction", cmd)

    def value_set(cmd: str, signal: int):
        klog(f"Set gpio {signal} to {cmd}")
        writeline(f"/sys/class/gpio/gpio{signal}/value", cmd)

    def value_get(cmd: str, signal: int):
        klog(f"Get gpio {signal}")
        return readline(f"/sys/class/gpio/gpio{signal}/value")
    # ==============================================================

    responses = []
    cmds = {"export" : export,
            "unexport" : export,
            "in" : direction,
            "out" : direction,
            "0" : value_set,
            "1" : value_set,
            "get" : value_get,
            }
    if not cmd in cmds:
        logging.error(f"Invalid gpio command: {cmd}")
        sys.exit(ExitCode.EXIT_RUNTIME)
    for signal in signals:
        response = cmds[cmd](cmd, signal)
        if response:
            responses.append(response)
    return responses

def warn_gpio(signal: str, expected_value: str, msg: str):
    value = gpio("get", signal)
    if not value:
        logging.error(f"Error reading gpio {signal} value")
        sys.exit(ExitCode.EXIT_RUNTIME)
    if value[0] != expected_value:
        logging.warning(msg)

# Get the underlying board (reference design) that we're running on (not the
# FPMCU or sensor).
# This may be an extended platform name, like nami-kernelnext, hatch-arc-r,
# or hatch-borealis.
def get_platform_name():
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
    def lsbval(key):
        try:
            with open("/etc/lsb-release", 'r') as fp:
                for line in fp.readlines():
                    line=line.rstrip();
                    keyval = line.split('=')
                    if key == keyval[0]:
                        return keyval[1]
                else:
                    logging.warning(f"Failed to find {key} in /etc/lsb-release")
        except OSError:
            logging.warning(f"--------------- Error reading /etc/lsb-release")
            logging.warning(f"\t\tOSError: {sys.exc_info()[1].strerror}")

    logging.info("Getting platform name from /etc/lsb-release.")
    return lsbval("CHROMEOS_RELEASE_BOARD")

# Given a full platform name, extract the base platform.
#
# Tests are also run on modified images, like hatch-arc-r, hatch-borealis, or
# hatch-kernelnext. These devices still have fingerprint and are expected to
# pass tests. The full platform name reflects these modifications and might
# be needed to apply an alternative configuration (kernelnext). Other modified
# tests (arc-r) just need to default to the base platform config, which is
# identified by this function.
# See b/186697064.
#
# Examples:
# * platform_base_name "hatch-kernelnext" --> "hatch"
# * platform_base_name "hatch-arc-r"      --> "hatch"
# * platform_base_name "hatch-borealis"   --> "hatch"
# * platform_base_name "hatch"            --> "hatch"
#
def get_platform_base_name(platform_name: str):
    return platform_name.split('-')[0]

def get_default_fw():
    rc, board, stderr = run_system_cmd("cros_config /fingerprint board")
    if rc != 0:
        logging.warning("Failed to identify fingerprint board name")
    # If cros_config returns "", that is okay assuming there is only
    # one firmware file on disk.
    if not board:
        board = ""
    fws = glob.glob("/opt/google/biod/fw/" + board + "*.bin")
    if len(fws) == 0:
        logging.error("Failed to identify the default fingerprint fw name")
        sys.exit(ExitCode.EXIT_CONFIG)
    if len(fws) != 1:
        logging.error("Multiple fingerprint fw names")
        sys.exit(ExitCode.EXIT_CONFIG)
    return fws[0]

# Find processes that have the named file, active or deleted, open.
#
# Deleted files are important because unbinding/rebinding cros-ec
# with biod/timberslide running will result in the processes holding open
# a deleted version of the files. Issues can arise if the process continue
# to interact with the deleted files (e.g. kernel panic) while the raw driver
# is being used in flash_fp_mcu. The lsof and fuser tools can't seem to
# identify usages of the deleted named file directly, without listing all
# files. This takes a large amount of time on Chromebooks, thus we need this
# custom search routine.
#
def proc_open_files(pattern):
    pids = []
    rc, ls_l, stderr = run_system_cmd("ls -l /proc/*/fd/* 2>/dev/null | grep "
            + "\"" + pattern + "\"")
    ls_l = ls_l.split("\n")
    for ls in ls_l:
        if ls:
            sp = ls.rstrip().split()
            pid = "PID "+ sp[8].split('/')[2] + " -> " + sp[10]
            if len(sp) > 11:
                pid += " " + sp[11]
            pids.append(pid)
    return pids

def cmd_flash(args: argparse.Namespace) -> int:
    """
    Flash the entire firmware FPMCU using the native bootloader.

    This requires the Chromebook to be in dev mode with hardware write protect
    disabled.
    """

    # =========================================================================
    # Board specific configuration functions
    # Returns the following:
    #   Transport, Transport Device, FPGpios(NRST, BOOT0, PWREN)

    def config_hatch() -> list:
        # See
        # third_party/coreboot/src/soc/intel/cannonlake/include/soc/
        #   gpio_soc_defs.h
        # for pin name to number mapping.
        return ["SPI", "/dev/spidev1.1",
            get_gpios(FPGpios(NRST=12, BOOT0=22, PWREN=192))]

    def config_herobrine() -> list:
        return ["SPI", "/dev/spidev11.0",
            get_gpios(FPGpios(NRST="FP_RST_L", BOOT0="FPMCU_BOOT0",
                PWREN="EN_FP_RAILS"))]

    def config_nami() -> list:
        return ["SPI", "/dev/spidev32765.0",
            get_gpios(FPGpios(NRST=57, BOOT0=77, PWREN=35))]

    def config_nami_kernelnext() -> list:
        return ["SPI", "/dev/spidev1.0",
            get_gpios(FPGpios(NRST=57, BOOT0=77, PWREN=35))]

    def config_nocturne() -> list:
        return ["SPI", "/dev/spidev32765.0",
            get_gpios(FPGpios(NRST=58, BOOT0=56, PWREN=11))]

    def config_nocturne_kernelnext() -> list:
        return ["SPI", "/dev/spidev1.0",
            get_gpios(FPGpios(NRST=58, BOOT0=56, PWREN=11))]

    def config_strongbad() -> list:
        return ["SPI", "/dev/spidev10.0",
            get_gpios(FPGpios(NRST="FP_RST_L", BOOT0="FPMCU_BOOT0", PWREN=-1))]

    def config_volteer() -> list:
        # See kernel/v5.4/drivers/pinctrl/intel/pinctrl-tigerlake.c
        # for pin name and pin number.
        # Examine `cat /sys/kernel/debug/pinctrl/INT34C5:00/gpio-ranges` on a
        # volteer device to determine gpio number from pin number.
        # For example: GPP_C23 is UART2_CTS which can be queried from EDS
        # the pin number is 194. From the gpio-ranges, the gpio value is
        # 408 + (194-171) = 431
        return ["SPI", "/dev/spidev1.0",
            get_gpios(FPGpios(NRST=194, BOOT0=193, PWREN=63))]

    def config_brya() -> list:
        # See kernel/v5.10/drivers/pinctrl/intel/pinctrl-tigerlake.c
        # for pin name and pin number.
        # Examine `cat /sys/kernel/debug/pinctrl/INTC1055:00/gpio-ranges` on a
        # brya device to determine gpio number from pin number.
        # For example: GPP_D1 is ISH_GP_1 which can be queried from EDS
        # the pin number is 100 from the pinctrl-tigerlake.c.
        # From the gpio-ranges, the gpio value is 312 + (100-99) = 313
        return ["SPI", "/dev/spidev0.0",
            get_gpios(FPGpios(NRST=100, BOOT0=99, PWREN=101))]

    def config_brask() -> list:
        # Let's call the config_brya since brask follows the brya HW design
        return config_brya()

    def config_zork() -> list:
        return ["UART", "/dev/ttyS1",
            get_gpios(FPGpios(NRST=11, BOOT0=69, PWREN=-1))]

    def config_guybrush() -> list:
        return ["UART", "/dev/ttyS1",
            get_gpios(FPGpios(NRST=11, BOOT0=144, PWREN=-1))]

    # =========================================================================

    logging.basicConfig(level=args.log_level)

    # The "platform name" corresponds to the underlying board (reference design)
    # that we're running on (not the FPMCU or sensor). At the moment all of the
    # reference designs use the same GPIOs. If for some reason a design differs
    # in the future, we will want to add a nested check in the
    # config_<platform_name> function.
    # Doing it in this manner allows us to reduce the number of
    # configurations that we have to maintain (and reduces the amount of testing
    # if we're only updating a specific config_<platform_name>).
    args.platform_name = get_platform_name()
    if not args.platform_name:
        logging.error("Failed to get platform name")
        sys.exit(ExitCode.EXIT_CONFIG)

    platform_base_name = get_platform_base_name(args.platform_name)
    if not platform_base_name:
        logging.error("Failed to get platform base name")
        sys.exit(ExitCode.EXIT_CONFIG)

    logging.info(f"Platform name is {args.platform_name} "
        f"({platform_base_name}).")

    # Replace '-' with '_' as Python does not support '-' in function names
    config_func = f"config_{'_'.join(args.platform_name.split('-'))}"
    # Check that the config function exists
    if config_func in locals():
        func = locals()[config_func]
    else:
        config_func = f"config_{platform_base_name}"
        logging.info(config_func)
        if config_func in locals():
            func = locals()[config_func]
        else:
            logging.error(f"No config for platform {args.platform_name}")
            sys.exit(ExitCode.EXIT_CONFIG)

    if not callable(func):
        logging.error(f"No config for platform {args.platform_name}")
        sys.exit(ExitCode.EXIT_CONFIG)

    logging.info(f"Using config for {args.platform_name}.")
    args.transport, args.device, args.gpios = func()

    # Help the user out with defaults, if no *file* was given.
    if not args.binary:
        if args.read:
            now = datetime.datetime.now(datetime.timezone.utc)
            tz = now.astimezone().tzinfo
            now = datetime.datetime.now(tz=tz)
            date = str(now.strftime('%Y-%m-%dT%H:%M:%S%z'))
            args.binary = f"/tmp/fpmcu-fw-{date}.bin"
        else:
            args.binary = get_default_fw()

    if not args.noservices:
        logging.info("# Stopping biod and timberslide")
        run_system_cmd("stop biod", show_output=True)
        run_system_cmd("stop timberslide "
            + "LOG_PATH=/sys/kernel/debug/cros_fp/console_log",
            show_output=True)

    # If cros-ec driver isn't bound on startup, this means the final rebinding
    # may fail.
    if (not os.path.exists("/dev/cros_fp") or not
        stat.S_ISCHR(os.stat("/dev/cros_fp").st_mode)):
        logging.warning("The cros-ec driver was not bound on startup.")

    if (os.path.exists(args.device) and
        stat.S_ISCHR(os.stat(args.device).st_mode)):
        logging.warning(f"The raw driver {args.device} was bound on startup.")

    # Ensure no processes have cros_fp device or debug device open.
    # This might be biod and/or timberslide.
    files_open = proc_open_files("/dev/cros_fp\|/sys/kernel/debug/cros_fp/*")
    if files_open:
        logging.warning(" Another process has a cros_fp device file open.")
        logging.warning(f"{os.linesep.join(files_open)}")
        logging.warning("Try 'stop biod' and")
        logging.warning("'stop timberslide LOG_PATH=/sys/kernel/debug/cros_fp/"
            + "console_log'")
        logging.warning("before running this script.")
        logging.warning("See b/188985272.")

    # Ensure no processes are using the raw driver. This might be a wedged
    # stm32mon process spawned by this script.
    files_open = proc_open_files(args.device)
    if files_open:
        logging.warning(f"Another process has {args.device} open.")
        logging.warning(f"{os.linesep.join(files_open)}")
        logging.warning(f"Try 'fuser -k {args.device}' before running this "
            "script.")
        logging.warning("See b/188985272.")

    #rc = flash_fp_mcu_stm32(args)

    if not args.noservices:
        logging.info("# Restarting biod and timberslide")
        run_system_cmd("start timberslide "
            + "LOG_PATH=/sys/kernel/debug/cros_fp/console_log",
            show_output=True)
        run_system_cmd("start biod", show_output=True)

    return rc

def flash_init(parser: argparse.ArgumentParser) -> argparse.ArgumentParser:
    flash_parser = parser.add_parser('flash', help=cmd_flash.__doc__)
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument("-r", "--read", action='store_true')
    group.add_argument("--noread", action='store_true',
            help="Read instead of write (Default: False)")
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument("-U", "--remove_flash_read_protect", action='store_true',
            default=True)
    group.add_argument("--noremove_flash_read_protect", action='store_true',
            help="Remove flash read protection while performing command "
            "(Default: True)")
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument("-u", "--remove_flash_write_protect",
            action='store_true', default=True)
    group.add_argument("--noremove_flash_write_protect", action='store_true',
            help="Remove flash read protection while "
            "performing command (Default: True)")
    flash_parser.add_argument("-R", "--retries", type=int, default=4,
            help="Specify number of retries (default: %(default)s)")
    flash_parser.add_argument("-B", "--baudrate", type=int, default=115200,
            help="Specify UART baudrate (default: %(default)s)")
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument("-H", "--hello", action='store_true')
    group.add_argument("--nohello", action='store_true',
            help="Only ping the bootloader (Default: %(default)s)")
    group = flash_parser.add_mutually_exclusive_group()
    group.add_argument("-s", "--services", default=True, action='store_true')
    group.add_argument("--noservices", action='store_true',
            help="Stop and restart conflicting fingerprint services "
            "(Default: True)")
    flash_parser.add_argument("binary", type=str, nargs='?',
            help="Flash binary [ec.bin]")
    flash_parser.set_defaults(func=cmd_flash, connect_retries=6)
    log_level_choices = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
    flash_parser.add_argument(
        '--log_level', '-l',
        choices=log_level_choices,
        default='INFO'
    )
    return flash_parser

def main() -> int:
    # print out canonical path to differentiate between /usr/local/bin and
    # /usr/bin installs
    run_system_cmd(f"readlink -f {sys.argv[0]}", show_output=True)
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest='subcommand', title='subcommands')
    # This method of setting required is more compatible with older python.
    subparsers.required = True

    parser_decrypt = flash_init(subparsers)
    opts = parser.parse_args()

    return opts.func(opts)

if __name__ == '__main__':
    sys.exit(main())
