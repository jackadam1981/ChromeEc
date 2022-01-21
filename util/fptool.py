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
from typing import List, Dict
import re
from platform import release
import datetime
import stat
import time


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


def ver_ge(ver: str) -> bool:
    major = int(ver.split('.')[0])
    minor = int(ver.split('.')[1])
    ver = release().split('-')[0]
    ver_major = int(ver.split('.')[0])
    ver_minor = int(ver.split('.')[1])
    return ver_major > major or ver_major == major and ver_minor >= minor


class FPGpios:
    """Common GPIOs structure to be initiated per platform"""

    n_reset = None
    boot_0 = None
    power_enable = None

    class Gpio:
        """Single GPIO control class"""

        _gpio = None
        _binded = False

        def _get_gpio_by_index(self, ranges: List[dict], idx: int) -> int:
            """Convert gpio index to its absolute location in the device.

            The absolute location is calculated as follows:
                Absolute location = gpio index - first index in the range + base
            """
            logging.debug('_get_gpio_by_index - looking for gpio %d', idx)
            if idx < 0:
                return -1
            for gpio_range in ranges:
                if idx > gpio_range['seq_idx']['last']:
                    continue
                if idx < gpio_range['seq_idx']['first']:
                    continue
                logging.debug('\tFound: %d', gpio_range['gpio']['first'] + idx
                              - gpio_range['seq_idx']['first'])
                return (gpio_range['gpio']['first'] + idx
                        - gpio_range['seq_idx']['first'])

            logging.error('GPIO pin index %d does not belong to any supported '
                          'range', idx)
            logging.error('Supported ranges:')
            for gpio_range in ranges:
                logging.error('\t%d - %d', gpio_range['seq_idx']['first'],
                              gpio_range['seq_idx']['last'])
            sys.exit(ExitCode.EXIT_RUNTIME)

        def _gpiofind(self, name: str) -> [int, str]:
            rc, stdout, _stderr = run_system_cmd(f'gpiofind {name}')
            return rc, stdout

        def _get_gpio_by_name(self, bases: dict, name: str) -> int:
            """Convert gpio's name to its absolute location in the device.

            Utilizes libgpiod APIs 'gpiofind' and 'gpiodetect'
            """
            rc, raw_find = self._gpiofind(name)
            if not bases or rc != 0:
                logging.error('Failed to find GPIO %s', name)
                sys.exit(ExitCode.EXIT_RUNTIME)
            gpio = raw_find.split()
            if len(gpio) < 2:
                logging.error('Error parsing `gpiofind` %s', raw_find)
                sys.exit(ExitCode.EXIT_RUNTIME)
            if gpio[0] not in bases:
                logging.error('Failed to locate %s in `gpiodetect`', gpio[0])
                sys.exit(ExitCode.EXIT_RUNTIME)
            # e.g.
            #   gpiofind FP_RST_L
            #   gpiochip0 22
            #
            # Gpio location is: 22 + bases['gpiochip0']
            return int(gpio[1]) + int(bases[gpio[0]])

        def _get_gpio(self, ranges: List[dict], bases: dict, gpio) -> int:
            if isinstance(gpio, int):
                return self._get_gpio_by_index(ranges, gpio)
            return self._get_gpio_by_name(bases, gpio)

        def _export(self, export: bool):
            cmd = 'export' if export else 'unexport'
            klog(f'Set gpio {self._gpio} to {cmd}')
            writeline(f'/sys/class/gpio/{cmd}', self._gpio)

        def _direction(self, out: bool):
            cmd = 'out' if out else 'in'
            klog(f'Set gpio {self._gpio} direction to {cmd}')
            writeline(f'/sys/class/gpio/gpio{self._gpio}/direction', cmd)

        def _value_set(self, _set: bool):
            cmd = '1' if _set else '0'
            klog(f'Set gpio {self._gpio} to {cmd}')
            writeline(f'/sys/class/gpio/gpio{self._gpio}/value', cmd)

        def _value_get(self) -> str:
            klog(f'Get gpio {self._gpio}')
            return readline(f'/sys/class/gpio/gpio{self._gpio}/value')

        def _bind(self):
            if not self._binded:
                self._export(export=True)
                self._binded = True
            self._direction(out=True)

        def _precheck(self):
            if not self._gpio:
                logging.error('Attemt to access non-existent GPIO')
                sys.exit(ExitCode.EXIT_RUNTIME)

        def set(self):
            self._precheck()
            self._bind()
            self._value_set(_set=True)

        def clear(self):
            self._precheck()
            self._bind()
            self._value_set(_set=False)

        def verify(self, val: int) -> bool:
            self._precheck()
            current_value = self._value_get()
            if not current_value:
                logging.error('Error reading gpio %s value', self._gpio)
                sys.exit(ExitCode.EXIT_RUNTIME)
            return self._value_get() == str(val)

        def release(self, tri_state: bool):
            self._precheck()
            if not self._binded:
                logging.error('Attemt to unexport an unexported GPIO')
                sys.exit(ExitCode.EXIT_RUNTIME)
            if tri_state:
                self._direction(out=False)
            self._export(export=False)
            self._binded = False

        def exists(self):
            return self._gpio is not None

        def __init__(self, ranges: List[dict], bases: dict, gpio):
            _gpio = self._get_gpio(ranges, bases, gpio)
            self._gpio = str(_gpio) if _gpio > -1 else None

    def _gpiodetect(self) -> [int, str]:
        rc, stdout, _stderr = run_system_cmd('gpiodetect')
        return rc, stdout

    def _read_gpio_ranges(self) -> List[str]:
        ranges = []
        path = glob.glob('/sys/kernel/debug/pinctrl/*/gpio-ranges')
        for filename in path:
            try:
                with open(filename, 'r') as fp:
                    for line in fp.readlines():
                        ranges.append(line)
            except OSError:
                logging.warning('------------------ Error reading from %s',
                                filename)
                logging.warning('\t\tOSError: %s', sys.exc_info()[1].strerror)

        logging.debug('raw gpio ranges: %s', ranges)
        return ranges

    def _parse_gpio_ranges(self) -> List[dict]:
        """Parse the GPIO ranges from "/sys/kernel/debug/pinctrl/*/gpio-ranges"

        Used when gpios are referenced by index
        The result is a list of dictionaries:
        ranges:
            gpio            pin             seq_idx
            first   last    first   last    first   last

            gpio[first] is the base index for the gpios range
            pin[first] - pin[last] is the sequential index of the gpio in the
            range seq_idx[first] - seq_idx[last] is the sequential global index
                of the gpio

        The list is sorted in ascending order of the base index (gpio[first])
        """
        # Ranges example:
        #   $ cat /sys/kernel/debug/pinctrl/*/gpio-ranges
        #   GPIO ranges handled:
        #   0: INT1055:00 GPIOS [344 - 355] PINS [0 - 11]
        #   15: INT1055:00 GPIOS [359 - 370] PINS [15 - 26]
        #   GPIO ranges handled:
        #   0: INT1055:01 GPIOS [288 - 295] PINS [0 - 7]
        #   15: INT1055:01 GPIOS [303 - 314] PINS [15 - 26]
        #   30: INT1055:01 GPIOS [318 - 323] PINS [30 - 35]
        #   45: INT1055:01 GPIOS [333 - 340] PINS [45 - 52]
        #   GPIO ranges handled:
        #   0: INTC1055:02 GPIOS [152 - 177] PINS [0 - 25]
        #   32: INTC1055:02 GPIOS [178 - 193] PINS [26 - 41]
        #   64: INTC1055:02 GPIOS [194 - 218] PINS [42 - 66]
        #   96: INTC1055:02 GPIOS [219 - 226] PINS [67 - 74]
        #   128: INTC1055:02 GPIOS [227 - 250] PINS [75 - 98]
        #
        # Extracting the following:
        #   device name: 'INTC1055:01'
        #   Range's first and last gpios: gpio: [318, 323]
        #   Chip's first and last pin associated with the GPIOs range:
        #       pin: [30, 35]
        #
        # As the ranges might be unsorted and not sequential, sort and calculate
        # the sequential order of the range
        # Gpio global sequential range: [129, 134]
        #   This is calculated as follows:
        #   INTC1055:02 range: seq_idx: [0, 98]
        #   INTC1055:01 range: seq_idx: [99, 151]
        #   INTC1055:00 range: seq_idx: [152, 178]
        #
        # The resulting dictionary list is:
        # [{'device': 'INTC1055:02',
        #   'gpio': {'first': 152, 'last': 177},
        #   'pin': {'first': 0, 'last': 25},
        #   'seq_idx': {'first': 0, 'last': 25}},
        #  {'device': 'INTC1055:02',
        #   'gpio': {'first': 178, 'last': 193},
        #   'pin': {'first': 26, 'last': 41},
        #   'seq_idx': {'first': 26, 'last': 41}},
        #  {'device': 'INTC1055:02',
        #   'gpio': {'first': 194, 'last': 218},
        #   'pin': {'first': 42, 'last': 66},
        #   'seq_idx': {'first': 42, 'last': 66}},
        #  {'device': 'INTC1055:02',
        #   'gpio': {'first': 219, 'last': 226},
        #   'pin': {'first': 67, 'last': 74},
        #   'seq_idx': {'first': 67, 'last': 74}},
        #  {'device': 'INTC1055:02',
        #   'gpio': {'first': 227, 'last': 250},
        #   'pin': {'first': 75, 'last': 98},
        #   'seq_idx': {'first': 75, 'last': 98}},
        #  {'device': 'INT1055:01',
        #   'gpio': {'first': 288, 'last': 295},
        #   'pin': {'first': 0, 'last': 7},
        #   'seq_idx': {'first': 99, 'last': 106}},
        #  {'device': 'INT1055:01',
        #   'gpio': {'first': 303, 'last': 314},
        #   'pin': {'first': 15, 'last': 26},
        #   'seq_idx': {'first': 114, 'last': 125}},
        #  {'device': 'INT1055:01',
        #   'gpio': {'first': 318, 'last': 323},
        #   'pin': {'first': 30, 'last': 35},
        #   'seq_idx': {'first': 129, 'last': 134}},
        #  {'device': 'INT1055:01',
        #   'gpio': {'first': 333, 'last': 340},
        #   'pin': {'first': 45, 'last': 52},
        #   'seq_idx': {'first': 144, 'last': 151}},
        #  {'device': 'INT1055:00',
        #   'gpio': {'first': 344, 'last': 355},
        #   'pin': {'first': 0, 'last': 11},
        #   'seq_idx': {'first': 152, 'last': 163}},
        #  {'device': 'INT1055:00',
        #   'gpio': {'first': 359, 'last': 370},
        #   'pin': {'first': 15, 'last': 26},
        #   'seq_idx': {'first': 167, 'last': 178}}
        #  ]

        ranges = []
        for line in self._read_gpio_ranges():
            d = {}
            s = re.split('[0-9]+: (\\S*) GPIOS \\[([0-9]*) - '
                         '([0-9]*)\\] PINS \\[([0-9]*) - '
                         '([0-9]*)\\]', line, 1)
            if len(s) < 7:
                continue
            d['device'] = s[1]
            d['gpio'] = {}
            d['gpio']['first'] = int(s[2])
            d['gpio']['last'] = int(s[3])
            d['pin'] = {}
            d['pin']['first'] = int(s[4])
            d['pin']['last'] = int(s[5])
            ranges.append(d)

        # Sort the range
        ranges = sorted(ranges, key=lambda x: x['gpio']['first'])

        # Calculate the sequential global index
        prev = {}
        for r in ranges:
            r['seq_idx'] = {}
            if not prev:
                start = 0
            elif r['pin']['first'] <= prev['pin']['last']:
                # Abnormal case where the PINS range is not sequential
                start = prev['seq_idx']['last'] + 1
            else:
                # Jump to the next range - update the start index
                start = prev['seq_idx']['first'] - prev['pin']['first']
            r['seq_idx']['first'] = r['pin']['first'] + start
            r['seq_idx']['last'] = r['pin']['last'] + start
            prev = r

        logging.debug('parsed gpio ranges: %s', ranges)
        return ranges

    def _parse_gpiochips(self) -> Dict[str, str]:
        """Parse the base and label of each gpio chip.

        Used when gpios are referenced by name
        The result is a dictionary of labels (device names) and bases
        bases
            device1: base1
            device2: base2
        """
        bases = {}
        detect = {}
        rc, raw_detect = self._gpiodetect()
        if rc != 0 or not raw_detect:
            return {}
        # 'gpiodetect' example:
        #
        #  # gpiodetect
        #    gpiochip0 [INTC1055:00] (27 lines)
        #    gpiochip1 [INTC1055:01] (53 lines)
        #    gpiochip2 [INTC1055:02] (99 lines)
        #
        # 'label' example:
        #
        # # cat /sys/class/gpio/gpiochip*/label
        # INTC1055:02
        # INTC1055:01
        # INTC1055:00
        #
        # 'base' example:
        #
        # # cat /sys/class/gpio/gpiochip*/base
        # 152
        # 288
        # 344
        #
        # These will be parsed to:
        #
        #  detect = {
        #       'INTC1055:00': 'gpiochip0',
        #       'INTC1055:01': 'gpiochip1',
        #       'INTC1055:02': 'gpiochip2'
        #  }
        #
        # bases = {
        #       'gpiochip0': 344
        #       'gpiochip1': 288
        #       'gpiochip2': 152
        # }
        for device in raw_detect.split('\n'):
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

    def __init__(self, n_reset, boot_0, power_enable=-1):
        self.bases = self._parse_gpiochips()
        self.ranges = self._parse_gpio_ranges()

        self.n_reset = self.Gpio(self.ranges, self.bases, n_reset)
        self.boot_0 = self.Gpio(self.ranges, self.bases, boot_0)
        self.power_enable = self.Gpio(self.ranges, self.bases, power_enable)


