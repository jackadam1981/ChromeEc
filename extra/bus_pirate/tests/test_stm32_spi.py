#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import unittest
from unittest.mock import MagicMock

from buspirate import BusPirate
from stm32_spi import Stm32BootloaderSpi
from tests.buspirate_test import TestCase


class TestStm32BootloaderSpi(TestCase):
    def setUp(self):
        self.mock_serial = self.patch_serial()
        self.mock_reset = self.patch_spi_toggle_reset()
        self.mock_configure = self.patch_spi_configure()

    def create_bootloader_spi(self):
        bootloader_spi = Stm32BootloaderSpi(BusPirate(1, 1))
        self.mock_configure.assert_called_once()
        self.mock_reset.assert_called_once()
        return bootloader_spi

    def test_send_start(self):
        bootloader_spi = self.create_bootloader_spi()
        self.mock_serial.read = MagicMock(side_effect=[b'\x01', b'\xA5'])
        ret = bootloader_spi.send_start()
        self.assertEqual(b'\xA5', ret)
        self.mock_serial.read.assert_called()
        self.mock_serial.write.assert_called_with(b'\x5A')

    def test_wait_for_ack(self):
        bootloader_spi = self.create_bootloader_spi()
        # A real device doesn't return 0x79 immediately, so we're testing
        # that the implementation retries until it gets the ACK.
        self.mock_serial.read = MagicMock(side_effect=[b'\x01', b'\xA5',
                                                       b'\x01', b'\xA5',
                                                       b'\x01', b'\x79',
                                                       b'\x01', b'\x00'])
        bootloader_spi.wait_for_ack()


if __name__ == '__main__':
    unittest.main()
