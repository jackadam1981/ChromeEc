# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

# the IC is STmicro STM32F767ZI
CHIP:=stm32
CHIP_FAMILY:=stm32f4
CHIP_VARIANT:=stm32f76x

board-y=board.o
