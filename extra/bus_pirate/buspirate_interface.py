#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from abc import ABC
from abc import abstractmethod
import logging


class BusPirateInterface(ABC):
    BUS_PIRATE_CONTROL = {
        'SUCCESS':  b'\x01'
    }

    @abstractmethod
    def send_bytes(self, data_bytes):
        pass

    def _calc_num_bytes(self, data_bytes):
        self._logger.debug('Sending bytes: 0x%s', data_bytes.hex())
        num_bytes = len(data_bytes)
        if num_bytes < 1 or num_bytes > 16:
            raise ValueError('Can send 1 to 16 bytes at once')
        return num_bytes

    def send_command(self, cmd):
        cmd_byte = bytes(cmd)
        self._logger.debug('Sending %d bytes: 0x%s', len(cmd_byte),
                           cmd_byte.hex())
        assert len(cmd_byte) == 1
        self._serial.write(cmd_byte)
        read_bytes = self._serial.read(1)
        if read_bytes != self.BUS_PIRATE_CONTROL.get('SUCCESS'):
            self._logger.warning('read_bytes: 0x%s', read_bytes.hex())
            return False
        return True

    def wait_for_bytes(self, bytes_):
        self._logger.debug('Waiting for bytes: 0x%s', bytes_.hex())
        num_bytes = len(bytes_)
        while True:
            ret = self._serial.read(num_bytes)
            if ret == bytes_:
                self._logger.debug("0x%s", bytes_.hex())
                break

    def __init__(self, serial):
        self._serial = serial
        self._logger = logging.getLogger(self.__class__.__name__)
