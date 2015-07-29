# -*- makefile -*-
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

CHIP:=g
CHIP_FAMILY:=cr50
CHIP_VARIANT ?= cr50_fpga

board-y=board.o
LDFLAGS_EXTRA += -L$(out)/tpm2/build -ltpm2

CFLAGS += -mfpu=vfpv3-d16
CFLAGS += -mfloat-abi=hard

# Need to generate a .hex file
all: hex

hex: lib_tpm2

ifeq ($(BOARD_MK_INCLUDED),)
BOARD_MK_INCLUDED=1
else
lib_tpm2:
	rsync -a ../../third_party/tpm2 $(out)
	make -j -C $(out)/tpm2
endif
