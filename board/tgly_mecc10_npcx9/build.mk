# -*- makefile -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Intel TGL-Y-RVP + Nuvoton MECC board with NPCX9
#

CHIP:=npcx
CHIP_FAMILY:=npcx9
CHIP_VARIANT:=npcx9m3f
BASEBOARD:=intelrvp

board-y=board.o
board-$(CONFIG_BATTERY_SMART)+=battery.o
