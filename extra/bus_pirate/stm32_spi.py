#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from abc import ABC
import logging
import time

import buspirate_spi


# See https://www.st.com/resource/en/application_note/dm00081379.pdf
class Stm32SpiInterface(ABC):
    BOOTLOADER_COMMANDS = {
        'START':    b'\x5A',
        'ACK':      b'\x79',
        'NACK':     b'\x1F',
    }

    def configure(self):
        self._logger.info('Configuring SPI')
        self._bus_pirate.bitbang.enable_binary_mode()
        self._bus_pirate.bitbang.enable_spi_mode()

        # Bitrate 1MHz
        spi_bitrate = buspirate_spi.SpiBitRateCmd(
            buspirate_spi.SpiBitRateCmd.BIT_RATE_KHZ[1000])
        self._spi.send_command(spi_bitrate)

        # Settings
        spi_settings = buspirate_spi.SpiSettingsCmd()
        spi_settings.set_output_3_3v()
        spi_settings.set_clock_edge_active_to_idle()
        spi_settings.set_clock_idle_phase_low()
        spi_settings.set_sample_time_middle()
        self._spi.send_command(spi_settings)

        # CS low
        spi_cs = buspirate_spi.SpiCsCmd()
        spi_cs.cs = 0
        self._spi.send_command(spi_cs)

        # Peripherals
        spi_peripherals = buspirate_spi.SpiPeripheralCmd()
        spi_peripherals.cs = 0
        spi_peripherals.aux = 1
        spi_peripherals.pullups = 1
        spi_peripherals.power = 1
        self._spi.send_command(spi_peripherals)

    def toggle_reset(self):
        # reset high
        spi_peripherals_reset_high = buspirate_spi.SpiPeripheralCmd()
        spi_peripherals_reset_high.cs = 0
        spi_peripherals_reset_high.aux = 1
        spi_peripherals_reset_high.pullups = 1
        spi_peripherals_reset_high.power = 1

        # reset low
        spi_peripherals_reset_low = buspirate_spi.SpiPeripheralCmd()
        spi_peripherals_reset_low.cs = 0
        spi_peripherals_reset_low.aux = 0
        spi_peripherals_reset_low.pullups = 1
        spi_peripherals_reset_low.power = 1

        self._spi.send_command(spi_peripherals_reset_high)
        time.sleep(0.5)
        self._spi.send_command(spi_peripherals_reset_low)
        time.sleep(0.5)
        self._spi.send_command(spi_peripherals_reset_high)

    def __init__(self, bus_pirate):
        self._logger = logging.getLogger(self.__class__.__name__)
        self._bus_pirate = bus_pirate
        self._spi = bus_pirate.spi


class Stm32BootloaderSpi(Stm32SpiInterface):
    def send_command(self, cmd_byte):
        assert len(cmd_byte) == 1
        checksum = 0xFF ^ cmd_byte
        cmd = bytearray(3)
        cmd[0] = self.BOOTLOADER_COMMANDS['START']
        cmd[1] = cmd_byte
        cmd[2] = checksum
        return self._spi.send_bytes(cmd)

    def send_start(self):
        return self._spi.send_bytes(self.BOOTLOADER_COMMANDS['START'])

    def wait_for_ack(self):
        dummy = self._spi.send_bytes(b'\x00')
        while True:
            ack_or_nack = self._spi.send_bytes(b'\x00')
            if ack_or_nack in (
                    self.BOOTLOADER_COMMANDS['ACK'],
                    self.BOOTLOADER_COMMANDS['NACK']):
                break
        dummy = self._spi.send_bytes(self.BOOTLOADER_COMMANDS['ACK'])

    def __init__(self, bus_pirate):
        super(Stm32BootloaderSpi, self).__init__(bus_pirate)
        self.configure()
        self.toggle_reset()


def stm32_bootloader_spi_debug(bus_pirate):
    stm32_bootloader_uart = Stm32BootloaderSpi(bus_pirate)
    stm32_bootloader_uart.send_start()
    stm32_bootloader_uart.wait_for_ack()
