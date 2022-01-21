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
import datetime
import stat
import time


class ExitCode:
    """Exit Codes."""
    EXIT_ARGUMENT = 3
    EXIT_CONFIG = 4
    EXIT_PRECONDITION = 5
    EXIT_RUNTIME = 6


class FPGpios:
    """Common GPIO structure to be initiated per platform."""
    NRST = -1
    BOOT0 = -1
    PWREN = -1
    def __init__(self, NRST, BOOT0, PWREN=-1):
        self.NRST = NRST
        self.BOOT0 = BOOT0
        self.PWREN = PWREN


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
    gpio_str = stdout.split()
    # e.g.
    #   gpiofind FP_RST_L
    #   gpiochip0 22
    #
    # Gpio location is: 22 + bases['gpiochip0']
    return int(gpio_str[1]) + int(bases[gpio_str[0]])


def get_gpio(ranges, bases, ngpio) -> int:
    if isinstance(ngpio, int):
        return get_gpio_by_index(ranges, ngpio)
    return get_gpio_by_name(bases, ngpio)


def get_gpios(gpios: FPGpios) -> FPGpios:
    bases = read_gpiochips()
    ranges = read_gpio_ranges()
    for key in vars(gpios):
        vars(gpios)[key] = get_gpio(ranges, bases, vars(gpios)[key])
    return gpios


def gpio(cmd, *args) -> list:
    """Usage: gpio <unexport|export|in|out|0|1|get> <signal> [signal...]"""

    # ==============================================================
    # Internal commands
    def export(cmd: str, signal: int):
        klog(f'Set gpio {signal} to {cmd}')
        writeline('/sys/class/gpio/' + cmd, str(signal))

    def direction(cmd: str, signal: int):
        klog(f'Set gpio {signal} direction to {cmd}')
        writeline(f'/sys/class/gpio/gpio{signal}/direction', cmd)

    def value_set(cmd: str, signal: int):
        klog(f'Set gpio {signal} to {cmd}')
        writeline(f'/sys/class/gpio/gpio{signal}/value', cmd)

    def value_get(_cmd: str, signal: int):
        klog(f'Get gpio {signal}')
        return readline(f'/sys/class/gpio/gpio{signal}/value')
    # ==============================================================

    responses = []
    cmds = {'export' : export,
            'unexport' : export,
            'in' : direction,
            'out' : direction,
            '0' : value_set,
            '1' : value_set,
            'get' : value_get,
            }
    if not cmd in cmds:
        logging.error('Invalid gpio command: %s', cmd)
        sys.exit(ExitCode.EXIT_RUNTIME)
    for signal in args:
        response = cmds[cmd](cmd, signal)
        if response:
            responses.append(response)
    return responses


def warn_gpio(signal: str, expected_value: str, msg: str):
    value = gpio('get', signal)
    if not value:
        logging.error('Error reading gpio %s value', signal)
        sys.exit(ExitCode.EXIT_RUNTIME)
    if value[0] != expected_value:
        logging.warning(msg)


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


