# -*- makefile -*-
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build
#

CHIP:=mt_scp
ifeq ($(BOARD), geralt_scp_core1)
CHIP_VARIANT:=mt8188_core1
else
CHIP_VARIANT:=mt8188
endif
CHIP_FAMILY:=RV55
BASEBOARD:=mtscp-rv32i
board-y+=dram_test.o
