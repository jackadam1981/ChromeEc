# -*- makefile -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Driver for AMS TCS3410
#

# Note that this variable includes the trailing "/"
_tcs3410_cur_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

include $(_tcs3410_cur_dir)ams_fft/build.mk
