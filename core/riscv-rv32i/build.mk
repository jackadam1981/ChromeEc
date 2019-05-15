# -*- makefile -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# RISC-V core OS files build
#

# TODO: Toolchain need to support 32-bit architecture or we will
# get the following error message:
# ABI is incompatible with that of the selected emulation:
# target emulation `elf64-littleriscv' does not match `elf32-littleriscv'
# Select RISC-V bare-metal toolchain
$(call set-option,CROSS_COMPILE,$(CROSS_COMPILE_riscv),\
	/opt/riscv/riscv/bin/riscv32-unknown-elf-)

# CPU specific compilation flags
# TODO: fix the rodata section shift 2-bytes issue on binary.
# CFLAGS_CPU+=-march=rv32imafc -mabi=ilp32f -Os
CFLAGS_CPU+=-march=rv32imaf -mabi=ilp32f -Os
LDFLAGS_EXTRA+=-mrelax

ifneq ($(CONFIG_LTO),)
CFLAGS_CPU+=-flto
LDFLAGS_EXTRA+=-flto
endif

core-y=cpu.o init.o panic.o task.o switch.o __builtin.o
