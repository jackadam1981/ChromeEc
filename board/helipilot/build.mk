# -*- makefile -*-
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Helipilot board specific files build
#

CHIP:=npcx
CHIP_FAMILY:=npcx9
CHIP_VARIANT:=npcx9m8s

board-y=
board-y+=board.o
board-rw=board_rw.o

# If we're mocking the sensor detection for testing (so we can test
# sensor/transport permutations in the unit tests), don't build the real sensor
# detection.
ifeq ($(HAS_MOCK_FPSENSOR_DETECT),)
	board-y+=fpsensor_detect.o
	board-rw+=fpsensor_detect_rw.o
endif