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


if __name__ == '__main__':
    unittest.main()
