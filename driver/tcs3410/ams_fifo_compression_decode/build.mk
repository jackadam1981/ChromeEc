# -*- makefile -*-
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# AMS FIFO compression decode build file.
#
_ams_fifo_cur_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

dirs-$(CONFIG_ALS_TCS3410)+="$(_ams_fifo_cur_dir)"
all-obj-$(CONFIG_ALS_TCS3410)+=$(_ams_fifo_cur_dir)ams_fifo_decode.o
