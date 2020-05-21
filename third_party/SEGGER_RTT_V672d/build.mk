# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Segger RTT source files build

# Note that this variable includes the trailing "/"
_rtt_cur_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

# Make sure output directory is created (in build directory)
dirs-y+="$(_rtt_cur_dir)"

all-obj-y+=$(_rtt_cur_dir)RTT/SEGGER_RTT.o \
	$(_rtt_cur_dir)RTT/SEGGER_RTT_Printf.o
	$(_rtt_cur_dir)RTT/RTT_Syscalls_GCC.o
