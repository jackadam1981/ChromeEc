# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Set coreboot-sdk as the default toolchain for nds32
TOOLCHAIN=nds32le-elf
TOOLCHAIN_VERSION=11.3.0-r2
TOOLCHAIN_HASH=c41e5fb8d49f77a8a6db8b12555585135e9fbd8f

TOOLCHAIN_PATH=${TOOLCHAIN}/${TOOLCHAIN_VERSION}/${TOOLCHAIN_HASH}
TOOLCHAIN_INSTALL_PATH=${TOOLCHAIN_INSTALL_DIR}${TOOLCHAIN_PATH}

ifndef CROSS_COMPILE_nds32
CROSS_COMPILE_FW_TOOLCHAIN:=nds32
endif

NDS32_DEFAULT_COMPILE:=${TOOLCHAIN_INSTALL_PATH}/bin/${TOOLCHAIN}-

# Select Andes bare-metal toolchain
$(call set-option,CROSS_COMPILE,$(CROSS_COMPILE_nds32),$(NDS32_DEFAULT_COMPILE))
