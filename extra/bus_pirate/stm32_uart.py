#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from abc import ABC
import logging
import time

import buspirate_uart


class Stm32UartInterface(ABC):
    BOOTLOADER_COMMANDS = {
        'START':    b'\x7F',
        'ACK':      b'\x79',
        'NACK':     b'\x1F',
    }

    def configure(self):
        self._bus_pirate.bitbang.enable_binary_mode()
        self._bus_pirate.bitbang.enable_uart_mode()

        uart_settings = buspirate_uart.UartSettingsCmd()
        uart_settings.set_output_3_3v()
        uart_settings.set_data_and_parity(self._parity)
        uart_settings.set_stop_bits_one()
        uart_settings.set_rx_idle_polarity_one()
        self._uart.send_command(uart_settings)

        uart_baud = buspirate_uart.UartBaudRateCmd(
            buspirate_uart.UartBaudRateCmd.BAUD[115200])
        self._uart.send_command(uart_baud)

        # set power and AUX (RST#)
        uart_peripheral = buspirate_uart.UartPeripheralCmd()
        uart_peripheral.power = 1
        uart_peripheral.aux = 1
        self._uart.send_command(uart_peripheral)

        # Drop AUX (RST#) low
        uart_peripheral.aux = 0
        self._uart.send_command(uart_peripheral)
        time.sleep(0.5)

        # Raise AUX (RST#) high
        uart_peripheral.aux = 1
        self._uart.send_command(uart_peripheral)
        time.sleep(0.5)

        self._uart.enable_rx()

    def __init__(self, bus_pirate, parity):
        self._logger = logging.getLogger(self.__class__.__name__)
        self._bus_pirate = bus_pirate
        self._uart = bus_pirate.uart
        self._parity = parity


class Stm32BootloaderUart(Stm32UartInterface):
    def send_command(self, cmd_byte):
        checksum = 0xFF ^ cmd_byte
        cmd = bytearray(2)
        cmd[0] = cmd_byte
        cmd[1] = checksum
        self._uart.send_bytes(cmd)

    def send_start(self):
        self._uart.send_bytes(self.BOOTLOADER_COMMANDS['START'])

    def wait_for_ack(self):
        self._uart.wait_for_bytes(self.BOOTLOADER_COMMANDS['ACK'])

    def __init__(self, bus_pirate):
        self._bus_pirate = bus_pirate
        self._uart = bus_pirate.uart
        parity = buspirate_uart.UartSettingsCmd().DataAndParity.\
            EIGHT_DATA_BITS_EVEN_PARITY_BIT
        super(Stm32BootloaderUart, self).__init__(self._bus_pirate, parity)
        self.configure()


class Stm32BootloaderUartBridge(Stm32BootloaderUart):
    def __init__(self, bus_pirate):
        super(Stm32BootloaderUartBridge, self).__init__(bus_pirate)
        self._uart.enter_bridge_mode()


class Stm32UartBridge(Stm32UartInterface):
    def __init__(self, bus_pirate):
        self._bus_pirate = bus_pirate

        parity = buspirate_uart.UartSettingsCmd().DataAndParity.\
            EIGHT_DATA_BITS_ZERO_PARITY_BITS
        super(Stm32UartBridge, self).__init__(self._bus_pirate, parity)

        self.configure()
        self._uart.enter_bridge_mode()


def stm32_bootloader_uart_debug(bus_pirate):
    stm32_bootloader_uart = Stm32BootloaderUart(bus_pirate)
    stm32_bootloader_uart.send_start()
    stm32_bootloader_uart.wait_for_ack()
