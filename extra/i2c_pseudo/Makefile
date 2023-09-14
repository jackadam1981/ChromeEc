# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Makefile for i2c-pseudo module.  Typical usage:
#   $ make
#   $ sudo make modules_install
#   $ make clean

obj-m := drivers/i2c/i2c-pseudo.o
ccflags-y := -I$(src)/include

.PHONY: all

all: modules

CFLAGS_i2c-pseudo.o += "-DHAVE_STREAM_OPEN=$(shell "$M"/check_stream_open.sh)"

.DEFAULT:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build \
		M=$(shell pwd) \
		$(MAKECMDGOALS)