def flash_fp_mcu_stm32(args):
    """Main flashing routine."""
    stm32_flags = f'-p --retries {args.connect_retries}'
    if args.config.transport == 'UART':
        stm32_flags += f' --baudrate {args.baudrate}'
        stm32_flags += f' --device {args.config.device}'
    else:
        stm32_flags += f' -s {args.config.device}'

    if args.hello:
        logging.info('# Saying hello over %s', args.config.transport)
    else:
        if not args.noremove_flash_write_protect:
            # Remove Write protect
            stm32_flags += ' -u'
        if not args.noremove_flash_read_protect:
            # Remove Read protect
            stm32_flags += ' -U'
        if args.read:
            # Read from FPMCU to file
            if os.path.isfile(args.binary):
                logging.error('Output file already exists: %s', args.binary)
                sys.exit(ExitCode.EXIT_PRECONDITION)
            logging.info('# Reading to {args.binary} over %s',
                         args.config.transport)
            stm32_flags += f' -r {args.binary}'
        elif os.path.isfile(args.binary):
            # Write to FPMCU from file
            logging.info('# Flashing "%s" over %s', args.binary,
                         args.config.transport)
            stm32_flags += f' -e -w {args.binary}'
        else:
            logging.error('Invalid image file: %s', args.binary)
            sys.exit(ExitCode.EXIT_PRECONDITION)

    # Assert that write protect is disabled
    assert_wp_is_disabled()

    if args.config.transport == 'UART':
        device_id = get_uartid()
    else:
        device_id = get_spiid()

    if not device_id:
        logging.error('Unable to find FP sensor %s device',
                      args.config.transport)
        sys.exit(ExitCode.EXIT_PRECONDITION)

    logging.info('Flashing %s device ID: %s', args.config.transport, device_id)

    # Remove cros_fp if present
    klog('Unbinding cros-ec driver')
    if args.config.transport == 'UART':
        unbind_file = '/sys/bus/serial/drivers/cros-ec-uart/unbind'
    else:
        unbind_file = '/sys/bus/spi/drivers/cros-ec-spi/unbind'
    writeline(unbind_file, device_id)

    # Configure the MCU Boot0 and NRST GPIOs
    gpio('export', args.config.gpios.BOOT0, args.config.gpios.NRST)
    gpio('out', args.config.gpios.BOOT0, args.config.gpios.NRST)

    # Reset sequence to enter bootloader mode
    gpio('1', args.config.gpios.BOOT0)
    gpio('0', args.config.gpios.NRST)
    time.sleep(0.001)

    klog('Binding raw driver')
    if args.config.transport == 'UART':
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
    warn_gpio(f'{args.config.gpios.BOOT0}', '1',
              'One of the drivers changed BOOT0 pin state on bind attempt.')
    warn_gpio(f'{args.config.gpios.NRST}', '0',
              'One of the drivers changed NRST pin state on bind attempt.')

    if (not os.path.exists(args.config.device)
            or not stat.S_ISCHR(os.stat(args.config.device).st_mode)):
        logging.error('Failed to bind raw device driver.')
        sys.exit(ExitCode.EXIT_RUNTIME)

    cmd = f'stm32mon {stm32_flags}'
    rc = 0
    for attempt in range(args.retries):
        # Reset sequence to enter bootloader mode
        gpio('0', args.config.gpios.NRST)
        time.sleep(0.01)
        # Release reset as the SPI bus is now ready
        gpio('1', args.config.gpios.NRST)

        # As per section '68: Bootloader timings' from application note below:
        # https://www.st.com/resource/en/application_note/
        #   cd00167594-stm32-microcontroller-system-memory-boot-mode-
        #   stmicroelectronics.pdf
        # bootloader startup time is 16.63 ms for STM32F74xxx/75xxx
        # and 53.975 msfor STM32H74xxx/75xxx.
        # SPI needs 1 us delay for one SPI byte sending.
        # Keeping some margin, add delay of 100 ms to consider minimum
        # bootloader
        # startup time after the reset for stm32 devices.
        time.sleep(0.1)

        # Print out the actual underlying command we're running and run it
        logging.info('# %s', cmd)
        rc, _, _ = run_system_cmd(cmd, show_output=True)
        if rc == 0:
            break
        logging.info('Attempt %d failed.', attempt)
        time.sleep(1)

    # unload device
    if args.config.transport != 'UART':
        klog('Unbinding raw driver')
        writeline('/sys/bus/spi/drivers/spidev/unbind', device_id)

    # Go back to normal mode
    gpio('out', args.config.gpios.NRST)
    gpio('0', args.config.gpios.BOOT0, args.config.gpios.NRST)
    gpio('1', args.config.gpios.NRST)

    # Give up GPIO control, unless we need to keep these driving as
    # outputs because they're not open-drain signals.
    # TODO(b/179839337): Make this the default and properly support
    # open-drain outputs on other platforms.
    if args.platform_name != 'strongbad' and args.platform_name != 'herobrine':
        gpio('in', args.config.gpios.BOOT0, args.config.gpios.NRST)

    gpio('unexport', args.config.gpios.BOOT0, args.config.gpios.NRST)

    # Dartmonkey's RO has a flashprotect logic issue that forces reboot loops
    # when SW-WP is enabled and HW-WP is disabled. It is avoided if a POR is
    # detected on boot. We force a POR here to ensure we avoid this reboot loop.
    # See to b/146428434.
    if args.config.gpios.PWREN > 0:
        logging.info('Power cycling the FPMCU.')
        gpio('export', args.config.gpios.PWREN)
        gpio('out', args.config.gpios.PWREN)
        gpio('0', args.config.gpios.PWREN)
        # Must outlast hardware soft start, which is typically ~3ms.
        time.sleep(0.5)
        gpio('1', args.config.gpios.PWREN)
        # Power enable line is externally pulled down, so leave as output-high.
        gpio('unexport', args.config.gpios.PWREN)

    # Put back cros_fp driver if transport is SPI
    if args.config.transport != 'UART':
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
    if args.config.transport == 'UART':
        logging.warning('Please reboot this device.')
    else:
        # Test it
        klog('Query version and reset flags')
        run_system_cmd('ectool --name=cros_fp version', show_output=True)
        run_system_cmd('ectool --name=cros_fp uptimeinfo', show_output=True)


