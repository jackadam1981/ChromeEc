# Copyright 2019 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build for USB Type-C and Power Delivery

# Note that this variable includes the trailing "/"
_pd_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

all-obj-$(CONFIG_PLATFORM_EC_USB_PD_INTEL_ALTMODE)+=$(_pd_dir)pd_task_intel_altmode.o
