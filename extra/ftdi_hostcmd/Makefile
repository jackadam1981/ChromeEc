# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use your own libmpsse if you want, but we're going to use the files
# that are part of the Chromium OS trunks_client program.
PLATFORM2 = ../../../../platform2
MPSSE_DIR = $(PLATFORM2)/trunks/ftdi

PROG = test_cmds
SRCS = test_cmds.c $(MPSSE_DIR)/mpsse.c $(MPSSE_DIR)/support.c

BUILD_CFLAGS = \
	-std=gnu99 \
	-g3 \
	-O3 \
        -Wall \
        -Werror \
        -Wpointer-arith \
        -Wcast-align \
        -Wcast-qual \
        -Wundef \
        -Wsign-compare \
        -Wredundant-decls \
        -Wmissing-declarations

BUILD_CFLAGS += -I../../include -I${MPSSE_DIR} -I${PLATFORM2}

BUILD_CFLAGS += $(shell ${BUILD_PKG_CONFIG} --cflags libusb-1.0 libftdi1)
BUILD_LIBS   += $(shell ${BUILD_PKG_CONFIG} --libs   libusb-1.0 libftdi1)

$(PROG): $(SRCS) Makefile
	$(BUILD_CC) $(BUILD_CFLAGS) $(SRCS) $(BUILD_LDFLAGS) $(BUILD_LIBS) -o $@

.PHONY: clean
clean:
	rm -rf $(PROG)
