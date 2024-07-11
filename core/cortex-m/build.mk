# -*- makefile -*-
# Copyright 2012 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Cortex-M4 core OS files build
#

# coreboot sdk
TOOLCHAIN=arm-eabi
TOOLCHAIN_VERSION=11.3.0-r2
TOOLCHAIN_HASH=ee6ffc5a0f85d11c4a9f1c88f7d0cbd1e142a19e

TOOLCHAIN_INSTALL_PATH=${TOOLCHAIN_INSTALL_DIR}/${TOOLCHAIN}

ifdef CROSS_COMPILE_arm
CROSS_COMPILE_arch:=arm
endif

CROSS_COMPILE_ARM_DEFAULT:=${TOOLCHAIN_INSTALL_PATH}/bin/${TOOLCHAIN}-
TOOLCHAIN=arm-eabi
TOOLCHAIN_VERSION=11.3.0-r2
TOOLCHAIN_HASH=ee6ffc5a0f85d11c4a9f1c88f7d0cbd1e142a19e

# Use coreboot-sdk
$(call set-option,CROSS_COMPILE,\
	$(CROSS_COMPILE_arm),\
	$(CROSS_COMPILE_ARM_DEFAULT))
# Force gcc compiler
cc-name:=gcc
# FPU compilation flags
CFLAGS_FPU-$(CONFIG_FPU)=-mfpu=fpv4-sp-d16 -mfloat-abi=hard

# CPU specific compilation flags
CFLAGS_CPU+=-mthumb -Os -mno-sched-prolog
CFLAGS_CPU+=-mno-unaligned-access
CFLAGS_CPU+=$(CFLAGS_FPU-y)

ifneq ($(CONFIG_LTO),)
CFLAGS_CPU+=-flto
LDFLAGS_EXTRA+=-flto
endif

# gcc 11.2 had a known issue which doesn't affect Cr50 build anymore
# but we can't remove -fno-ipa-modref as it changes FIPS module digest
GCC_VERSION := $(shell $(CROSS_COMPILE)$(cc-name) -dumpversion)
ifeq ("$(GCC_VERSION)","11.2.0")
# IPA modref pass crashes gcc 11.2 when LTO is used with partial linking
CFLAGS_CPU += -fno-ipa-modref

# -ffat-lto-objects is a workaround for b/134623681
# Disable assembler warnings (gcc 11.2 came with v2.35 binutils with issues)
# TODO (b/238039591) Remove `-Wa,W` when binutils is fixed.
CFLAGS_CPU += -Wa,-W -ffat-lto-objects
endif

ifeq ("$(GCC_VERSION)","11.3.0")
# IPA modref pass crashes gcc 11.3 when LTO is used with partial linking
CFLAGS_CPU += -fno-ipa-modref

# -ffat-lto-objects is a workaround for b/134623681
CFLAGS_CPU += -ffat-lto-objects
endif

# Set an option to force LTO to generate target machine code.
# `-flinker-output=nolto-rel` first became available in gcc 9.x
CFLAGS_LTO_PARTIAL_LINK := -flinker-output=nolto-rel

core-y=cpu.o init.o ldivmod.o llsr.o uldivmod.o vecttable.o
core-$(CONFIG_AES)+=aes.o
core-$(CONFIG_AES_GCM)+=ghash.o
core-$(CONFIG_COMMON_PANIC_OUTPUT)+=panic.o
core-$(CONFIG_COMMON_RUNTIME)+=switch.o task.o
core-$(CONFIG_WATCHDOG)+=watchdog.o
