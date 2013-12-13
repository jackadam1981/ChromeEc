# -*- makefile -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# AVP processor specific files build
#

# AVP has an ARM7 core (ie ARMv4t ISA)
CORE:=armv4t

# Required chip modules
chip-y=clock.o gpio.o hwtimer.o jtag.o system.o uart.o

# Optional chip modules
chip-$(CONFIG_I2C)+=i2c.o
chip-$(CONFIG_WATCHDOG)+=watchdog.o
