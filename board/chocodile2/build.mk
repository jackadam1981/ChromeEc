# -*- makefile -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

CHIP:=stm32
CHIP_FAMILY:=stm32g0
CHIP_VARIANT:=stm32g071b

board-y=board.o vpd_api.o demo.o
#
# This target builds RW only.  Therefore, remove RO from dependencies.
all_deps=$(patsubst ro,,$(def_all_deps))
