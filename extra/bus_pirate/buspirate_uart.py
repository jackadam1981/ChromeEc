#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ctypes
from enum import Enum
import time

from bus_pirate_command import PeripheralCmd
from bus_pirate_command import WriteCmd
from buspirate_interface import BusPirateInterface

_BBIO_X = 'BBIOx'
_ART_X = 'ARTx'
_UART_RX = 'UART_RX'
_UART_MANUAL_BAUD = 'UART_MANUAL_BAUD'
_UART_BRIDGE_MODE = 'UART_BRIDGE_MODE'
_UART_WRITE = 'UART_WRITE'
_UART_PERIPHERAL = 'UART_PERIPHERAL'
_UART_BAUD = 'UART_BAUD'
_UART_SETTINGS = 'UART_SETTINGS'

# http://dangerousprototypes.com/docs/UART_(binary)
_BINARY_COMMANDS = {
    _BBIO_X: 0x00,
    _ART_X: 0x01,
    _UART_RX: 0x02,
    _UART_MANUAL_BAUD: 0x03,
    _UART_BRIDGE_MODE: 0x0F,
    _UART_WRITE: 0x10,
    _UART_PERIPHERAL: 0x40,
    _UART_BAUD: 0x60,
    _UART_SETTINGS: 0x80,
}


class UartRxCmd(ctypes.Structure):
    _fields_ = [
        ('rx', ctypes.c_uint8, 1),
        ('cmd', ctypes.c_uint8, 7),
    ]

    def start(self):
        self.rx = 0

    def stop(self):
        self.rx = 1

    def __init__(self):
        super(UartRxCmd, self).__init__()
        self.rx = 0
        self.cmd = _BINARY_COMMANDS[_UART_RX] >> 1


class UartWriteCmd(WriteCmd):
    def __init__(self, num_bytes):
        super(UartWriteCmd, self).\
            __init__(num_bytes=num_bytes,
                     cmd=_BINARY_COMMANDS[_UART_WRITE] >> 4)


class UartPeripheralCmd(PeripheralCmd):
    def __init__(self):
        super(UartPeripheralCmd, self).__init__(
            cmd=_BINARY_COMMANDS[_UART_PERIPHERAL] >> 4)


class UartBaudRateCmd(ctypes.Structure):
    BAUD = {
        300: 0x00,
        1200: 0x01,
        2400: 0x02,
        4800: 0x03,
        9600: 0x04,
        19200: 0x05,
        31250: 0x06,
        38400: 0x07,
        57600: 0x08,
        115200: 0x09
    }

    _fields_ = [
        ('baud', ctypes.c_uint8, 4),
        ('cmd', ctypes.c_uint8, 4)
    ]

    def __init__(self, baud):
        super(UartBaudRateCmd, self).__init__()
        self.baud = baud
        self.cmd = _BINARY_COMMANDS[_UART_BAUD] >> 4


class UartSettingsCmd(ctypes.Structure):
    _fields_ = [
        ('rx_idle_polarity', ctypes.c_uint8, 1),
        ('stop_bits', ctypes.c_uint8, 1),
        ('data_and_parity', ctypes.c_uint8, 2),
        ('pin_output', ctypes.c_uint8, 1),
        ('cmd', ctypes.c_uint8, 3)
    ]

    def set_ouptut_highz(self):
        self.pin_output = 0

    def set_output_3_3v(self):
        self.pin_output = 1

    def set_stop_bits_one(self):
        self.stop_bits = 0

    def set_stop_bits_two(self):
        self.stop_bits = 1

    def set_rx_idle_polarity_one(self):
        self.rx_idle_polarity = 0

    def set_rx_idle_polarity_zero(self):
        self.rx_idle_polarity = 1

    class DataAndParity(Enum):
        EIGHT_DATA_BITS_ZERO_PARITY_BITS = 0
        EIGHT_DATA_BITS_EVEN_PARITY_BIT = 1
        EIGHT_DATA_BITS_ODD_PARITY_BIT = 2
        NINE_DATA_BITS_EVEN_PARITY_BIT = 3

    def set_data_and_parity(self, data_and_parity):
        self.data_and_parity = data_and_parity.value

    def __init__(self):
        super(UartSettingsCmd, self).__init__()
        self.rx_idle_polarity = 0
        self.stop_bits = 0
        self.data_and_parity = self.DataAndParity.\
            EIGHT_DATA_BITS_ZERO_PARITY_BITS.value
        self.pin_output = 0
        self.cmd = _BINARY_COMMANDS[_UART_SETTINGS] >> 5


class UartBridgeModeCmd(ctypes.Structure):
    _fields_ = [
        ('cmd', ctypes.c_uint8, 8)
    ]

    def __init__(self):
        super(UartBridgeModeCmd, self).__init__()
        self.cmd = _BINARY_COMMANDS[_UART_BRIDGE_MODE]


class Uart(BusPirateInterface):
    def send_bytes(self, data_bytes):
        num_bytes = self._calc_num_bytes(data_bytes)
        uart_write = UartWriteCmd(num_bytes)
        if not self.send_command(uart_write):
            raise RuntimeError('Unable to send UART write command')

        # actual data
        self._serial.write(data_bytes)
        ret = self._serial.read(num_bytes)
        success_val = int.from_bytes(
            BusPirateInterface.BUS_PIRATE_CONTROL['SUCCESS'],
            byteorder='little')
        expected = bytes([success_val for i in range(0, num_bytes)])
        if ret != expected:
            raise RuntimeError('Error response from bus pirate')

    def enter_bridge_mode(self):
        self._logger.info('Entering bridge mode')
        bridge_mode_cmd = UartBridgeModeCmd()
        return self.send_command(bridge_mode_cmd)

    def enable_rx(self):
        # enable display of RX
        self._logger.info('Enabling RX read')
        uart_rx = UartRxCmd()
        uart_rx.start()
        if not self.send_command(uart_rx):
            raise RuntimeError('Unable to modify RX settings')
        time.sleep(1)
