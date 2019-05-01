#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import unittest
from unittest.mock import call
from unittest.mock import MagicMock

from buspirate_uart import Uart
from buspirate_uart import UartBaudRateCmd
from buspirate_uart import UartBridgeModeCmd
from buspirate_uart import UartPeripheralCmd
from buspirate_uart import UartRxCmd
from buspirate_uart import UartSettingsCmd
from buspirate_uart import UartWriteCmd
from tests.buspirate_test import TestCase
from tests.buspirate_test import TestCommandHelper
from tests.buspirate_test import TestCommandInterface


class TestUartCommand(TestCommandHelper, TestCommandInterface):
    # pylint: disable=abstract-method
    pass


class TestUartRx(TestUartCommand):
    def test_fields(self):
        uart_rx = UartRxCmd()
        self.check_command_bytes_equal(uart_rx, b'\x02')
        uart_rx.rx = 1
        self.check_command_bytes_equal(uart_rx, b'\x03')

    def test_command_size(self):
        self.check_command_byte_size(UartRxCmd())


class TestUartWrite(TestUartCommand):
    def test_fields(self):
        uart_write = UartWriteCmd(1)
        self.check_command_bytes_equal(uart_write, b'\x10')
        uart_write.num_bytes = 15
        self.check_command_bytes_equal(uart_write, b'\x1F')

    def test_command_size(self):
        for i in range(1, 16):
            with self.subTest(i=i):
                self.check_command_byte_size(UartWriteCmd(i))


class TestUartPeripheral(TestUartCommand):
    def test_fields(self):
        uart_peripheral = UartPeripheralCmd()
        self.check_command_bytes_equal(uart_peripheral, b'\x40')
        uart_peripheral.cs = 1
        self.check_command_bytes_equal(uart_peripheral, b'\x41')
        uart_peripheral.aux = 1
        self.check_command_bytes_equal(uart_peripheral, b'\x43')
        uart_peripheral.pullups = 1
        self.check_command_bytes_equal(uart_peripheral, b'\x47')
        uart_peripheral.power = 1
        self.check_command_bytes_equal(uart_peripheral, b'\x4F')

    def test_command_size(self):
        self.check_command_byte_size(UartPeripheralCmd())


class TestUartBaudRate(TestUartCommand):
    def test_fields(self):
        uart_baud = UartBaudRateCmd(UartBaudRateCmd.BAUD[300])
        self.check_command_bytes_equal(uart_baud, b'\x60')

        uart_baud = UartBaudRateCmd(UartBaudRateCmd.BAUD[1200])
        self.check_command_bytes_equal(uart_baud, b'\x61')

        uart_baud = UartBaudRateCmd(UartBaudRateCmd.BAUD[2400])
        self.check_command_bytes_equal(uart_baud, b'\x62')

        uart_baud = UartBaudRateCmd(UartBaudRateCmd.BAUD[4800])
        self.check_command_bytes_equal(uart_baud, b'\x63')

        uart_baud = UartBaudRateCmd(UartBaudRateCmd.BAUD[9600])
        self.check_command_bytes_equal(uart_baud, b'\x64')

        uart_baud = UartBaudRateCmd(UartBaudRateCmd.BAUD[19200])
        self.check_command_bytes_equal(uart_baud, b'\x65')

        uart_baud = UartBaudRateCmd(UartBaudRateCmd.BAUD[31250])
        self.check_command_bytes_equal(uart_baud, b'\x66')

        uart_baud = UartBaudRateCmd(UartBaudRateCmd.BAUD[38400])
        self.check_command_bytes_equal(uart_baud, b'\x67')

        uart_baud = UartBaudRateCmd(UartBaudRateCmd.BAUD[57600])
        self.check_command_bytes_equal(uart_baud, b'\x68')

        uart_baud = UartBaudRateCmd(UartBaudRateCmd.BAUD[115200])
        self.check_command_bytes_equal(uart_baud, b'\x69')

    def test_command_size(self):
        for key in UartBaudRateCmd.BAUD:
            with self.subTest(i=key):
                self.check_command_byte_size(
                    UartBaudRateCmd(UartBaudRateCmd.BAUD[key]))


class TestUartSettings(TestUartCommand):

    def test_fields(self):
        uart_settings = UartSettingsCmd()
        self.check_command_bytes_equal(uart_settings, b'\x80')
        uart_settings.rx_idle_polarity = 1
        self.check_command_bytes_equal(uart_settings, b'\x81')
        uart_settings.stop_bits = 1
        self.check_command_bytes_equal(uart_settings, b'\x83')
        uart_settings.data_and_parity = 1
        self.check_command_bytes_equal(uart_settings, b'\x87')
        uart_settings.data_and_parity = 2
        self.check_command_bytes_equal(uart_settings, b'\x8B')
        uart_settings.data_and_parity = 3
        self.check_command_bytes_equal(uart_settings, b'\x8F')
        uart_settings.pin_output = 1
        self.check_command_bytes_equal(uart_settings, b'\x9F')

    def test_command_size(self):
        self.check_command_byte_size(UartSettingsCmd())


class TestUartBridgeMode(TestUartCommand):

    def test_fields(self):
        uart_bridge = UartBridgeModeCmd()
        self.check_command_bytes_equal(uart_bridge, b'\x0F')

    def test_command_size(self):
        self.check_command_byte_size(UartBridgeModeCmd())


class TestUart(TestCase):
    def setUp(self):
        self.mock_serial = self.patch_serial()

    def test_send_bytes_single_success(self):
        self.mock_serial.read = MagicMock(return_value=b'\x01')
        uart = Uart(self.mock_serial)
        uart.send_bytes(b'\xAA')
        self.mock_serial.write.assert_has_calls([call(b'\x10'),
                                                 call(b'\xAA')])

    def test_send_bytes_multiple_success(self):
        self.mock_serial.read = MagicMock(side_effect=[b'\x01',
                                                       b'\x01\x01'])
        uart = Uart(self.mock_serial)
        uart.send_bytes(b'\xAA\xBB')
        self.mock_serial.write.assert_has_calls([call(b'\x11'),
                                                 call(b'\xAA\xBB')])

    def test_send_bytes_failure(self):
        self.mock_serial.read = MagicMock(return_value=b'\x00')
        uart = Uart(self.mock_serial)
        with self.assertRaises(RuntimeError):
            uart.send_bytes(b'\xAA')
        self.mock_serial.read.assert_called()
        self.mock_serial.write.assert_called_with(b'\x10')


# We don't want to test the abstract class
del TestUartCommand


if __name__ == '__main__':
    unittest.main()
