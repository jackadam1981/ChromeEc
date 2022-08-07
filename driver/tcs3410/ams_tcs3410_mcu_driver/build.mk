# -*- makefile -*-
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# AMS FFT build file.
#
_ams_driver_cur_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

dirs-$(CONFIG_ALS_TCS3410)+="$(_ams_driver_cur_dir)"
all-obj-$(CONFIG_ALS_TCS3410)+=$(_ams_driver_cur_dir)devices/ams_errno.o
all-obj-$(CONFIG_ALS_TCS3410)+=$(_ams_driver_cur_dir)devices/ams_device.o
all-obj-$(CONFIG_ALS_TCS3410)+=$(_ams_driver_cur_dir)devices/tcs3410/i2c_stubs.o
all-obj-$(CONFIG_ALS_TCS3410)+=$(_ams_driver_cur_dir)devices/tcs3410/tcs3410_als.o
all-obj-$(CONFIG_ALS_TCS3410)+=$(_ams_driver_cur_dir)devices/tcs3410/tcs3410.o
all-obj-$(CONFIG_ALS_TCS3410)+=$(_ams_driver_cur_dir)devices/tcs3410/tcs3410_fd.o
all-obj-$(CONFIG_ALS_TCS3410)+=$(_ams_driver_cur_dir)devices/tcs3410/tcs3410_utils.o
all-obj-$(CONFIG_ALS_TCS3410)+=$(_ams_driver_cur_dir)devices/tcs3410/tcs3410_irq.o
all-obj-$(CONFIG_ALS_TCS3410)+=$(_ams_driver_cur_dir)devices/tcs3410/tcs3410_fifo.o
