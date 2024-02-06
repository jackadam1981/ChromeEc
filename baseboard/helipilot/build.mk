# -*- makefile -*-
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Helipilot baseboard specific files build
#

CHIP := npcx
CHIP_FAMILY := npcx9
CHIP_VARIANT := npcx9mfp

baseboard-y += base_board.o
baseboard-rw += base_board_rw.o

# If we're mocking the sensor detection for testing (so we can test
# sensor/transport permutations in the unit tests), don't build the real sensor
# detection.
ifeq ($(HAS_MOCK_FPSENSOR_DETECT),)
	baseboard-y += fpsensor_detect.o
	baseboard-rw += fpsensor_detect_rw.o
endif
