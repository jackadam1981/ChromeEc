# -*- makefile -*-
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Required chip modules
chip-y+=mt8195/uart.o
chip-y+=mt8195/clock_$(CHIP_VARIANT).o
chip-y+=mt8195/intc_group_$(CHIP_VARIANT).o

ifeq ($(CHIP_VARIANT), mt8195)
chip-y+=$(CHIP_VARIANT)/video.o
endif

