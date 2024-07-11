# -*- makefile -*-
# Copyright 2016 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Minute-IA core build
#

# coreboot sdk
TOOLCHAIN=i386-elf
TOOLCHAIN_VERSION=11.3.0-r2
TOOLCHAIN_HASH=fb03ed1518de7dfd962f6e7cb95e1f7ef3e5e0f0

TOOLCHAIN_INSTALL_PATH=/opt/coreboot-sdk/${TOOLCHAIN}/${TOOLCHAIN_VERSION}/${TOOLCHAIN_HASH}
CROSS_COMPILE_X86_DEFAULT:=${TOOLCHAIN_INSTALL_PATH}/bin/${TOOLCHAIN}-
COREBOOT_SDK_URI_PATH=https://storage.googleapis.com/chromiumos-sdk/toolchains/coreboot-sdk
TARBALL=${TOOLCHAIN_HASH}.tar.zst
TOOLCHAIN_URI=${COREBOOT_SDK_URI_PATH}-${TOOLCHAIN}/${TOOLCHAIN_VERSION}/${TARBALL}

toolchain_status := $(shell	if [[ ! -d $(TOOLCHAIN_INSTALL_PATH) ]]; then \
		sudo mkdir -p $(TOOLCHAIN_INSTALL_PATH) && \
		sudo chmod 777 $(TOOLCHAIN_INSTALL_PATH) && \
		pushd $(TOOLCHAIN_INSTALL_PATH) && \
		curl -L -O $(TOOLCHAIN_URI) && \
		sudo tar -I pzstd --no-same-owner -xf "./$(TARBALL)" && \
		popd; \
	fi )

# Select Minute-IA bare-metal toolchain
$(call set-option,CROSS_COMPILE,$(CROSS_COMPILE_i386),\
	$(CROSS_COMPILE_X86_DEFAULT))

# FPU compilation flags
CFLAGS_FPU-$(CONFIG_FPU)=

# CPU specific compilation flags
CFLAGS_CPU+=-O2 -fomit-frame-pointer -mno-accumulate-outgoing-args	\
	    -ffunction-sections -fdata-sections				\
	    -fno-builtin-printf -fno-builtin-sprintf			\
	    -fno-stack-protector -gdwarf-2  -fno-common -ffreestanding	\
	    -minline-all-stringops -fno-strict-aliasing

CFLAGS_CPU+=$(CFLAGS_FPU-y)

ifneq ($(CONFIG_LTO),)
CFLAGS_CPU+=-flto
LDFLAGS_EXTRA+=-flto
endif

core-y=cpu.o init.o interrupts.o
core-$(CONFIG_COMMON_PANIC_OUTPUT)+=panic.o
core-$(CONFIG_COMMON_RUNTIME)+=switch.o task.o
core-$(CONFIG_MPU)+=mpu.o

# for 64bit division
LDFLAGS_EXTRA+=-static-libgcc -lgcc
