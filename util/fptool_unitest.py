#!/usr/bin/env python3
#
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittest for fptool."""

import unittest
from unittest import mock

import fptool

fptool.logging = mock.Mock()

_SPI_MODALIAS_GLOB = ['/sys/bus/spi/devices/spi-PRP0001:01/modalias',
                      '/sys/bus/spi/devices/spi-PRP0001:02/modalias'
                      ]

_SPI_MODALIAS = ['of:Ns001TCgoogle,cr50',
                 'of:NcrfpTCgoogle,cros-ec-spi'
                 ]

_UART_MODALIAS_GLOB = ['/sys/bus/serial/devices/serial0-0/modalias']

_UART_MODALIAS = ['of:NcrfpTCgoogle,cros-ec-uart']

_STRONGBAD_MODALIAS_GLOB = ['/sys/bus/spi/devices/spi0.0/modalias',
                            '/sys/bus/spi/devices/spi10.0/modalias',
                            '/sys/bus/spi/devices/spi12.0/modalias',
                            '/sys/bus/spi/devices/spi6.0/modalias'
                            ]

_STRONGBAD_MODALIAS = ['spi:cr50',
                       'spi:cros-ec-spi',
                       'spi:spi-nor',
                       'spi:cros-ec-spi'
                       ]

_DEV_NAME_GLOB_INPUT = '/sys/bus/platform/drivers/dw-apb-uart/*/*/serial0-0/'

_DEV_NAME_GLOB_SINGLE = ['/sys/bus/platform/drivers/dw-apb-uart/AMD0020:01/'
                         'serial0/serial0-0/'
                         ]

_DEV_NAME_GLOB_DOUBLE = ['/sys/bus/platform/drivers/dw-apb-uart/AMD0020:01/'
                         'serial0/serial0-0/',
                         '/sys/bus/platform/drivers/dw-apb-uart/AMD0020:02/'
                         'serial0/serial0-0/'
                         ]

_LSBVAL = ['CHROMEOS_RELEASE_APPID={0BE68F68-A2F2-46B7-A7B4-B51B63F64FBA}',
           '# Normal line.',
           'SOME_KEY=value',
           '# Key and value with leading/trailing whitespace.',
           '        WS_KEY  =    value       ',
           '# Value with whitespace in the middle.',
           'WS_VALUE = v a l u e ',
           '# Value with quotes don\'t get removed.',
           'DOUBLE_QUOTES = \"double\"',
           'SINGLE_QUOTES = \'sin gle\'',
           'RANDOM_QUOTES = \'\"',
           'CHROMEOS_BOARD_APPID={0BE68F68-A2F2-46B7-A7B4-B51B63F64FBA}',
           'CHROMEOS_CANARY_APPID={90F229CE-83E2-4FAF-8479-E368A34938B1}',
           'DEVICETYPE=CHROMEBOOK', 'CHROMEOS_RELEASE_NAME=Chrome OS',
           'CHROMEOS_AUSERVER=https://tools.google.com/service/update2',
           'CHROMEOS_DEVSERVER=',
           'CHROMEOS_ARC_VERSION=7906507',
           'CHROMEOS_ARC_ANDROID_SDK_VERSION=28',
           'CHROMEOS_RELEASE_BUILDER_PATH=zork-release/R98-14346.0.0',
           'CHROMEOS_RELEASE_KEYSET=devkeys',
           'CHROMEOS_RELEASE_TRACK=testimage-channel',
           'CHROMEOS_RELEASE_BUILD_TYPE=Official Build',
           'CHROMEOS_RELEASE_DESCRIPTION=14346.0.0 (Official Build) '
           'dev-channel zork test',
           'CHROMEOS_RELEASE_BOARD=zork',
           'CHROMEOS_RELEASE_BRANCH_NUMBER=0',
           'CHROMEOS_RELEASE_BUILD_NUMBER=14346',
           'CHROMEOS_RELEASE_CHROME_MILESTONE=98',
           'CHROMEOS_RELEASE_PATCH_NUMBER=0',
           'CHROMEOS_RELEASE_VERSION=14346.0.0',
           'GOOGLE_RELEASE=14346.0.0',
           'CHROMEOS_RELEASE_UNIBUILD=1'
           ]

_DEFAULT_FIRMWARE = ['/opt/google/biod/fw/bloonchipper_v2.0.5938-197506c1-RO_v2'
                     '.0.10551-87ebfe5-RW.bin'
                     ]

_TWO_DEFAULT_FIRMWARES = ['/opt/google/biod/fw/bloonchipper_v1.0.5938-197506c1-'
                          'RO_v1.0.10551-87ebfe5-RW.bin',
                          '/opt/google/biod/fw/bloonchipper_v2.0.5938-197506c1-'
                          'RO_v2.0.10551-87ebfe5-RW.bin'
                          ]

_FIRMWARE_GLOB_INPUT_NO_BOARD = '/opt/google/biod/fw/*.bin'

_FIRMWARE_GLOB_INPUT_BLOONCHIPPER = '/opt/google/biod/fw/bloonchipper*.bin'

_GPIO_RANGES = ['GPIO ranges handled:\n',
                '0: INT1055:00 GPIOS [344 - 355] PINS [0 - 11]\n',
                '15: INT1055:00 GPIOS [359 - 370] PINS [15 - 26]\n',
                'GPIO ranges handled:\n',
                '0: INT1055:01 GPIOS [288 - 295] PINS [0 - 7]\n',
                '15: INT1055:01 GPIOS [303 - 314] PINS [15 - 26]\n',
                '30: INT1055:01 GPIOS [318 - 323] PINS [30 - 35]\n',
                '45: INT1055:01 GPIOS [333 - 340] PINS [45 - 52]\n',
                'GPIO ranges handled:\n',
                '0: INTC1055:02 GPIOS [152 - 177] PINS [0 - 25]\n',
                '32: INTC1055:02 GPIOS [178 - 193] PINS [26 - 41]\n',
                '64: INTC1055:02 GPIOS [194 - 218] PINS [42 - 66]\n',
                '96: INTC1055:02 GPIOS [219 - 226] PINS [67 - 74]\n',
                '128: INTC1055:02 GPIOS [227 - 250] PINS [75 - 98]\n',
                ]

_GPIO_DETECT = 'gpiochip0 [INTC1055:00] (27 lines)\n' \
               'gpiochip1 [INTC1055:01] (53 lines)\n' \
               'gpiochip2 [INTC1055:02] (99 lines)\n'

_GPIOCHIP_GLOB_INPUT = '/sys/class/gpio/gpiochip*/'

_GPIOCHIP_GLOB = ['/sys/class/gpio/gpiochip344/',
                  '/sys/class/gpio/gpiochip288/',
                  '/sys/class/gpio/gpiochip152/'
                  ]

_GPIOCHIP_LABLES_BASES = ['INTC1055:00', '344', 'INTC1055:01', '288',
                          'INTC1055:02', '152'
                          ]

_GPIOFIND_FP_RST_L = 'gpiochip0 22'

_GPIOFIND_FPMCU_BOOT0 = 'gpiochip2 98'

_GPIOFIND_FPMCU_SEL = 'gpiochip0 74'

_GPIOFIND_OUT_OF_SCOPE = 'gpiochip3 10'

class AssertWpIsDisabledTest(unittest.TestCase):
    """Test the assert_wp_is_disabled functionality"""

    @mock.patch('fptool.run_system_cmd')
    def test_assert_wp_is_disabled(self, mock_sys_cmd):
        mock_sys_cmd.return_value = [1, None, None]
        with self.assertRaises(SystemExit) as exit_trap:
            fptool.assert_wp_is_disabled()
        assert isinstance(exit_trap.exception, SystemExit)
        assert exit_trap.exception.code == fptool.ExitCode.EXIT_PRECONDITION

        mock_sys_cmd.return_value = [0, '1', None]
        with self.assertRaises(SystemExit) as exit_trap:
            fptool.assert_wp_is_disabled()
        assert isinstance(exit_trap.exception, SystemExit)
        assert exit_trap.exception.code == fptool.ExitCode.EXIT_PRECONDITION

        mock_sys_cmd.return_value = [0, '0', None]
        try:
            with self.assertRaises(SystemExit) as exit_trap:
                fptool.assert_wp_is_disabled()
        except AssertionError:
            pass


class ReadModaliasTest(unittest.TestCase):
    """Test the correctness of reading the transport device ID"""

    def test_get_devid_no_modalias(self):
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = []
            ret = fptool.get_spiid()
            mock_glob.assert_called_once_with('/sys/bus/spi/devices/*/modalias')
            assert ret == ''

        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = []
            ret = fptool.get_uartid()
            mock_glob.assert_called_once_with('/sys/bus/serial/devices/*/'
                                              'modalias')
            assert ret == ''

    @mock.patch('glob.glob')
    @mock.patch('fptool.readline')
    def test_get_devid(self, mock_readline, mock_glob):
        mock_glob.return_value = _SPI_MODALIAS_GLOB
        mock_readline.side_effect = _SPI_MODALIAS
        ret = fptool.get_spiid()
        mock_glob.assert_called_once()
        assert mock_readline.call_count == 2
        assert ret == 'spi-PRP0001:02'

        mock_glob.reset_mock()
        mock_readline.reset_mock()
        mock_glob.return_value = _UART_MODALIAS_GLOB
        mock_readline.side_effect = _UART_MODALIAS
        ret = fptool.get_uartid()
        mock_glob.assert_called_once()
        mock_readline.assert_called_once()
        assert ret == 'serial0-0'

        mock_glob.reset_mock()
        mock_readline.reset_mock()
        mock_glob.return_value = _STRONGBAD_MODALIAS_GLOB
        mock_readline.side_effect = _STRONGBAD_MODALIAS
        ret = fptool.get_spiid()
        mock_glob.assert_called_once()
        assert mock_readline.call_count == 4
        assert ret == 'spi10.0'

class GetUartDevNameTest(unittest.TestCase):
    """Test the correctness of reading the UART device name"""

    def test_get_uart_dev_name_no_device(self):
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = []
            dev_name = fptool.get_uart_dev_name('serial0-0')
            mock_glob.assert_called_once_with(_DEV_NAME_GLOB_INPUT)
            assert dev_name == ''

    def test_get_uart_dev_name_two_devices(self):
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = _DEV_NAME_GLOB_DOUBLE
            dev_name = fptool.get_uart_dev_name('serial0-0')
            mock_glob.assert_called_once_with(_DEV_NAME_GLOB_INPUT)
            assert dev_name == ''

    def test_get_uart_dev_name(self):
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = _DEV_NAME_GLOB_SINGLE
            dev_name = fptool.get_uart_dev_name('serial0-0')
            mock_glob.assert_called_once_with(_DEV_NAME_GLOB_INPUT)
            assert dev_name == 'AMD0020:01'


class lsbvalTest(unittest.TestCase):
    """Test the correctness of the lsbval parser"""

    @mock.patch('fptool.read_lsbval')
    def test_get_platform_name(self, mock_lsbval):
        mock_lsbval.return_value = _LSBVAL
        platform_name = fptool.get_platform_name()
        assert platform_name == 'zork'


class PlatformBaseNameTest(unittest.TestCase):
    """Test the correctness of the platform base name parser"""

    def test_get_platform_base_name(self):
        base_name = fptool.get_platform_base_name('hatch')
        assert base_name == 'hatch'
        base_name = fptool.get_platform_base_name('hatch-kernel')
        assert base_name == 'hatch'
        base_name = fptool.get_platform_base_name('hatch-arc-c')
        assert base_name == 'hatch'


class GetDefaultFirmwareTest(unittest.TestCase):
    """Test the get_default_firmware function"""

    @mock.patch('fptool.run_system_cmd')
    def test_get_default_firmware(self, mock_sys_cmd):
        mock_sys_cmd.return_value = [1, None, None]
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = []
            with self.assertRaises(SystemExit) as exit_trap:
                fptool.get_default_firmware()
            assert isinstance(exit_trap.exception, SystemExit)
            assert exit_trap.exception.code == fptool.ExitCode.EXIT_CONFIG
            mock_glob.assert_called_once_with(_FIRMWARE_GLOB_INPUT_NO_BOARD)

        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = _TWO_DEFAULT_FIRMWARES
            with self.assertRaises(SystemExit) as exit_trap:
                fptool.get_default_firmware()
            assert isinstance(exit_trap.exception, SystemExit)
            assert exit_trap.exception.code == fptool.ExitCode.EXIT_CONFIG
            mock_glob.assert_called_once_with(_FIRMWARE_GLOB_INPUT_NO_BOARD)

        mock_sys_cmd.return_value = [0, None, None]
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = []
            with self.assertRaises(SystemExit) as exit_trap:
                fptool.get_default_firmware()
            assert isinstance(exit_trap.exception, SystemExit)
            assert exit_trap.exception.code == fptool.ExitCode.EXIT_CONFIG
            mock_glob.assert_called_once_with(_FIRMWARE_GLOB_INPUT_NO_BOARD)

        mock_sys_cmd.return_value = [0, 'bloonchipper', None]
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = []
            try:
                with self.assertRaises(SystemExit) as exit_trap:
                    default_firmware = fptool.get_default_firmware()
            except AssertionError:
                mock_glob.assert_called_once_with(
                    _FIRMWARE_GLOB_INPUT_BLOONCHIPPER)
                assert default_firmware == _DEFAULT_FIRMWARE


class FPGpiosGetGpioByIndexTest(unittest.TestCase):
    """Test access to GPIO link by index"""

    @mock.patch('fptool.run_system_cmd')
    def test_get_gpio_by_index(self, mock_sys_cmd):
        mock_sys_cmd.return_value = [1, None, None]
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = []
            with self.assertRaises(SystemExit) as exit_trap:
                fpgpios = fptool.FPGpios(n_reset=57, boot_0=77, power_enable=35)
            assert isinstance(exit_trap.exception, SystemExit)
            assert exit_trap.exception.code == fptool.ExitCode.EXIT_RUNTIME

        with mock.patch('fptool.FPGpios._read_gpio_ranges') as mock_ranges:
            mock_ranges.return_value = _GPIO_RANGES
            with self.assertRaises(SystemExit) as exit_trap:
                fpgpios = fptool.FPGpios(n_reset=126, boot_0=0, power_enable=0)
            assert isinstance(exit_trap.exception, SystemExit)
            assert exit_trap.exception.code == fptool.ExitCode.EXIT_RUNTIME

            with self.assertRaises(SystemExit) as exit_trap:
                fpgpios = fptool.FPGpios(n_reset=0, boot_0=179, power_enable=0)
            assert isinstance(exit_trap.exception, SystemExit)
            assert exit_trap.exception.code == fptool.ExitCode.EXIT_RUNTIME

            try:
                with self.assertRaises(SystemExit) as exit_trap:
                    fpgpios = fptool.FPGpios(n_reset=99, boot_0=129,
                                             power_enable=-1)
            except AssertionError:
                assert fpgpios.n_reset._gpio == '288'
                assert fpgpios.boot_0._gpio == '318'
                assert not fpgpios.power_enable._gpio


class FPGpiosGetGpioByNameTest(unittest.TestCase):
    """Test access to GPIO link by name"""

    @mock.patch('fptool.FPGpios._parse_gpio_ranges')
    def test_get_gpio_by_name(self, mock_ranges):
        mock_ranges.return_value = []
        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.return_value = [1, None, None]
            with self.assertRaises(SystemExit) as exit_trap:
                fpgpios = fptool.FPGpios(n_reset='FP_RST_L',
                                         boot_0='FPMCU_BOOT0',
                                         power_enable=-1)
            assert isinstance(exit_trap.exception, SystemExit)
            assert exit_trap.exception.code == fptool.ExitCode.EXIT_RUNTIME

        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.return_value = [0, None, None]
            with self.assertRaises(SystemExit) as exit_trap:
                fpgpios = fptool.FPGpios(n_reset='FP_RST_L',
                                         boot_0='FPMCU_BOOT0',
                                         power_enable=-1)
            assert isinstance(exit_trap.exception, SystemExit)
            assert exit_trap.exception.code == fptool.ExitCode.EXIT_RUNTIME

        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.side_effect = [[0, _GPIO_DETECT, None],
                                        [1, None, None]]
            with mock.patch('glob.glob') as mock_glob:
                mock_glob.return_value = _GPIOCHIP_GLOB
                with mock.patch('fptool.readline') as mock_readline:
                    mock_readline.side_effect = _GPIOCHIP_LABLES_BASES
                    with self.assertRaises(SystemExit) as exit_trap:
                        fpgpios = fptool.FPGpios(n_reset='UNKNOWN_GPIO',
                                                 boot_0='FPMCU_BOOT0',
                                                 power_enable=-1)
                    mock_sys_cmd.assert_called_with('gpiofind UNKNOWN_GPIO')
                    assert isinstance(exit_trap.exception, SystemExit)
                    assert (exit_trap.exception.code ==
                            fptool.ExitCode.EXIT_RUNTIME)

        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.side_effect = [[0, _GPIO_DETECT, None],
                                        [0, _GPIOFIND_OUT_OF_SCOPE, None]]
            with mock.patch('glob.glob') as mock_glob:
                mock_glob.return_value = _GPIOCHIP_GLOB
                with mock.patch('fptool.readline') as mock_readline:
                    mock_readline.side_effect = _GPIOCHIP_LABLES_BASES
                    with self.assertRaises(SystemExit) as exit_trap:
                        fpgpios = fptool.FPGpios(n_reset=-1,
                                                 boot_0='OUT_OF_SCOPE',
                                                 power_enable=-1)
                    mock_sys_cmd.assert_called_with('gpiofind OUT_OF_SCOPE')
                    assert isinstance(exit_trap.exception, SystemExit)
                    assert (exit_trap.exception.code
                            == fptool.ExitCode.EXIT_RUNTIME)

        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.side_effect = [[0, _GPIO_DETECT, None],
                                        [0, _GPIOFIND_FP_RST_L, None],
                                        [0, _GPIOFIND_FPMCU_BOOT0, None]]
            with mock.patch('glob.glob') as mock_glob:
                mock_glob.return_value = _GPIOCHIP_GLOB
                with mock.patch('fptool.readline') as mock_readline:
                    mock_readline.side_effect = _GPIOCHIP_LABLES_BASES
                    try:
                        with self.assertRaises(SystemExit) as exit_trap:
                            fpgpios = fptool.FPGpios(n_reset='FP_RST_L',
                                                     boot_0='FPMCU_BOOT0',
                                                     power_enable=-1)
                    except AssertionError:
                        assert (mock_sys_cmd.mock_calls[1][1][0]
                                == 'gpiofind FP_RST_L')
                        assert (mock_sys_cmd.mock_calls[2][1][0]
                                == 'gpiofind FPMCU_BOOT0')
                        assert fpgpios.n_reset._gpio == '366'
                        assert fpgpios.boot_0._gpio == '250'
                        assert not fpgpios.power_enable._gpio


class ConfigPlatformTest(unittest.TestCase):
    """Test the platform specific GPIO and transport configuration"""

    def test_unknown_platform(self):
        config = fptool.ConfigPlatform(None)
        assert not config
        config = fptool.ConfigPlatform('')
        assert not config
        config = fptool.ConfigPlatform('unknown')
        assert not config

    @mock.patch('fptool.run_system_cmd')
    @mock.patch('fptool.FPGpios._read_gpio_ranges')
    @mock.patch('fptool.release')
    def test_index_base_config(self, mock_release, mock_ranges, mock_sys_cmd):
        mock_sys_cmd.return_value = [1, None, None]
        mock_ranges.return_value = _GPIO_RANGES
        mock_release.return_value = '4.5.0-25-generic'
        try:
            with self.assertRaises(SystemExit):
                config = fptool.ConfigPlatform('nami')
        except AssertionError:
            assert config._config['TRANSPORT'] == 'SPI'
            assert config._config['DEVICE'] == '/dev/spidev1.0'
            assert config._config['GPIOS'].n_reset._gpio == '209'
            assert config._config['GPIOS'].boot_0._gpio == '229'
            assert config._config['GPIOS'].power_enable._gpio == '187'

    @mock.patch('fptool.run_system_cmd')
    @mock.patch('glob.glob')
    @mock.patch('fptool.readline')
    @mock.patch('fptool.FPGpios._read_gpio_ranges')
    @mock.patch('fptool.release')
    def test_name_base_config(self, mock_release, mock_ranges, mock_readline,
                              mock_glob, mock_sys_cmd):
        mock_sys_cmd.side_effect = [[0, _GPIO_DETECT, None],
                                    [0, _GPIOFIND_FP_RST_L, None],
                                    [0, _GPIOFIND_FPMCU_BOOT0, None],
                                    [0, _GPIOFIND_FPMCU_SEL, None]]
        mock_glob.return_value = _GPIOCHIP_GLOB
        mock_readline.side_effect = _GPIOCHIP_LABLES_BASES
        mock_ranges.return_value = _GPIO_RANGES
        mock_release.return_value = '4.5.0-25-generic'
        try:
            with self.assertRaises(SystemExit):
                config = fptool.ConfigPlatform('strongbad')
        except AssertionError:
            assert config._config['TRANSPORT'] == 'SPI'
            assert config._config['DEVICE'] == '/dev/spidev10.0'
            assert config._config['GPIOS'].n_reset._gpio == '366'
            assert config._config['GPIOS'].boot_0._gpio == '250'
            assert config._config['GPIOS'].power_enable._gpio == '418'


if __name__ == '__main__':
    unittest.main()
