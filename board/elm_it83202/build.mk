#-*- makefile -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

# ITE IT83202
CHIP:=it83xx
CHIP_FAMILY:=it8xxx2
CHIP_VARIANT:=it83202bx

board-y = board.o battery.o led.o
board-$(CONFIG_USB_POWER_DELIVERY)+=usb_pd_policy.o
