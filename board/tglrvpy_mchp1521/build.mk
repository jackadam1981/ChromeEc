# -*- makefile -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Intel ADL-P-RVP-ITE board-specific configuration
#

#it8320
#CHIP:=it83xx
#CHIP_FAMILY:=it8320
#CHIP_VARIANT:=it8320dx
#BASEBOARD:=intelrvp

# the IC is Microchip MEC1521
# external SPI is 512KB
# clock is Internal ROSC
CHIP:=mchp
CHIP_FAMILY:=mec152x
CHIP_VARIANT:=mec1521
CHIP_SPI_SIZE_KB:=512
BASEBOARD:=intelrvp

board-y=board.o
board-$(CONFIG_BATTERY_SMART)+=battery.o
