#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ctypes

from bus_pirate_command import PeripheralCmd
from bus_pirate_command import WriteCmd
from buspirate_interface import BusPirateInterface


_SPI1 = 'SPI1'

_SPI_X = 'SPIx'
_SPI_CS = 'SPI_CS'
_SPI_SNIFF = 'SPI_SNIFF'
_SPI_WRITE = 'SPI_WRITE'
_SPI_PERIPHERAL = 'SPI_PERIPHERAL'
_SPI_SPEED = 'SPI_SPEED'
_SPI_SETTINGS = 'SPI_SETTINGS'
_SPI_WRITE_THEN_READ = 'SPI_WRITE_THEN_READ'
_SPI_WRITE_THEN_READ_NO_CS = 'SPI_WRITE_THEN_READ_NO_CS'

# http://dangerousprototypes.com/docs/SPI_(binary)
_BINARY_COMMANDS = {
    _SPI_X: 0x01,
    _SPI_CS: 0x02,
    _SPI_SNIFF: 0x0A,
    _SPI_WRITE: 0x10,
    _SPI_PERIPHERAL: 0x40,
    _SPI_SPEED: 0x60,
    _SPI_SETTINGS: 0x80,
    _SPI_WRITE_THEN_READ: 0x04,
    _SPI_WRITE_THEN_READ_NO_CS: 0x05,
}

# TODO(tomhughes): Commands that still need to be implemented
# SpiSniffCmd
# SpiWRiteThenReadCmd
# WriteThenReadNoCsCmd


class SpiCsCmd(ctypes.Structure):
    _fields_ = [
        ('cs', ctypes.c_uint8, 1),
        ('cmd', ctypes.c_uint8, 7),
    ]

    def __init__(self):
        super(SpiCsCmd, self).__init__()
        self.cs = 0
        self.cmd = _BINARY_COMMANDS[_SPI_CS] >> 1


class SpiBitRateCmd(ctypes.Structure):
    BIT_RATE_KHZ = {
        30: 0x00,
        125: 0x01,
        250: 0x02,
        1000: 0x03,
        2000: 0x04,
        2600: 0x05,
        4000: 0x06,
        8000: 0x07,
    }

    _fields_ = [
        ('bitrate', ctypes.c_uint8, 3),
        ('cmd', ctypes.c_uint8, 5)
    ]

    def __init__(self, bitrate):
        super(SpiBitRateCmd, self).__init__()
        self.bitrate = bitrate
        self.cmd = _BINARY_COMMANDS[_SPI_SPEED] >> 3


class SpiPeripheralCmd(PeripheralCmd):
    def __init__(self):
        super(SpiPeripheralCmd, self).__init__(
            cmd=_BINARY_COMMANDS[_SPI_PERIPHERAL] >> 4)


class SpiSettingsCmd(ctypes.Structure):
    _fields_ = [
        ('sample_time', ctypes.c_uint8, 1),
        ('clock_edge', ctypes.c_uint8, 1),
        ('clock_idle_phase', ctypes.c_uint8, 1),
        ('pin_output', ctypes.c_uint8, 1),
        ('cmd', ctypes.c_uint8, 4)
    ]

    def set_ouptut_highz(self):
        self.pin_output = 0

    def set_output_3_3v(self):
        self.pin_output = 1

    def set_clock_edge_active_to_idle(self):
        self.clock_edge = 1

    def set_clock_edge_idle_to_active(self):
        self.clock_edge = 0

    def set_clock_idle_phase_high(self):
        self.clock_idle_phase = 1

    def set_clock_idle_phase_low(self):
        self.clock_idle_phase = 0

    def set_sample_time_middle(self):
        self.sample_time = 0

    def set_sample_time_begin(self):
        self.sample_time = 1

    def __init__(self):
        super(SpiSettingsCmd, self).__init__()
        self.sample_time = 0
        self.clock_edge = 0
        self.clock_idle_phase = 0
        self.pin_output = 0
        self.cmd = _BINARY_COMMANDS[_SPI_SETTINGS] >> 4


class SpiWriteCmd(WriteCmd):
    def __init__(self, num_bytes):
        super(SpiWriteCmd, self).\
            __init__(num_bytes=num_bytes,
                     cmd=_BINARY_COMMANDS[_SPI_WRITE] >> 4)


class Spi(BusPirateInterface):
    def send_bytes(self, data_bytes):
        num_bytes = self._calc_num_bytes(data_bytes)

        cmd = SpiWriteCmd(num_bytes)
        self._serial.write(bytes(cmd))

        ret = self._serial.read(1)
        if ret != BusPirateInterface.BUS_PIRATE_CONTROL['SUCCESS']:
            self._logger.debug('Response bytes: 0x%s', ret.hex())
            raise RuntimeError('Error response from bus pirate')

        # actual data
        self._logger.debug('sending data byte: 0x%s', data_bytes.hex())
        self._serial.write(data_bytes)
        ret = self._serial.read(num_bytes)
        self._logger.debug('received data byte: 0x%s', ret.hex())
        return ret
