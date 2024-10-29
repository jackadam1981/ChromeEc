# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CURRENT_DIR:=$(dir $(abspath $(firstword $(MAKEFILE_LIST))))
CFLAGS=-Wall -Werror -I$(CURRENT_DIR) -I$(CURRENT_DIR)/third_party \
			 -L$(CURRENT_DIR)/third_party -li2c  -lgpiod -l:aardvark.so
SUBDIRS=test

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

um_ppm: rts5453.o tps6699x.o \
	rts5453_usermode.o tps6699x_usermode.o \
	smbus_usermode.o um_ppm_chardev.o platform_usermode.o \
	ppm_common.o \
	third_party/aardvark.o third_party/i2c_aardvark.o \
	main.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean local_tests

local_tests:
	$(MAKE) -C test run_local_tests

remote_tests:
	$(MAKE) -C test remote_unit_tests

clean:
	rm -f *.o third_party/*.o um_ppm
	$(MAKE) -C test clean
