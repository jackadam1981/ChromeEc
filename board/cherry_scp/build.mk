# -*- makefile -*-
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build
#

CHIP:=mt_scp
ifeq ($(BOARD), cherry_scp_core1)
CHIP_VARIANT:=mt8195_core1
board-y+=ipi_test.o
board-y+=dram_test.o
else
CHIP_VARIANT:=mt8195
endif
BASEBOARD:=mtscp-rv32i