def cmd_flash(args: argparse.Namespace):
    """Flash the entire firmware FPMCU using the built-in bootloader.

    This requires the Chromebook to be in dev mode with hardware write protect
    disabled.
    """

    class config_platform:
        """Board specific configuration functions

        Holds the following:
            Transport, Transport Device, FPGpios
        """
        # =====================================================================
        # Board specific configuration functions
        # Returns the following:
        # Transport, Transport Device, FPGpios(NRST, BOOT0, PWREN)

        def config_hatch(self) -> list:
            # See
            # third_party/coreboot/src/soc/intel/cannonlake/include/soc/
            #   gpio_soc_defs.h
            # for pin name to number mapping.
            return ['SPI', '/dev/spidev1.1',
                    get_gpios(FPGpios(NRST=12, BOOT0=22, PWREN=192))]

        def config_herobrine(self) -> list:
            return ['SPI', '/dev/spidev11.0',
                    get_gpios(FPGpios(NRST='FP_RST_L', BOOT0='FPMCU_BOOT0',
                                      PWREN='EN_FP_RAILS'))]

        def config_nami(self) -> list:
            return ['SPI', '/dev/spidev32765.0',
                    get_gpios(FPGpios(NRST=57, BOOT0=77, PWREN=35))]

        def config_nami_kernelnext(self) -> list:
            return ['SPI', '/dev/spidev1.0',
                    get_gpios(FPGpios(NRST=57, BOOT0=77, PWREN=35))]

        def config_nocturne(self) -> list:
            return ['SPI', '/dev/spidev32765.0',
                    get_gpios(FPGpios(NRST=58, BOOT0=56, PWREN=11))]

        def config_nocturne_kernelnext(self) -> list:
            return ['SPI', '/dev/spidev1.0',
                    get_gpios(FPGpios(NRST=58, BOOT0=56, PWREN=11))]

        def config_strongbad(self) -> list:
            return ['SPI', '/dev/spidev10.0',
                    get_gpios(FPGpios(NRST='FP_RST_L', BOOT0='FPMCU_BOOT0',
                                      PWREN=-1))]

        def config_volteer(self) -> list:
            # See kernel/v5.4/drivers/pinctrl/intel/pinctrl-tigerlake.c
            # for pin name and pin number.
            # Examine `cat /sys/kernel/debug/pinctrl/INT34C5:00/gpio-ranges`
            # on a volteer device to determine gpio number from pin number.
            # For example: GPP_C23 is UART2_CTS which can be queried from EDS
            # the pin number is 194. From the gpio-ranges, the gpio value is
            # 408 + (194-171) = 431
            return ['SPI', '/dev/spidev1.0',
                    get_gpios(FPGpios(NRST=194, BOOT0=193, PWREN=63))]

        def config_brya(self) -> list:
            # See kernel/v5.10/drivers/pinctrl/intel/pinctrl-tigerlake.c
            # for pin name and pin number.
            # Examine `cat /sys/kernel/debug/pinctrl/INTC1055:00/gpio-ranges`
            # on a brya device to determine gpio number from pin number.
            # For example: GPP_D1 is ISH_GP_1 which can be queried from EDS
            # the pin number is 100 from the pinctrl-tigerlake.c.
            # From the gpio-ranges, the gpio value is 312 + (100-99) = 313
            return ['SPI', '/dev/spidev0.0',
                    get_gpios(FPGpios(NRST=100, BOOT0=99, PWREN=101))]

        def config_brask(self) -> list:
            # Let's call the config_brya since brask follows the brya HW design
            return self.config_brya()

        def config_zork(self) -> list:
            return ['UART', '/dev/ttyS1',
                    get_gpios(FPGpios(NRST=11, BOOT0=69, PWREN=-1))]

        def config_guybrush(self) -> list:
            return ['UART', '/dev/ttyS1',
                    get_gpios(FPGpios(NRST=11, BOOT0=144, PWREN=-1))]

        # =====================================================================

        config_func = {
            'hatch' :               config_hatch,
            'herobrine' :           config_herobrine,
            'nami' :                config_nami,
            'nami-kernelnext' :     config_nami_kernelnext,
            'nocturne' :            config_nocturne,
            'nocturne-kernelnext' : config_nocturne_kernelnext,
            'strongbad' :           config_strongbad,
            'volteer' :             config_volteer,
            'brya' :                config_brya,
            'brask' :               config_brask,
            'zork' :                config_zork,
            'guybrush' :            config_guybrush,
        }

        def __new__(cls, platform: str):
            if not platform in config_platform.config_func:
                return None
            return super().__new__(cls)

        def __init__(self, platform: str):
            self.transport, self.device, self.gpios = (
                self.config_func[platform](self)
            )


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
    args.config = config_platform(args.platform_name)
    if not args.config:
        args.config = config_platform(args.platform_base_name)
        if not args.config:
            logging.error('No config for platform %s', args.platform_name)
            sys.exit(ExitCode.EXIT_CONFIG)

    # Help the user out with defaults, if no *file* was given.
    if not args.binary:
        if args.read:
            now = datetime.datetime.now(datetime.timezone.utc)
            tz = now.astimezone().tzinfo
            now = datetime.datetime.now(tz=tz)
            date = str(now.strftime('%Y-%m-%dT%H:%M:%S%z'))
            args.binary = f'/tmp/fpmcu-fw-{date}.bin'
        else:
            args.binary = get_default_fw()

    if not args.noservices:
        logging.info('# Stopping biod and timberslide')
        run_system_cmd('stop biod', show_output=True, check=False)
        run_system_cmd('stop timberslide '
                       + 'LOG_PATH=/sys/kernel/debug/cros_fp/console_log',
                       show_output=True, check=False)

    # If cros-ec driver isn't bound on startup, this means the final rebinding
    # may fail.
    if (not os.path.exists('/dev/cros_fp') or not
            stat.S_ISCHR(os.stat('/dev/cros_fp').st_mode)):
        logging.warning('The cros-ec driver was not bound on startup.')

    if (os.path.exists(args.config.device) and
            stat.S_ISCHR(os.stat(args.config.device).st_mode)):
        logging.warning('The raw driver %s was bound on startup.',
                        args.config.device)

    # Ensure no processes have cros_fp device or debug device open.
    # This might be biod and/or timberslide.
    files_open = proc_open_files('/dev/cros_fp\\|/sys/kernel/debug/cros_fp/*')
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
    files_open = proc_open_files(args.config.device)
    if files_open:
        logging.warning('Another process has %s open.', args.config.device)
        logging.warning('%s', os.linesep.join(files_open))
        logging.warning('Try "fuser -k %s" before running this script.',
                        args.config.device)
        logging.warning('See b/188985272.')

    rc = flash_fp_mcu_stm32(args)

    if not args.noservices:
        logging.info('# Restarting biod and timberslide')
        run_system_cmd('start timberslide '
                       + 'LOG_PATH=/sys/kernel/debug/cros_fp/console_log',
                       show_output=True)
        run_system_cmd('start biod', show_output=True)

    return rc


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
