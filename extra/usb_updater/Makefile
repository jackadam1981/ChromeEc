# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CC ?= gcc
PKG_CONFIG ?= pkg-config
PROGRAMS := usb_updater usb_updater2
LIBS    :=
LFLAGS  :=
CFLAGS  := -std=gnu99 \
	-g \
        -Wall \
        -Werror \
        -Wpointer-arith \
        -Wcast-align \
        -Wcast-qual \
        -Wundef \
        -Wsign-compare \
        -Wredundant-decls \
        -Wmissing-declarations

ifeq (DEBUG,)
CFLAGS += -O3
else
CFLAGS += -O0
endif

#
# Add libusb-1.0 required flags
#
LIBS    += $(shell $(PKG_CONFIG) --libs   libusb-1.0) -lfmap
CFLAGS  += $(shell $(PKG_CONFIG) --cflags libusb-1.0)
CFLAGS  += -I../../include -I../../util


LIBS_cr50  = $(shell $(PKG_CONFIG) --libs   libcrypto)
CFLAGS_cr50 = $(shell $(PKG_CONFIG) --cflags libcrypto)
CFLAGS_cr50  += -I../../board/cr50 -I ../../chip/g

all: $(PROGRAMS)

# cr50 updater
usb_updater: usb_updater.c Makefile
	$(CC) $(CFLAGS) $(CFLAGS_cr50) $< $(LFLAGS) $(LIBS) $(LIBS_cr50) -o $@

# common EC code USB updater
usb_updater2: usb_updater2.c Makefile
	$(CC) $(CFLAGS) $< $(LFLAGS) $(LIBS) -o $@

.PHONY: clean

clean:
	rm -rf $(PROGRAMS) *~
