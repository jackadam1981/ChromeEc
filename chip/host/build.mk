# -*- makefile -*-
# Copyright 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# emulator specific files build
#

CORE:=host

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
chip-y=system.o gpio.o uart.o persistence.o flash.o lpc.o reboot.o i2c.o \
	clock.o
chip-$(HAS_TASK_KEYSCAN)+=keyboard_raw.o
chip-$(CONFIG_USB_PD_TCPC)+=usb_pd_phy.o
=======
chip-y=system.o gpio.o uart.o persistence.o flash.o lpc.o reboot.o \
	clock.o spi_master.o trng.o
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)

ifndef CONFIG_KEYBOARD_NOT_RAW
chip-$(HAS_TASK_KEYSCAN)+=keyboard_raw.o
endif
chip-$(CONFIG_USB_PD_TCPC)+=usb_pd_phy.o

dirs-y += chip/host/dcrypto

chip-$(CONFIG_I2C)+= i2c.o
