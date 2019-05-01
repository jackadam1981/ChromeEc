#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import serial

import buspirate_bitbang
import buspirate_spi
import buspirate_uart


class BusPirate:
    def __init__(self, device, baud_rate):
        self.logger = logging.getLogger('BusPirate')
        self.logger.info('Trying to connect to device: %s at baudrate: %d',
                         device, baud_rate)
        self._serial = serial.Serial(device, baud_rate, timeout=0.1)
        self.bitbang = buspirate_bitbang.Bitbang(self._serial)
        self.uart = buspirate_uart.Uart(self._serial)
        self.spi = buspirate_spi.Spi(self._serial)
