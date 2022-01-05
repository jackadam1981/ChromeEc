# -*- makefile -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# SCP specific files build
#

# Required chip modules
chip-y=

ifeq ($(CHIP_VARIANT),$(filter $(CHIP_VARIANT),mt8183 mt8186))
CPPFLAGS+=-Ichip/$(CHIP)/mt818x
dirs-y+=chip/$(CHIP)/mt818x
# Each chip variant can provide specific build.mk if any
include chip/$(CHIP)/mt818x/build.mk
endif

ifeq ($(CHIP_VARIANT),$(filter $(CHIP_VARIANT),mt8192 mt8195))
CPPFLAGS+=-Ichip/$(CHIP)/rv32i_common
dirs-y+=chip/$(CHIP)/rv32i_common
include chip/$(CHIP)/rv32i_common/build.mk
endif