class ConfigPlatform:
    """Board specific configuration functions

    Holds the following:
        Transport, Transport Device, FPGpios
    """
    # Board specific configuration functions
    # Returns the following:
    # Transport, Transport Device, FPGpios(n_reset, boot_0, power_enable)

    def _config_hatch(self) -> dict:
        # See
        # third_party/coreboot/src/soc/intel/cannonlake/include/soc/
        #   gpio_soc_defs.h
        # for pin name to number mapping.
        return {'TRANSPORT': 'SPI',
                'DEVICE':    '/dev/spidev1.1',
                'GPIOS':     FPGpios(n_reset=12, boot_0=22,
                                     power_enable=192)}

    def _config_herobrine(self) -> dict:
        return {'TRANSPORT': 'SPI',
                'DEVICE':    '/dev/spidev9.0',
                'GPIOS':     FPGpios(n_reset='FP_RST_L', boot_0='FPMCU_BOOT0',
                                     power_enable='EN_FP_RAILS')}

    def _config_nami(self) -> dict:
        return {'TRANSPORT': 'SPI',
                'DEVICE':    '/dev/spidev1.0' if ver_ge('4.4')
                             else '/dev/spidev32765.0',
                'GPIOS':     FPGpios(n_reset=57, boot_0=77, power_enable=35)}

    def _config_nami_kernelnext(self) -> dict:
        return self._config_nami()

    def _config_nocturne(self) -> dict:
        return {'TRANSPORT': 'SPI',
                'DEVICE':    '/dev/spidev32765.0',
                'GPIOS':     FPGpios(n_reset=58, boot_0=56, power_enable=11)}

    def _config_nocturne_kernelnext(self) -> dict:
        return {'TRANSPORT': 'SPI',
                'DEVICE':    '/dev/spidev1.0',
                'GPIOS':     FPGpios(n_reset=58, boot_0=56, power_enable=11)}

    def _config_strongbad(self) -> dict:
        return {'TRANSPORT': 'SPI',
                'DEVICE':    '/dev/spidev10.0',
                'GPIOS':     FPGpios(n_reset='FP_RST_L', boot_0='FPMCU_BOOT0',
                                     power_enable='FPMCU_SEL')}

    def _config_volteer(self) -> dict:
        # See kernel/v5.4/drivers/pinctrl/intel/pinctrl-tigerlake.c
        # for pin name and pin number.
        # Examine `cat /sys/kernel/debug/pinctrl/INT34C5:00/gpio-ranges`
        # on a volteer device to determine gpio number from pin number.
        # For example: GPP_C23 is UART2_CTS which can be queried from EDS
        # the pin number is 194. From the gpio-ranges, the gpio value is
        # 408 + (194-171) = 431
        return {'TRANSPORT': 'SPI',
                'DEVICE':    '/dev/spidev1.0',
                'GPIOS':     FPGpios(n_reset=194, boot_0=193, power_enable=63)}

    def _config_brya(self) -> dict:
        # See kernel/v5.10/drivers/pinctrl/intel/pinctrl-tigerlake.c
        # for pin name and pin number.
        # Examine `cat /sys/kernel/debug/pinctrl/INTC1055:00/gpio-ranges`
        # on a brya device to determine gpio number from pin number.
        # For example: GPP_D1 is ISH_GP_1 which can be queried from EDS
        # the pin number is 100 from the pinctrl-tigerlake.c.
        # From the gpio-ranges, the gpio value is 312 + (100-99) = 313
        return {'TRANSPORT': 'SPI',
                'DEVICE':    '/dev/spidev0.0',
                'GPIOS':     FPGpios(n_reset=100, boot_0=99, power_enable=101)}

    def _config_brask(self) -> dict:
        # Let's call the config_brya since brask follows the brya HW design
        return self.config_brya()

    def _config_zork(self) -> dict:
        return {'TRANSPORT': 'UART',
                'DEVICE':    '/dev/ttyS1',
                'GPIOS':     FPGpios(n_reset=11, boot_0=69, power_enable=-1)}

    def _config_guybrush(self) -> dict:
        return {'TRANSPORT': 'UART',
                'DEVICE':    '/dev/ttyS1',
                'GPIOS':     FPGpios(n_reset=11, boot_0=144, power_enable=-1)}

    _config_funcs = {
        'hatch':                _config_hatch,
        'herobrine':            _config_herobrine,
        'nami':                 _config_nami,
        'nami-kernelnext':      _config_nami_kernelnext,
        'nocturne':             _config_nocturne,
        'nocturne-kernelnext':  _config_nocturne_kernelnext,
        'strongbad':            _config_strongbad,
        'volteer':              _config_volteer,
        'brya':                 _config_brya,
        'brask':                _config_brask,
        'zork':                 _config_zork,
        'guybrush':             _config_guybrush,
    }

    def transport(self) -> str:
        return self._config['TRANSPORT']

    def device(self) -> str:
        return self._config['DEVICE']

    def gpios(self) -> FPGpios:
        return self._config['GPIOS']

    def __new__(cls, platform: str):
        if not platform or platform not in ConfigPlatform._config_funcs:
            return None
        return super().__new__(cls)

    def __init__(self, platform: str):
        self._config = self._config_funcs[platform](self)


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


