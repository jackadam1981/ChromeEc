#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import unittest
from unittest.mock import MagicMock

from buspirate import BusPirate
from stm32_uart import Stm32BootloaderUart
from tests.buspirate_test import TestCase


# We are in bridge mode for this class
class TestStm32BootloaderUart(TestCase):
    def setUp(self):
        self.mock_serial = self.patch_serial()
        self.mock_configure = self.patch_uart_configure()

    def create_bootloader_uart(self):
        bootloader_uart = Stm32BootloaderUart(BusPirate(1, 1))
        self.mock_configure.assert_called_once()
        return bootloader_uart

    def test_send_start(self):
        bootloader_uart = self.create_bootloader_uart()
        self.mock_serial.read = MagicMock(return_value=b'\x01')
        bootloader_uart.send_start()
        self.mock_serial.read.assert_called()
        self.mock_serial.write.assert_called_with(b'\x7F')

    def test_wait_for_ack(self):
        bootloader_uart = self.create_bootloader_uart()
        self.mock_serial.read = MagicMock(return_value=b'\x79')
        bootloader_uart.wait_for_ack()
        self.mock_serial.read.assert_called()
        self.mock_serial.write.assert_not_called()


if __name__ == '__main__':
    unittest.main()
