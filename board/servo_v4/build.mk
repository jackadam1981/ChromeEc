# -*- makefile -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

# the IC is STmicro STM32F072RBT6
CHIP:=stm32
CHIP_FAMILY:=stm32f0
CHIP_VARIANT:=stm32f07x
<<<<<<< HEAD   (6f5daa driver/opt3100: Set min/max frequency that match the driver)

# Use coreboot-sdk
$(call set-option,CROSS_COMPILE_arm,\
	$(CROSS_COMPILE_coreboot_sdk_arm),\
	/opt/coreboot-sdk/bin/arm-eabi-)
=======
>>>>>>> BRANCH (40d09f ectool: motionsense: add commands for fast/manual offset com)

# Not enough SRAM: Disable all tests
test-list-y=

board-y=board.o
board-$(CONFIG_USB_POWER_DELIVERY)+=usb_pd_policy.o

all_deps=$(patsubst ro,,$(def_all_deps))
