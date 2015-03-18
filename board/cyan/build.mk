# -*- makefile -*-
# Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
# Copyright (C) 2015 Intel Corporation.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build
#

# the IC is SMSC MEC1322 / external SPI is 4MB / external clock is crystal
CHIP:=mec1322
CHIP_SPI_SIZE_KB:=4096

board-y=board.o
# As this file is read more than once, must put the rules
# elsewhere (Makefile.rules) and just use variable to trigger them
PROJECT_EXTRA+=${out}/ec.spi.bin
