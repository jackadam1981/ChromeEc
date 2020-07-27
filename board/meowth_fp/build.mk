<<<<<<< HEAD   (6f5daa driver/opt3100: Set min/max frequency that match the driver)
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

# the IC is STmicro STM32H743
CHIP:=stm32
CHIP_FAMILY:=stm32h7
CHIP_VARIANT:=stm32h7x3

board-y=board.o

test-list-y=aes sha256 sha256_unrolled
=======
>>>>>>> BRANCH (40d09f ectool: motionsense: add commands for fast/manual offset com)
