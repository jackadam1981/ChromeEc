# -*- makefile -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Norrin board (in EC-less configuration) specific files build
#

# the IC is the AVP core of the T124 SoC
CHIP:=avp

board-y=board.o
