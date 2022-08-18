# -*- makefile -*-
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Required chip modules
chip-y+=$(CHIP_VARIANT)/clock.o
chip-y+=mt8188/clock_$(CHIP_VARIANT).o
chip-y+=mt8188/intc_group.o
chip-y+=mt8188/uart.o

ifeq ($(CHIP_VARIANT), mt8188)
chip-y+=$(CHIP_VARIANT)/video.o
endif

ifeq ($(CHIP_VARIANT), mt8188_core1)
chip-y+=mt8188/ipi_ops.o
endif
