#!/usr/bin/env python3.6
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging

from buspirate import BusPirate
from stm32_spi import stm32_bootloader_spi_debug
from stm32_uart import stm32_bootloader_uart_debug
from stm32_uart import Stm32BootloaderUartBridge
from stm32_uart import Stm32UartBridge


def mode_choice_help():
    return """
Mode Options:
  NOTE: BOOTLOADER modes require BOOT0 to be high. See README for more details.

  STM32_BOOTLOADER_SPI_DEBUG
    Send start command to STM32 bootloader over SPI and wait for ACK.

  STM32_BOOTLOADER_UART_BRIDGE
    Create a USB to UART bridge in a mode compatible with the STM32 bootloader.
    After running you can use the interface to flash the device:
    stm32mon -b 115200 -d /dev/buspirate -u -e -w ec.bin

  STM32_BOOTLOADER_UART_DEBUG
    Send start command to STM32 bootloader over UART and wait for ACK.

  STM32_UART_BRIDGE
    Create a USB to UART bridge. After running you can connect with your
    favorite terminal:
    screen /dev/buspirate 115200
"""


def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=mode_choice_help())

    default_device = '/dev/buspirate'
    parser.add_argument(
        '--device', '-d',
        metavar='DEV',
        help='BusPirate device (default: ' + default_device + ')',
        default=default_device)

    default_baud = 115200
    parser.add_argument(
        '--baud', '-b',
        metavar='BAUD',
        help='Baud rate to BusPirate (default: ' + str(default_baud) + ')',
        type=int,
        default=default_baud)

    log_level_choices = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
    parser.add_argument(
        '--log_level', '-l',
        choices=log_level_choices,
        metavar='LEVEL',
        help=', '.join(log_level_choices),
        default='DEBUG'
    )

    mode_choices = ['STM32_BOOTLOADER_SPI_DEBUG',
                    'STM32_BOOTLOADER_UART_BRIDGE',
                    'STM32_BOOTLOADER_UART_DEBUG',
                    'STM32_UART_BRIDGE']
    parser.add_argument(
        '--mode', '-m',
        choices=mode_choices,
        metavar='MODE',
        help=', '.join(mode_choices),
        default='STM32_BOOTLOADER_UART_DEBUG'
    )

    args = parser.parse_args()
    logging.basicConfig(level=args.log_level)
    bus_pirate = BusPirate(args.device, args.baud)

    if args.mode == 'STM32_BOOTLOADER_SPI_DEBUG':
        stm32_bootloader_spi_debug(bus_pirate)
    elif args.mode == 'STM32_BOOTLOADER_UART_BRIDGE':
        Stm32BootloaderUartBridge(bus_pirate)
    elif args.mode == 'STM32_BOOTLOADER_UART_DEBUG':
        stm32_bootloader_uart_debug(bus_pirate)
    elif args.mode == 'STM32_UART_BRIDGE':
        Stm32UartBridge(bus_pirate)


if __name__ == '__main__':
    main()
