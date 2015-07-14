# -*- makefile -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# USB PD files build
#

usb_pd-y=

usb_pd-$(CONFIG_USB_POWER_DELIVERY)+=protocol.o policy.o
usb_pd-$(CONFIG_USB_PD_LOGGING)+=log.o
usb_pd-$(CONFIG_USB_PD_TCPC)+=tcpc.o

