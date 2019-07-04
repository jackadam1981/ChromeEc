# -*- makefile -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# ISH chip specific files build
#

# ISH SoC has a Minute-IA core
CORE:=minute-ia
# Allow the i486 instruction set
CFLAGS_CPU+=-march=pentium -mtune=i486 -m32

ifeq ($(CONFIG_LTO),y)
# Re-include the core's build.mk file so we can remove the lto flag.
include core/$(CORE)/build.mk
endif

# Required chip modules
chip-y+=clock.o gpio.o system.o hwtimer.o uart.o flash.o ish_persistent_data.o
chip-$(CONFIG_I2C)+=i2c.o
chip-$(CONFIG_WATCHDOG)+=watchdog.o
chip-$(CONFIG_HOSTCMD_HECI)+=host_command_heci.o
chip-$(CONFIG_HOSTCMD_HECI)+=heci.o system_state_subsys.o ipc_heci.o
chip-$(CONFIG_HID_HECI)+=hid_subsys.o
chip-$(CONFIG_HID_HECI)+=heci.o system_state_subsys.o ipc_heci.o
chip-$(CONFIG_DMA_PAGING)+=dma.o
chip-$(CONFIG_LOW_POWER_IDLE)+=power_mgt.o

# There is no framework for on-board tests in ISH. Do not specify any.
test-list-y=

ifeq ($(CONFIG_ISH_PM_AONTASK),y)

ish-aon-fw=ish_aontask
ish-aon-fw-out=$(out)/aontaskfw
ish-aon-fw-elf=$(ish-aon-fw-out)/$(ish-aon-fw).elf
ish-aon-fw-map=$(ish-aon-fw-out)/$(ish-aon-fw).map
ish-aon-fw-bin=$(ish-aon-fw-out)/$(ish-aon-fw).bin
ish-aon-fw-lds=$(ish-aon-fw-out)/$(ish-aon-fw).ld
ish-aon-fw-lds-templet=chip/ish/aontaskfw/ish_aontask.ld.in
ish-aon-fw-srcs=chip/ish/aontaskfw/ish_aontask.c chip/ish/dma.c
ish-aon-fw-objs=$(foreach o,$(subst .c,.o,$(ish-aon-fw-srcs)),$(ish-aon-fw-out)/$(o))
ish-aon-fw-deps=$(addsuffix .d, $(ish-aon-fw-objs))

PROJECT_EXTRA+=$(ish-aon-fw-bin)
deps += $(ish-aon-fw-deps)

_aon_size_str=$(shell stat -L -c %s $(ish-aon-fw-bin))
_aon_size=$(shell echo "$$(($(_aon_size_str)))")

$(out)/$(PROJECT).bin: $(ish-aon-fw-bin) $(out)/RW/$(PROJECT).RW.flat

# rules for building ISH aon task fw
$(ish-aon-fw-bin): $(ish-aon-fw-lds) $(ish-aon-fw-objs)
	$(if $(V),,@echo '  EXTBIN ' $(subst $(out)/,,$@) ; )
	-@ $(CC) $(ish-aon-fw-objs)   $(LDFLAGS)                 \
		-o $(ish-aon-fw-elf) -Wl,-T,$(ish-aon-fw-lds)    \
		-Wl,-Map,$(ish-aon-fw-map)
	-@ $(OBJCOPY) -O binary $(ish-aon-fw-elf)  $@

$(ish-aon-fw-lds): $(ish-aon-fw-lds-templet)
	-@ mkdir -p $(@D)
	@ $(CC) $(CFLAGS) -x assembler-with-cpp -E -P $< -o $@

$(ish-aon-fw-out)/%.o: %.c
	-@ mkdir -p $(@D)
	$(call quiet,c_to_o,CC)

endif

_kernel_size_str=$(shell stat -L -c %s $(out)/RW/$(PROJECT).RW.flat)
_kernel_size=$(shell echo "$$(($(_kernel_size_str)))")

# location of the scripts used to pack image
SCRIPTDIR:=./chip/${CHIP}/util


# Commands to convert ec.RW.flat to $@.tmp - This will add the manifest header
# needed to load the FW onto the ISH HW.

ifeq ($(CONFIG_ISH_PM_AONTASK),y)
cmd_obj_to_bin = ${SCRIPTDIR}/pack_ec.py -o $@.tmp \
		 -k $(out)/RW/$(PROJECT).RW.flat \
		 --kernel-size $(_kernel_size) \
		 -a $(ish-aon-fw-bin)  \
		 --aon-size $(_aon_size);
else
cmd_obj_to_bin = ${SCRIPTDIR}/pack_ec.py -o $@.tmp \
		 -k $(out)/RW/$(PROJECT).RW.flat \
		 --kernel-size $(_kernel_size);
endif
