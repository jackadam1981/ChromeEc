# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Segger RTT source files build

# Note that this variable includes the trailing "/"
_rtt_cur_dir:=$(dir $(lastword $(MAKEFILE_LIST)))
_rtt_src_dir:=$(_rtt_cur_dir)RTT
_syscalls_src_dir:=$(_rtt_cur_dir)Syscalls

# Make sure output directory is created (in build directory)
dirs-y+="$(_rtt_cur_dir)"
dirs-y+="$(_rtt_src_dir)"
dirs-y+="$(_syscalls_src_dir)"

all-obj-ro+=$(_rtt_src_dir)/SEGGER_RTT.o
all-obj-ro+=$(_rtt_src_dir)/SEGGER_RTT_printf.o
#all-obj-ro+=$(_syscalls_src_dir)/SEGGER_RTT_Syscalls_GCC.o