def char_device_exists(char_device: str) -> bool:
    return (os.path.exists(char_device) and
            stat.S_ISCHR(os.stat(char_device).st_mode))


def flash_fp_mcu_stm32(args):
    """Main flashing routine."""
    cmd = ['stm32mon']
    cmd.append(f'-p --retries {args.connect_retries}')
    if args.config.transport() == 'UART':
        cmd.append(f'--baudrate {args.baudrate}')
        cmd.append(f'--device {args.config.device()}')
    else:
        cmd.append(f'-s {args.config.device()}')

    if args.hello:
        logging.info('# Saying hello over %s', args.config.transport())
    else:
        if args.remove_flash_write_protect:
            # Remove Write protect
            cmd.append('-u')
        if args.remove_flash_read_protect:
            # Remove Read protect
            cmd.append('-U')
        if args.read:
            # Read from FPMCU to file
            if os.path.isfile(args.binary):
                logging.error('Output file already exists: %s', args.binary)
                sys.exit(ExitCode.EXIT_PRECONDITION)
            logging.info('# Reading to {args.binary} over %s',
                         args.config.transport())
            cmd.append(f'-r {args.binary}')
        elif os.path.isfile(args.binary):
            # Write to FPMCU from file
            logging.info('# Flashing "%s" over %s', args.binary,
                         args.config.transport())
            cmd.append(f'-e -w {args.binary}')
        else:
            logging.error('Invalid image file: %s', args.binary)
            sys.exit(ExitCode.EXIT_PRECONDITION)

    assert_wp_is_disabled()

    if args.config.transport() == 'UART':
        device_id = get_uartid()
    else:
        device_id = get_spiid()

    if not device_id:
        logging.error('Unable to find FP sensor %s device',
                      args.config.transport())
        sys.exit(ExitCode.EXIT_PRECONDITION)

    logging.info('Flashing %s device ID: %s', args.config.transport(),
                 device_id)

    # Remove cros_fp if present
    klog('Unbinding cros-ec driver')
    if args.config.transport() == 'UART':
        unbind_file = '/sys/bus/serial/drivers/cros-ec-uart/unbind'
    else:
        unbind_file = '/sys/bus/spi/drivers/cros-ec-spi/unbind'
    writeline(unbind_file, device_id)

    # Reset sequence to enter bootloader mode
    args.config.gpios().boot_0.set()
    args.config.gpios().n_reset.clear()
    time.sleep(0.001)

    klog('Binding raw driver')
    if args.config.transport() == 'UART':
        dev_name = get_uart_dev_name(device_id)
        logging.info('Serial device: %s', dev_name)
        # load AMDI0020:01 ttyS1
        writeline('/sys/bus/platform/drivers/dw-apb-uart/unbind', dev_name)
        writeline('/sys/bus/platform/drivers/dw-apb-uart/bind', dev_name)
    else:
        driver_override = f'/sys/bus/spi/devices/{device_id}/driver_override'
        writeline(driver_override, 'spidev')
        writeline('/sys/bus/spi/drivers/spidev/bind', device_id)

    # The following sleep is a workaround to mitigate the effects of a
    # poorly behaved chip select line. See b/145023809.
    time.sleep(0.5)

    # We do not expect the drivers to change the pin state when binding.
    # If you receive this warning, the driver needs to be fixed on this board
    # and this flash attempt will probably fail.
    if not args.config.gpios().boot_0.verify(1):
        logging.warning('One of the drivers changed BOOT0 pin state on bind '
                        'attempt.')

    if not args.config.gpios().n_reset.verify(0):
        logging.warning('One of the drivers changed NRST pin state on bind '
                        'attempt.')

    if not char_device_exists(args.config.device()):
        logging.error('Failed to bind raw device driver.')
        sys.exit(ExitCode.EXIT_RUNTIME)

    rc = 0
    for attempt in range(args.retries):
        # Reset sequence to enter bootloader mode
        args.config.gpios().n_reset.clear()
        time.sleep(0.01)
        # Release reset as the SPI bus is now ready
        args.config.gpios().n_reset.set()

        # As per section '68: Bootloader timings' from application note below:
        # https://www.st.com/resource/en/application_note/cd00167594-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf
        # bootloader startup time is 16.63 ms for STM32F74xxx/75xxx and
        # 53.975 ms for STM32H74xxx/75xxx.
        # SPI needs 1 us delay for sending one SPI byte.
        # Keeping some margin, add delay of 100 ms to consider minimum
        # bootloader startup time after the reset of stm32 devices.
        time.sleep(0.1)

        # Print out the actual underlying command we're running and run it
        logging.info('# %s', ' '.join(cmd))
        rc, _, _ = run_system_cmd(' '.join(cmd), show_output=True)
        if rc == 0:
            break
        logging.info('Attempt %d failed.', attempt)
        time.sleep(1)

    # unload device
    if args.config.transport() != 'UART':
        klog('Unbinding raw driver')
        writeline('/sys/bus/spi/drivers/spidev/unbind', device_id)

    # Go back to normal mode
    args.config.gpios().boot_0.clear()
    args.config.gpios().n_reset.clear()
    args.config.gpios().n_reset.set()

    # Give up GPIO control, unless we need to keep these driving as
    # outputs because they're not open-drain signals.
    # TODO(b/179839337): Make this the default and properly support
    # open-drain outputs on other platforms.
    if args.platform_name != 'strongbad' and args.platform_name != 'herobrine':
        args.config.gpios().boot_0.release(tri_state=True)
        args.config.gpios().n_reset.release(tri_state=True)
    else:
        args.config.gpios().boot_0.release(tri_state=False)
        args.config.gpios().n_reset.release(tri_state=False)

    # Dartmonkey's RO has a flashprotect logic issue that forces reboot loops
    # when SW-WP is enabled and HW-WP is disabled. It is avoided if a POR is
    # detected on boot. We force a POR here to ensure we avoid this reboot loop.
    # See to b/146428434.
    if args.config.gpios().power_enable.exists():
        logging.info('Power cycling the FPMCU.')
        args.config.gpios().power_enable.clear()
        # Must outlast hardware soft start, which is typically ~3ms.
        time.sleep(0.5)
        args.config.gpios().power_enable.set()
        # Power enable line is externally pulled down, so leave as output-high.
        args.config.gpios().power_enable.release(tri_state=False)

    # Put back cros_fp driver if transport is SPI
    if args.config.transport() != 'UART':
        # wait for FP MCU to come back up (including RWSIG delay)
        time.sleep(2)
        klog('Binding cros-ec driver')
        driver_override = f'/sys/bus/spi/devices/{device_id}/driver_override'
        writeline(driver_override, '')
        writeline('/sys/bus/spi/drivers/cros-ec-spi/bind', device_id)

    if rc != 0:
        return ExitCode.EXIT_RUNTIME

    # Inform user to reboot if transport is UART.
    # Display fw version is transport is SPI
    if args.config.transport() == 'UART':
        logging.warning('Please reboot this device.')
    else:
        # Test it
        klog('Query version and reset flags')
        run_system_cmd('ectool --name=cros_fp version', show_output=True)
        run_system_cmd('ectool --name=cros_fp uptimeinfo', show_output=True)


