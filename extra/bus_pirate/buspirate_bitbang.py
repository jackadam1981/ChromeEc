#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ctypes

from buspirate_interface import BusPirateInterface

_BITBANG_RESET = 'BBIO1'
_BITBANG_ENTER_SPI = 'SPI1'
_BITBANG_ENTER_I2C = 'I2C1'
_BITBANG_ENTER_UART = 'ART1'
_BITBANG_ENTER_1WIRE = '1W01'
_BITBANG_ENTER_RAW_WIRE = 'RAW1'
_BITBANG_ENTER_OPENOCD_JTAG = 'BITBANG_ENTER_OPENOCD_JTAG'
_BITBANG_RESET2 = 'RESET'
_BITBANG_SELF_TESTS = 'STEST'
_BITBANG_SETUP_PWM = 'BITBANG_SETUP_PWM'
_BITBANG_CLEAR_PWM = 'BITBANG_CLEAR_PWM'
_BITBANG_VOLTAGE_PROBE = 'BITBANG_VOLTAGE_PROBE'
_BITBANG_VOLTAGE_PROBE_CONTINUOUS = 'BITBANG_VOLTAGE_PROBE_CONTINUOUS'
_BITBANG_MEASURE_HZ = 'BITBANG_MEASURE_HZ'
_BITBANG_CONFIGURE_PIN_DIRECTION = 'BITBANG_CONFIGURE_PIN_DIRECTION'
_BITBANG_CONFIGURE_PIN_STATE = 'BITBANG_CONFIGURE_PIN_STATE'

# http://dangerousprototypes.com/docs/Bitbang
_BINARY_COMMANDS = {
    _BITBANG_RESET: 0x00,
    _BITBANG_ENTER_SPI: 0x01,
    _BITBANG_ENTER_I2C: 0x02,
    _BITBANG_ENTER_UART: 0x03,
    _BITBANG_ENTER_1WIRE: 0x04,
    _BITBANG_ENTER_RAW_WIRE: 0x05,
    _BITBANG_ENTER_OPENOCD_JTAG: 0x06,
    _BITBANG_RESET2: 0x0F,
    _BITBANG_SELF_TESTS: 0x80,
    _BITBANG_SETUP_PWM: 0x12,
    _BITBANG_CLEAR_PWM: 0x13,
    _BITBANG_VOLTAGE_PROBE: 0x14,
    _BITBANG_VOLTAGE_PROBE_CONTINUOUS: 0x15,
    _BITBANG_MEASURE_HZ: 0x16,
    _BITBANG_CONFIGURE_PIN_DIRECTION: 0x40,
    _BITBANG_CONFIGURE_PIN_STATE: 0x80,
}


class BitbangResetCmd(ctypes.Structure):
    _fields_ = [
        ('cmd', ctypes.c_uint8, 8),
    ]

    @staticmethod
    def expected_response():
        return bytes(_BITBANG_RESET, encoding='ascii')

    def __init__(self):
        super(BitbangResetCmd, self).__init__()
        self.cmd = _BINARY_COMMANDS[_BITBANG_RESET]


class BitbangEnterSpiCmd(ctypes.Structure):
    _fields_ = [
        ('cmd', ctypes.c_uint8, 8),
    ]

    @staticmethod
    def expected_response():
        return bytes(_BITBANG_ENTER_SPI, encoding='ascii')

    def __init__(self):
        super(BitbangEnterSpiCmd, self).__init__()
        self.cmd = _BINARY_COMMANDS[_BITBANG_ENTER_SPI]


class BitbangEnterUartCmd(ctypes.Structure):
    _fields_ = [
        ('cmd', ctypes.c_uint8, 8),
    ]

    @staticmethod
    def expected_response():
        return bytes(_BITBANG_ENTER_UART, encoding='ascii')

    def __init__(self):
        super(BitbangEnterUartCmd, self).__init__()
        self.cmd = _BINARY_COMMANDS[_BITBANG_ENTER_UART]


class Bitbang(BusPirateInterface):
    def send_bytes(self, data_bytes):
        pass

    def enable_binary_mode(self):
        self._logger.info('Enabling bitbang mode')

        # Protocol requires sending 0x00 at least 20 times
        # http://dangerousprototypes.com/docs/Bitbang#00000000_-_Reset.2C_responds_.22BBIO1.22
        got_resp = False
        reset_cmd = BitbangResetCmd()
        for dummy in range(0, 20):
            if self.change_mode(reset_cmd):
                got_resp = True
                break

        if not got_resp:
            raise RuntimeError('Failed to enter bitbang mode')

    def enable_uart_mode(self):
        enter_uart_cmd = BitbangEnterUartCmd()
        if not self.change_mode(enter_uart_cmd):
            raise RuntimeError('Failed to enter UART mode')
        self._logger.info('Enabled binary UART mode')

    def enable_spi_mode(self):
        enter_spi_cmd = BitbangEnterSpiCmd()
        if not self.change_mode(enter_spi_cmd):
            raise RuntimeError('Failed to enter SPI mode')
        self._logger.info('Enabled binary SPI mode')

    def change_mode(self, cmd):
        self._serial.write(bytes(cmd))

        expected_read_bytes = cmd.expected_response()
        read_bytes = self._serial.read(len(expected_read_bytes))
        if read_bytes != expected_read_bytes:
            return False
        return True
