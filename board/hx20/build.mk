# -*- makefile -*-
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build
#

CHIP:=mchp

CHIP_FAMILY:=mec152x
# MEC1521 144 WFBGA
CHIP_VARIANT:=mec152x_3400

CHIP_SPI_SIZE_KB:=512

board-y=board.o led.o cypress5525.o cpu_power.o power_sequence.o
board-$(CONFIG_KEYBOARD_CUSTOMIZATION)+=keyboard_customization.o
board-$(CONFIG_BATTERY_SMART)+=battery.o
board-$(CONFIG_PECI) += peci_customization.o
board-$(CONFIG_SYSTEMSERIAL_DEBUG) += system_serial.o
