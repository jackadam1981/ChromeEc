#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from abc import ABC
from abc import abstractmethod
import unittest
from unittest.mock import patch

from stm32_spi import Stm32SpiInterface
from stm32_uart import Stm32UartInterface


class TestCase(unittest.TestCase):
    def patch_class(self, name):
        patcher = patch(name, autospec=True)
        mock_class = patcher.start()
        self.addCleanup(patcher.stop)
        return mock_class.return_value

    def patch_method(self, target, method):
        patcher = patch.object(target, method)
        mock = patcher.start()
        self.addCleanup(patcher.stop)
        return mock

    def patch_serial(self):
        return self.patch_class('serial.Serial')

    def patch_spi_toggle_reset(self):
        return self.patch_method(Stm32SpiInterface, 'toggle_reset')

    def patch_spi_configure(self):
        return self.patch_method(Stm32SpiInterface, 'configure')

    def patch_uart_configure(self):
        return self.patch_method(Stm32UartInterface, 'configure')


class TestCommandInterface(ABC):
    @abstractmethod
    def test_command_size(self):
        pass

    @abstractmethod
    def test_fields(self):
        pass


class TestCommandHelper(unittest.TestCase):
    def check_command_byte_size(self, cmd):
        cmd_bytes = bytes(cmd)
        self.assertEqual(1, len(cmd_bytes))

    def check_command_bytes_equal(self, cmd, expected_bytes):
        self.assertEqual(expected_bytes, bytes(cmd))