def cmd_flash(args: argparse.Namespace) -> int:
    """Flash the entire firmware FPMCU using the built-in bootloader.

    This requires the Chromebook to be in dev mode with hardware write protect
    disabled.
    """

    logging.basicConfig(level=args.log_level)

    # The 'platform name' corresponds to the underlying board (reference design)
    # that we're running on (not the FPMCU or sensor). At the moment all of the
    # reference designs use the same GPIOs. If for some reason a design differs
    # in the future, we will want to add a nested check in the
    # config_<platform_name> function.
    # Doing it in this manner allows us to reduce the number of
    # configurations that we have to maintain (and reduces the amount of testing
    # if we're only updating a specific config_<platform_name>).
    args.platform_name = get_platform_name()
    if not args.platform_name:
        logging.error('Failed to get platform name')
        sys.exit(ExitCode.EXIT_CONFIG)

    platform_base_name = get_platform_base_name(args.platform_name)
    if not platform_base_name:
        logging.error('Failed to get platform base name')
        sys.exit(ExitCode.EXIT_CONFIG)

    logging.info('Platform name is %s (%s)', args.platform_name,
                 platform_base_name)

    logging.info('Using config for %s', args.platform_name)
    args.config = ConfigPlatform(args.platform_name)
    if not args.config:
        args.config = ConfigPlatform(args.platform_base_name)
        if not args.config:
            logging.error('No config for platform %s', args.platform_name)
            sys.exit(ExitCode.EXIT_CONFIG)

    # Help the user out with defaults, if no *file* was given.
    if not args.binary:
        if args.read:
            date = datetime.datetime.now().isoformat()
            args.binary = f'/tmp/fpmcu-fw-{date}.bin'
        else:
            args.binary = get_default_firmware()

    if args.services:
        logging.info('# Stopping biod and timberslide')
        run_system_cmd('stop biod', show_output=True)
        run_system_cmd('stop timberslide '
                       'LOG_PATH=/sys/kernel/debug/cros_fp/console_log',
                       show_output=True)

    # If cros-ec driver isn't bound on startup, this means the final rebinding
    # may fail.
    if not char_device_exists('/dev/cros_fp'):
        logging.warning('The cros-ec driver was not bound on startup.')

    if char_device_exists(args.config.device()):
        logging.warning('The raw driver %s was bound on startup.',
                        args.config.device())

    # Ensure no processes have cros_fp device or debug device open.
    # This might be biod and/or timberslide.
    files_open = proc_open_files('/dev/cros_fp', '/sys/kernel/debug/cros_fp/*')
    if files_open:
        logging.warning(' Another process has a cros_fp device file open.')
        logging.warning('%s', os.linesep.join(files_open))
        logging.warning('Try "stop biod" and')
        logging.warning('"stop timberslide '
                        'LOG_PATH=/sys/kernel/debug/cros_fp/console_log"')
        logging.warning('before running this script.')
        logging.warning('See b/188985272.')

    # Ensure no processes are using the raw driver. This might be a wedged
    # stm32mon process spawned by this script.
    files_open = proc_open_files(args.config.device())
    if files_open:
        logging.warning('Another process has %s open.', args.config.device())
        logging.warning('%s', os.linesep.join(files_open))
        logging.warning('Try "fuser -k %s" before running this script.',
                        args.config.device())
        logging.warning('See b/188985272.')

    rc = flash_fp_mcu_stm32(args)

    if args.services:
        logging.info('# Restarting biod and timberslide')
        run_system_cmd('start timberslide '
                       'LOG_PATH=/sys/kernel/debug/cros_fp/console_log',
                       show_output=True)
        run_system_cmd('start biod', show_output=True)

    return rc


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
