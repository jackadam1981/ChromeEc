# -*- makefile -*-
# Copyright 2013 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# emulator specific files build
#

CORE:=host

chip-y=gpio.o lpc.o clock.o spi_controller.o
ifneq ($(USE_BUILTIN_STDLIB),1)
chip-y+=flash.o persistence.o reboot.o system.o trng.o uart.o
endif

ifndef CONFIG_KEYBOARD_DISCRETE
chip-$(HAS_TASK_KEYSCAN)+=keyboard_raw.o
endif
chip-$(CONFIG_USB_PD_TCPC)+=usb_pd_phy.o

dirs-y += chip/host/dcrypto

chip-$(CONFIG_I2C)+= i2c.o
