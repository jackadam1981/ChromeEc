# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ifeq ($(cc-name),gcc)
# coreboot sdk
TOOLCHAIN=riscv-elf
TOOLCHAIN_VERSION=11.3.0-r2
TOOLCHAIN_HASH=8888ce57a9f4b0b393715e50bd6feb6693fbc148

TOOLCHAIN_INSTALL_PATH=${TOOLCHAIN_INSTALL_DIR}/${TOOLCHAIN}/${TOOLCHAIN_VERSION}/${TOOLCHAIN_HASH}

ifdef CROSS_COMPILE_riscv
CROSS_COMPILE_arch:=riscv
endif

CROSS_COMPILE_RISC_DEFAULT:=${TOOLCHAIN_INSTALL_PATH}/bin/${TOOLCHAIN}-
else
CROSS_COMPILE_RISC_DEFAULT:=$(CROSS_COMPILE_riscv)
endif

# Select RISC-V bare-metal toolchain
$(call set-option,CROSS_COMPILE,$(CROSS_COMPILE_riscv),\
	$(CROSS_COMPILE_RISC_DEFAULT))
