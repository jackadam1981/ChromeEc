# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ifeq ($(cc-name),gcc)
# coreboot sdk
TOOLCHAIN=i386-elf
TOOLCHAIN_VERSION=11.3.0-r2
TOOLCHAIN_HASH=fb03ed1518de7dfd962f6e7cb95e1f7ef3e5e0f0

TOOLCHAIN_INSTALL_PATH=${TOOLCHAIN_INSTALL_DIR}/${TOOLCHAIN}/${TOOLCHAIN_VERSION}/${TOOLCHAIN_HASH}

ifndef CROSS_COMPILE_i386
$(shell ./sdk_extractor.py --install-path ${TOOLCHAIN_INSTALL_DIR} --toolchain ${TOOLCHAIN}:${TOOLCHAIN_VERSION}:${TOOLCHAIN_HASH})
endif

CROSS_COMPILE_X86_DEFAULT:=${TOOLCHAIN_INSTALL_PATH}/bin/${TOOLCHAIN}-
else
# llvm sdk
CROSS_COMPILE_X86_DEFAULT:=
endif

$(call set-option,CROSS_COMPILE,\
	$(CROSS_COMPILE_x86),\
	$(CROSS_COMPILE_X86_DEFAULT))
