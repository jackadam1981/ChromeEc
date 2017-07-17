# -*- makefile -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# LM3S811 chip specific files build
#

# LM3S811 SoC has a Cortex-M3 ARM core
CORE:=cortex-m
# Allow the full Cortex-M4 instruction set
CFLAGS_CPU+=-march=armv7-m -mcpu=cortex-m3

# Required chip modules
chip-y=clock.o gpio.o hwtimer.o jtag.o system.o uart.o

# Optional chip modules
chip-$(CONFIG_ADC)+=adc.o
chip-$(CONFIG_I2C)+=i2c.o
