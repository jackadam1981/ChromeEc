# -*- makefile -*-
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Baseboard specific files build
#

baseboard-y=baseboard.o
baseboard-y+=board_chipset.o
baseboard-y+=board_id.o
baseboard-y+=hibernate.o
baseboard-$(VARIANT_EC_IT81202)+=regulator.o
baseboard-$(VARIANT_EC_IT81202)+=usbc_config_it81202.o
baseboard-$(VARIANT_EC_NPCX9M6F)+=usbc_config_npcx9m6f.o
baseboard-$(CONFIG_USB_POWER_DELIVERY)+=usb_pd_policy.o
