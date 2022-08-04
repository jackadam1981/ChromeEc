# -*- makefile -*-
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# AMS FFT build file.
#
_ams_fft_cur_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

dirs-$(CONFIG_ALS_TCS3410)+="$(_ams_fft_cur_dir)"
all-obj-$(CONFIG_ALS_TCS3410)+=$(_ams_fft_cur_dir)ams_fft.o $(_ams_fft_cur_dir)ams_helper.o
