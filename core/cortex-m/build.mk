# -*- makefile -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Cortex-M4 core OS files build
#

# Select ARMv7-m bare-metal toolchain
$(call set-option,CROSS_COMPILE,$(CROSS_COMPILE_arm),arm-none-eabi-)

# FPU compilation flags
CFLAGS_FPU-$(CONFIG_FPU)=-mfpu=fpv4-sp-d16 -mfloat-abi=hard

# CPU specific compilation flags
CFLAGS_CPU+=-mthumb -Os
ifneq ($(cc-name),clang)
CFLAGS_CPU+=-mno-sched-prolog
endif
CFLAGS_CPU+=-mno-unaligned-access
CFLAGS_CPU+=$(CFLAGS_FPU-y)

ifneq ($(CONFIG_LTO),)
CFLAGS_CPU+=-flto
LDFLAGS_EXTRA+=-flto
endif

ifeq ($(cc-name),clang)
LDFLAGS_EXTRA+=-Wl,--noinhibit-exec
endif

$(out)/RO/core/cortex-m/vecttable.o: CFLAGS+=-Wno-initializer-overrides
$(out)/RW/core/cortex-m/vecttable.o: CFLAGS+=-Wno-initializer-overrides

# clang warns:
#
#     inline asm clobber list contains reserved registers: R7
#
# but this is ok because we are actually setting the clobber list to
# avoid that register from being clobbered. We don't actually clobber it in
# the assembly.
$(out)/RO/core/cortex-m/panic.o: CFLAGS+=-Wno-inline-asm
$(out)/RW/core/cortex-m/panic.o: CFLAGS+=-Wno-inline-asm

core-y=cpu.o init.o ldivmod.o llsr.o uldivmod.o vecttable.o
core-$(CONFIG_AES)+=aes.o
core-$(CONFIG_AES_GCM)+=ghash.o
core-$(CONFIG_ARMV7M_CACHE)+=cache.o
core-$(CONFIG_COMMON_PANIC_OUTPUT)+=panic.o
core-$(CONFIG_COMMON_RUNTIME)+=switch.o task.o
core-$(CONFIG_WATCHDOG)+=watchdog.o
core-$(CONFIG_MPU)+=mpu.o
