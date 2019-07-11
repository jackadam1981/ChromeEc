# -*- makefile -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Baseboard specific files build
#

baseboard-y=baseboard.o
baseboard-$(CONFIG_USB_POWER_DELIVERY)+=usb_pd_policy.o
baseboard-$(CONFIG_BOOTBLOCK)+=emmc.o

# TODO(b:137172860) move mm8013 profile to a separate file */
baseboard-$(VARIANT_KUKUI_BATTERY_MAX17055)+=battery_max17055.o
baseboard-$(VARIANT_KUKUI_BATTERY_MM8013)+=battery_max17055.o
baseboard-$(VARIANT_KUKUI_BATTERY_SMART)+=battery_smart.o

$(out)/RO/baseboard/$(BOARD)/emmc.o: $(out)/bootblock_data.h
