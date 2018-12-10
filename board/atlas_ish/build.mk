# -*- makefile -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build
#

CHIP:=ish
CHIP_FAMILY:=ish3
CHIP_VARIANT:=ish3p0

# Before we enable C++ in default toolchain, use SDK toolchain.
CXX=i686-pc-linux-gnu-gcc

centroiding = board/$(BOARD)/centroiding

-include $(centroiding)/ec_build.mk

board-y = board.o $(addprefix centroiding/, $(centroiding-y))

ifeq (,$(findstring $(centroiding), $(dirs-y)))
dirs-y += board/$(BOARD)/centroiding
endif

EXTRA_OPT_FLAGS = \
	-fgcse-after-reload \
	-finline-functions \
	-fipa-cp-clone \
	-fpredictive-commoning \
	-funswitch-loops \
	-ftree-vectorize

ARCH_FLAGS = -march=pentium -mfpmath=387 -m32 \
	-DVERBOSITY=0 -O2 $(EXTRA_OPT_FLAGS)
INCLUDES = -I$(centroiding) \
	-I$(centroiding)/embbeded
CFLAGS += $(INCLUDES) $(ARCH_FLAGS) -fmessage-length=0 \
	-DFIRMWARE_MCU \
	-DNO_STDCPLUSCPLUS_LIB
CXXFLAGS += $(INCLUDES) $(ARCH_FLAGS) -fno-rtti -fno-exceptions \
	-DFIRMWARE_MCU \
	-DNO_STDCPLUSCPLUS_LIB
LDFLAGS += $(ARCH_FLAGS)
