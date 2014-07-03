# -*- makefile -*-
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build
#

# the IC is SMSC MEC1322 / external SPI is 2MB / external clock is crystal
CHIP:=mec1322
CHIP_SPI_SIZE:=2
CHIP_EXT_CLOCK:=crystal

board-y=board.o
board-$(HAS_TASK_CHIPSET)+=power_sequence.o
