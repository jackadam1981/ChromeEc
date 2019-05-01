#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import unittest
from unittest.mock import call
from unittest.mock import MagicMock

from buspirate_spi import Spi
from buspirate_spi import SpiCsCmd
from buspirate_spi import SpiPeripheralCmd
from buspirate_spi import SpiSettingsCmd
from buspirate_spi import SpiWriteCmd
from tests.buspirate_test import TestCase
from tests.buspirate_test import TestCommandHelper
from tests.buspirate_test import TestCommandInterface


class TestSpiCommand(TestCommandHelper, TestCommandInterface):
    # pylint: disable=abstract-method
    pass


class TestSpiCs(TestSpiCommand):
    def test_fields(self):
        spi_cs = SpiCsCmd()
        self.check_command_bytes_equal(spi_cs, b'\x02')
        spi_cs.cs = 1
        self.check_command_bytes_equal(spi_cs, b'\x03')

    def test_command_size(self):
        self.check_command_byte_size(SpiCsCmd())


class TestSpiWrite(TestSpiCommand):
    def test_fields(self):
        spi_write = SpiWriteCmd(1)
        self.check_command_bytes_equal(spi_write, b'\x10')
        spi_write.num_bytes = 15
        self.check_command_bytes_equal(spi_write, b'\x1F')

    def test_command_size(self):
        for i in range(1, 16):
            with self.subTest(i=i):
                self.check_command_byte_size(SpiWriteCmd(i))


class TestSpiPeripheral(TestSpiCommand):
    def test_fields(self):
        spi_peripheral = SpiPeripheralCmd()
        self.check_command_bytes_equal(spi_peripheral, b'\x40')
        spi_peripheral.cs = 1
        self.check_command_bytes_equal(spi_peripheral, b'\x41')
        spi_peripheral.aux = 1
        self.check_command_bytes_equal(spi_peripheral, b'\x43')
        spi_peripheral.pullups = 1
        self.check_command_bytes_equal(spi_peripheral, b'\x47')
        spi_peripheral.power = 1
        self.check_command_bytes_equal(spi_peripheral, b'\x4F')

    def test_command_size(self):
        self.check_command_byte_size(SpiPeripheralCmd())


class TestSpiSettings(TestSpiCommand):

    def test_fields(self):
        spi_settings = SpiSettingsCmd()
        self.check_command_bytes_equal(spi_settings, b'\x80')
        spi_settings.sample_time = 1
        self.check_command_bytes_equal(spi_settings, b'\x81')
        spi_settings.clock_edge = 1
        self.check_command_bytes_equal(spi_settings, b'\x83')
        spi_settings.clock_idle_phase = 1
        self.check_command_bytes_equal(spi_settings, b'\x87')
        spi_settings.pin_output = 1
        self.check_command_bytes_equal(spi_settings, b'\x8F')

    def test_command_size(self):
        self.check_command_byte_size(SpiSettingsCmd())


class TestSpi(TestCase):
    def setUp(self):
        self.mock_serial = self.patch_serial()

    def test_send_bytes_single_success(self):
        self.mock_serial.read = MagicMock(side_effect=[b'\x01', b'\xFF'])
        spi = Spi(self.mock_serial)
        ret = spi.send_bytes(b'\xAA')
        self.mock_serial.write.assert_has_calls([call(b'\x10'),
                                                 call(b'\xAA')])
        self.assertEqual(b'\xFF', ret)

    def test_send_bytes_multiple_success(self):
        expected_read_data = b'\x12\x34'
        self.mock_serial.read = MagicMock(side_effect=[b'\x01',
                                                       expected_read_data])
        spi = Spi(self.mock_serial)
        ret = spi.send_bytes(b'\xAA\xBB')
        self.mock_serial.write.assert_has_calls([call(b'\x11'),
                                                 call(b'\xAA\xBB')])
        self.assertEqual(expected_read_data, ret)

    def test_send_bytes_failure(self):
        self.mock_serial.read = MagicMock(return_value=b'\x00')
        spi = Spi(self.mock_serial)
        with self.assertRaises(RuntimeError):
            spi.send_bytes(b'\xAA')
        self.mock_serial.read.assert_called()
        self.mock_serial.write.assert_called_with(b'\x10')


# We don't want to test the abstract class
del TestSpiCommand


if __name__ == '__main__':
    unittest.main()
