# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ifeq ($(cc-name),gcc)
# coreboot sdk
TOOLCHAIN=arm-eabi
TOOLCHAIN_VERSION=11.3.0-r2
TOOLCHAIN_HASH=ee6ffc5a0f85d11c4a9f1c88f7d0cbd1e142a19e

TOOLCHAIN_INSTALL_PATH=${TOOLCHAIN_INSTALL_DIR}/${TOOLCHAIN}

ifdef CROSS_COMPILE_arm
CROSS_COMPILE_arch:=arm
endif

CROSS_COMPILE_ARM_DEFAULT:=${TOOLCHAIN_INSTALL_PATH}/bin/${TOOLCHAIN}-
else
# llvm sdk
CROSS_COMPILE_ARM_DEFAULT:=armv7m-cros-eabi-
endif
CMAKE_SYSTEM_PROCESSOR ?= armv7
# TODO(b/275450331): Enable the asm after we fix the crash.
OPENSSL_NO_ASM ?= 1

$(call set-option,CROSS_COMPILE,\
	$(CROSS_COMPILE_arm),\
	$(CROSS_COMPILE_ARM_DEFAULT))
