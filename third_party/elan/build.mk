# -*- makefile -*-
# vim: set filetype=make :
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Add optional Elan private libraries.

third-party-elan-cur-dir := $(dir $(lastword $(MAKEFILE_LIST)))

third-party-elan-private-dir := ../fingerprint/elan

p-elan-lib-name-$(and $(CONFIG_FP_SENSOR_ELAN80),$(CHIP_FAMILY_STM32F4),rw) \
	= elan_80_m4
# This has only been tested on the NPCX9, but is compatible with Cortex M4 MCUs.
p-elan-lib-name-$(and $(CONFIG_FP_SENSOR_ELAN80SG),$(CORE_CORTEX_M),rw) \
	= elan_80sg_m4
p-elan-lib-name-$(and $(CONFIG_FP_SENSOR_ELAN80),$(CHIP_FAMILY_STM32H7),rw) \
	= elan_80_m7
p-elan-lib-name-$(and $(CONFIG_FP_SENSOR_ELAN515),$(CHIP_FAMILY_STM32F4),rw) \
	= elan_515_m4
p-elan-lib-name-$(and $(CONFIG_FP_SENSOR_ELAN515),$(CHIP_FAMILY_STM32H7),rw) \
	= elan_515_m7

p-elan-staticlib = lib$(p-elan-lib-name-rw).a

ifneq ($(wildcard $(third-party-elan-private-dir)/$(p-elan-staticlib)),)

include $(third-party-elan-private-dir)/build.mk

p-elan-rw-out = $(out)/RW/$(third-party-elan-cur-dir)
$(out)/RW/$(PROJECT).RW.elf: LDFLAGS_EXTRA += -L$(p-elan-rw-out)
$(out)/RW/$(PROJECT).RW.elf: LDFLAGS_EXTRA += -l$(p-elan-lib-name-rw)
$(out)/RW/$(PROJECT).RW.elf: $(out)/RW/$(third-party-elan-cur-dir)$(p-elan-staticlib)

endif
