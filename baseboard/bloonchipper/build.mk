# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Bloonchipper baseboard specific files build

# the IC is STmicro STM32F412
CHIP:=stm32
CHIP_FAMILY:=stm32f4
CHIP_VARIANT:=stm32f412

baseboard-y=base-board.o

# If we're mocking the sensor detection for testing (so we can test
# sensor/transport permutations in the unit tests), don't build the real sensor
# detection.
ifeq ($(HAS_MOCK_FPSENSOR_DETECT),)
	baseboard-y+=fpsensor_detect.o
endif
