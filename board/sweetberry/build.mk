# -*- makefile -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

CHIP:=stm32
CHIP_FAMILY:=stm32f4
CHIP_VARIANT:=stm32f446

# Use coreboot-sdk
$(call set-option,CROSS_COMPILE_arm,\
	$(CROSS_COMPILE_coreboot_sdk_arm),\
	/opt/coreboot-sdk/bin/arm-eabi-)

board-y=board.o
