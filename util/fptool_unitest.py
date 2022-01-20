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

_LSBVAL_NO_PLATFORM_NAME = ['CHROMEOS_ARC_VERSION=7906507',
                            'CHROMEOS_ARC_ANDROID_SDK_VERSION=28',
                            'CHROMEOS_RELEASE_KEYSET=devkeys',
                            'CHROMEOS_RELEASE_TRACK=testimage-channel',
                            ]

_LSBVAL_NORMAL = ['# Normal line.',
                  'SOME_KEY=value'
                  ]

_LSBVAL_LEADING_SPACES = ['# Key and value with leading/trailing whitespace.',
                          '        WS_KEY  =    value       ',
                          ]

_LSBVAL_SPACED_VALUE = ['# Value with whitespace in the middle.',
                        'WS_VALUE = v a l u e ',
                        ]

_LSBVAL_QUOTED_VALUES = ['# Value with quotes don\'t get removed.',
                         'DOUBLE_QUOTES = \"double\"',
                         'SINGLE_QUOTES = \'sin gle\'',
                         'RANDOM_QUOTES = \'\"',
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


class AssertWpIsDisabledTest(unittest.TestCase):
    """Test the assert_wp_is_disabled functionality"""

    def test_assert_wp_is_disabled_crossystem_error(self):
        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.return_value = [1, None, None]
            with self.assertRaises(SystemExit) as exit_trap:
                fptool.assert_wp_is_disabled()
            self.assertEqual(exit_trap.exception.code,
                             fptool.ExitCode.EXIT_PRECONDITION)

    def test_assert_wp_is_disabled_wp_enabled(self):
        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.return_value = [0, '1', None]
            with self.assertRaises(SystemExit) as exit_trap:
                fptool.assert_wp_is_disabled()
            self.assertEqual(exit_trap.exception.code,
                             fptool.ExitCode.EXIT_PRECONDITION)

    def test_assert_wp_is_disabled_wp_disabled(self):
        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.return_value = [0, '0', None]
            fptool.assert_wp_is_disabled()


class ReadModaliasTest(unittest.TestCase):
    """Test the correctness of reading the transport device ID"""

    def test_spi_get_devid_no_modalias(self):
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = []
            ret = fptool.get_spiid()
            mock_glob.assert_called_once_with('/sys/bus/spi/devices/*/modalias')
            self.assertEqual(ret, '')

    def test_get_uart_devid_no_modalias(self):
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = []
            ret = fptool.get_uartid()
            mock_glob.assert_called_once_with('/sys/bus/serial/devices/*/'
                                              'modalias')
            self.assertEqual(ret, '')

    def test_get_spi_devid(self):
        with mock.patch('glob.glob') as mock_glob:
            with mock.patch('fptool.readline') as mock_readline:
                mock_glob.return_value = _SPI_MODALIAS_GLOB
                mock_readline.side_effect = _SPI_MODALIAS
                ret = fptool.get_spiid()
                mock_glob.assert_called_once()
                self.assertEqual(mock_readline.call_count, 2)
                self.assertEqual(ret, 'spi-PRP0001:02')

    def test_get_uart_devid(self):
        with mock.patch('glob.glob') as mock_glob:
            with mock.patch('fptool.readline') as mock_readline:
                mock_glob.return_value = _UART_MODALIAS_GLOB
                mock_readline.side_effect = _UART_MODALIAS
                ret = fptool.get_uartid()
                mock_glob.assert_called_once()
                mock_readline.assert_called_once()
                self.assertEqual(ret, 'serial0-0')

    def test_get_strongbad_devid(self):
        with mock.patch('glob.glob') as mock_glob:
            with mock.patch('fptool.readline') as mock_readline:
                mock_glob.return_value = _STRONGBAD_MODALIAS_GLOB
                mock_readline.side_effect = _STRONGBAD_MODALIAS
                ret = fptool.get_spiid()
                mock_glob.assert_called_once()
                self.assertEqual(mock_readline.call_count, 4)
                self.assertEqual(ret, 'spi10.0')


class GetUartDevNameTest(unittest.TestCase):
    """Test the correctness of reading the UART device name"""

    def test_get_uart_dev_name_no_device(self):
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = []
            dev_name = fptool.get_uart_dev_name('serial0-0')
            mock_glob.assert_called_once_with(_DEV_NAME_GLOB_INPUT)
            self.assertEqual(dev_name, '')

    def test_get_uart_dev_name_two_devices(self):
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = _DEV_NAME_GLOB_DOUBLE
            dev_name = fptool.get_uart_dev_name('serial0-0')
            mock_glob.assert_called_once_with(_DEV_NAME_GLOB_INPUT)
            self.assertEqual(dev_name, '')

    def test_get_uart_dev_name(self):
        with mock.patch('glob.glob') as mock_glob:
            mock_glob.return_value = _DEV_NAME_GLOB_SINGLE
            dev_name = fptool.get_uart_dev_name('serial0-0')
            mock_glob.assert_called_once_with(_DEV_NAME_GLOB_INPUT)
            self.assertEqual(dev_name, 'AMD0020:01')


class LsbvalTest(unittest.TestCase):
    """Test the correctness of the lsbval parser"""

    def test_leading_spaces(self):
        with mock.patch('fptool.read_lsbval') as mock_lsbval:
            mock_lsbval.return_value = _LSBVAL_LEADING_SPACES
            value = fptool.lsbval('WS_KEY')
            self.assertEqual(value, 'value')

    def test_spaced_value(self):
        with mock.patch('fptool.read_lsbval') as mock_lsbval:
            mock_lsbval.return_value = _LSBVAL_SPACED_VALUE
            value = fptool.lsbval('WS_VALUE')
            self.assertEqual(value, 'v a l u e')

    def test_double_quoted_value(self):
        with mock.patch('fptool.read_lsbval') as mock_lsbval:
            mock_lsbval.return_value = _LSBVAL_QUOTED_VALUES
            value = fptool.lsbval('DOUBLE_QUOTES')
            self.assertEqual(value, '\"double\"')

    def test_single_quoted_value(self):
        with mock.patch('fptool.read_lsbval') as mock_lsbval:
            mock_lsbval.return_value = _LSBVAL_QUOTED_VALUES
            value = fptool.lsbval('SINGLE_QUOTES')
            self.assertEqual(value, '\'sin gle\'')

    def test_random_quotes(self):
        with mock.patch('fptool.read_lsbval') as mock_lsbval:
            mock_lsbval.return_value = _LSBVAL_QUOTED_VALUES
            value = fptool.lsbval('RANDOM_QUOTES')
            self.assertEqual(value, '\'\"')

    def test_regular_lsbval(self):
        with mock.patch('fptool.read_lsbval') as mock_lsbval:
            mock_lsbval.return_value = _LSBVAL
            value = fptool.lsbval('CHROMEOS_RELEASE_BUILD_NUMBER')
            self.assertEqual(value, '14346')


class PlatformNameTest(unittest.TestCase):
    """Test the lsbval parser for fetching the platform's name"""

    def test_no_platform_name(self):
        with mock.patch('fptool.read_lsbval') as mock_lsbval:
            mock_lsbval.return_value = _LSBVAL_NO_PLATFORM_NAME
            platform_name = fptool.get_platform_name()
            self.assertEqual(platform_name, '')

    def test_get_platform_name(self):
        with mock.patch('fptool.read_lsbval') as mock_lsbval:
            mock_lsbval.return_value = _LSBVAL
            platform_name = fptool.get_platform_name()
            self.assertEqual(platform_name, 'zork')


class PlatformBaseNameTest(unittest.TestCase):
    """Test the correctness of the platform base name parser"""

    def test_get_platform_base_name(self):
        base_name = fptool.get_platform_base_name('hatch')
        self.assertEqual(base_name, 'hatch')
        base_name = fptool.get_platform_base_name('hatch-kernel')
        self.assertEqual(base_name, 'hatch')
        base_name = fptool.get_platform_base_name('hatch-arc-c')
        self.assertEqual(base_name, 'hatch')


class GetDefaultFirmwareTest(unittest.TestCase):
    """Test the get_default_firmware function"""

    def test_cros_config_error(self):
        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.return_value = [1, None, None]
            with mock.patch('glob.glob') as mock_glob:
                mock_glob.return_value = []
                with self.assertRaises(SystemExit) as exit_trap:
                    fptool.get_default_firmware()
                self.assertEqual(exit_trap.exception.code,
                                 fptool.ExitCode.EXIT_CONFIG)
                mock_glob.assert_called_once_with(_FIRMWARE_GLOB_INPUT_NO_BOARD)

    def test_multiple_fingerprint_firmwares(self):
        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.return_value = [1, None, None]
            with mock.patch('glob.glob') as mock_glob:
                mock_glob.return_value = _TWO_DEFAULT_FIRMWARES
                with self.assertRaises(SystemExit) as exit_trap:
                    fptool.get_default_firmware()
                self.assertEqual(exit_trap.exception.code,
                                 fptool.ExitCode.EXIT_CONFIG)
                mock_glob.assert_called_once_with(_FIRMWARE_GLOB_INPUT_NO_BOARD)

    def test_no_fingerprint_firmwares(self):
        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.return_value = [0, None, None]
            with mock.patch('glob.glob') as mock_glob:
                mock_glob.return_value = []
                with self.assertRaises(SystemExit) as exit_trap:
                    fptool.get_default_firmware()
                self.assertEqual(exit_trap.exception.code,
                                 fptool.ExitCode.EXIT_CONFIG)
                mock_glob.assert_called_once_with(_FIRMWARE_GLOB_INPUT_NO_BOARD)

    def test_get_default_firmware_no_fingerprint_board_name(self):
        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.return_value = [0, 'bloonchipper', None]
            with mock.patch('glob.glob') as mock_glob:
                mock_glob.return_value = _DEFAULT_FIRMWARE
                default_firmware = fptool.get_default_firmware()
                mock_glob.assert_called_once_with(
                    _FIRMWARE_GLOB_INPUT_BLOONCHIPPER)
                self.assertEqual(default_firmware, _DEFAULT_FIRMWARE[0])

    def test_get_default_firmware_with_fingerprint_board_name(self):
        with mock.patch('fptool.run_system_cmd') as mock_sys_cmd:
            mock_sys_cmd.return_value = [0, '', None]
            with mock.patch('glob.glob') as mock_glob:
                mock_glob.return_value = _DEFAULT_FIRMWARE
                default_firmware = fptool.get_default_firmware()
                mock_glob.assert_called_once_with(
                    _FIRMWARE_GLOB_INPUT_NO_BOARD)
                self.assertEqual(default_firmware, _DEFAULT_FIRMWARE[0])


if __name__ == '__main__':
    unittest.main()
