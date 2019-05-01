#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import unittest
from unittest.mock import MagicMock

from buspirate_bitbang import Bitbang
from buspirate_bitbang import BitbangEnterSpiCmd
from buspirate_bitbang import BitbangEnterUartCmd
from buspirate_bitbang import BitbangResetCmd
from tests.buspirate_test import TestCase
from tests.buspirate_test import TestCommandHelper
from tests.buspirate_test import TestCommandInterface


class TestBitbangCommand(TestCommandHelper, TestCommandInterface):
    # pylint: disable=abstract-method
    pass


class TestBitbangReset(TestBitbangCommand):
    def test_fields(self):
        bitbang_reset = BitbangResetCmd()
        self.check_command_bytes_equal(bitbang_reset, b'\x00')

    def test_command_size(self):
        self.check_command_byte_size(BitbangResetCmd())


class TestBitbangEnterSpi(TestBitbangCommand):
    def test_fields(self):
        bitbang_spi = BitbangEnterSpiCmd()
        self.check_command_bytes_equal(bitbang_spi, b'\x01')

    def test_command_size(self):
        self.check_command_byte_size(BitbangEnterSpiCmd())


class TestBitbangEnterUart(TestBitbangCommand):
    def test_fields(self):
        bitbang_uart = BitbangEnterUartCmd()
        self.check_command_bytes_equal(bitbang_uart, b'\x03')

    def test_command_size(self):
        self.check_command_byte_size(BitbangEnterSpiCmd())


class TestBitbangBinaryMode(TestCase):
    def setUp(self):
        self.mock_serial = self.patch_serial()

    def test_enable_binary_mode_success(self):
        self.mock_serial.read = \
            MagicMock(return_value=bytes('BBIO1', encoding='ascii'))
        bitbang = Bitbang(self.mock_serial)
        bitbang.enable_binary_mode()
        self.mock_serial.read.assert_called_with(5)
        self.mock_serial.write.assert_called_with(b'\x00')

    def test_enable_binary_mode_failure(self):
        self.mock_serial.read = \
            MagicMock(return_value=bytes('foo', encoding='ascii'))
        bitbang = Bitbang(self.mock_serial)
        with self.assertRaises(RuntimeError):
            bitbang.enable_binary_mode()
        self.mock_serial.read.assert_called()
        self.mock_serial.write.assert_called_with(b'\x00')


# We don't want to test the abstract class
del TestBitbangCommand


if __name__ == '__main__':
    unittest.main()
