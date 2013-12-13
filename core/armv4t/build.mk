# -*- makefile -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# ARMv4T core OS files build
#

# Select ARM bare-metal toolchain
CROSS_COMPILE?=arm-none-eabi-

# CPU specific compilation flags
CFLAGS_CPU+=-march=armv4t -marm -Os -mno-sched-prolog
CFLAGS_CPU+=-mno-unaligned-access

core-y=cpu.o init.o panic.o switch.o task.o lib1funcs.o
core-$(CONFIG_WATCHDOG)+=watchdog.o
