# -*- makefile -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# SCP specific files build
#

CORE:=riscv-rv32i

# Required chip modules
chip-y+=$(CHIP_VARIANT)/cache.o
chip-y+=$(CHIP_VARIANT)/clock.o
chip-y+=$(CHIP_VARIANT)/gpio.o
chip-y+=$(CHIP_VARIANT)/intc.o
chip-y+=$(CHIP_VARIANT)/memmap.o
chip-y+=$(CHIP_VARIANT)/system.o
chip-y+=$(CHIP_VARIANT)/uart.o

ifeq ($(CONFIG_IPI),y)
$(out)/RW/chip/$(CHIP)/$(CHIP_VARIANT)/ipi_table.o: $(out)/ipi_table_gen.inc
endif

# Optional chip modules
chip-$(CONFIG_COMMON_TIMER)+=$(CHIP_VARIANT)/hrtimer.o
chip-$(CONFIG_IPI)+=$(CHIP_VARIANT)/ipi.o $(CHIP_VARIANT)/ipi_table.o
chip-$(CONFIG_WATCHDOG)+=$(CHIP_VARIANT)/watchdog.o
chip-$(HAS_TASK_HOSTCMD)+=$(CHIP_VARIANT)/hostcmd.o
