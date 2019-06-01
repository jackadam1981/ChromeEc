# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

# the IC is STmicro STM32H7A3
CHIP:=stm32
CHIP_FAMILY:=stm32h7
CHIP_VARIANT:=stm32h7a

board-y=board.o

test-list-y=aes sha256 sha256_unrolled
