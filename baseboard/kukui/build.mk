# -*- makefile -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Baseboard specific files build
#

baseboard-y=baseboard.o
baseboard-$(CONFIG_USB_POWER_DELIVERY)+=usb_pd_policy.o
ifeq ($(CHIP),stm32)
baseboard-$(CONFIG_BOOTBLOCK)+=emmc.o
else
baseboard-$(CONFIG_BOOTBLOCK)+=emmc_ite.o
endif

baseboard-$(VARIANT_KUKUI_BATTERY_MAX17055)+=battery_max17055.o
baseboard-$(VARIANT_KUKUI_BATTERY_MM8013)+=battery_mm8013.o
baseboard-$(VARIANT_KUKUI_BATTERY_SMART)+=battery_smart.o

baseboard-$(VARIANT_KUKUI_CHARGER_MT6370)+=charger_mt6370.o

baseboard-$(VARIANT_KUKUI_POGO_KEYBOARD)+=base_detect_kukui.o

ifeq ($(CHIP),stm32)
$(out)/RO/baseboard/$(BASEBOARD)/emmc.o: $(out)/bootblock_data.h
else
$(out)/RO/baseboard/$(BASEBOARD)/emmc_ite.o: $(out)/bootblock_data.h
endif

# bootblock size from 12769.0
DEFAULT_BOOTBLOCK_SIZE:=21504
