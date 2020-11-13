# -*- makefile -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build
#

CHIP:=stm32
CHIP_FAMILY:=stm32g4
ifdef BOARD_P1
CHIP_VARIANT:=stm32g473xc
else
CHIP_VARIANT:=stm32g431xb
endif
BASEBOARD:=honeybuns

board-y=board.o
