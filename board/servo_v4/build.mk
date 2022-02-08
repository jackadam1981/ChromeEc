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

# Not enough SRAM: Disable all tests
test-list-y=

# These files are compiled into RO
chip-ro=bkpdata.o system.o

board-rw=board.o
board-$(CONFIG_USB_POWER_DELIVERY)+=usb_pd_policy.o usb_pd_pdo.o

all_deps=$(patsubst ro,,$(def_all_deps))

# Passing the parameter MIGRATION to the make file will generate
# the migration image instead. Note: changing this parameter doesn't
# clear the build/$(BOARD) directory so it needs to be cleaned.
ifdef MIGRATION
CPPFLAGS+=-DMIGRATION=1
endif
