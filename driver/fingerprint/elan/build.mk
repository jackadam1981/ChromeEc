# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build for ELAN fingerprint drivers

# Note that this variable includes the trailing "/"
_elan_cur_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

# Make sure output directory is created (in build directory)
dirs-y+="$(_elan_cur_dir)"
ifneq (,$(filter rw,$(CONFIG_FP_SENSOR_ELAN80) $(CONFIG_FP_SENSOR_ELAN515)))
all-obj-rw+=$(_elan_cur_dir)elan_private.o
all-obj-rw+=$(_elan_cur_dir)elan_sensor_pal.o

ifeq ($(CONFIG_FP_SENSOR_ELAN80),rw)
SENSOR=elan_80
ELANLIB=lib$(SENSOR).a
else
ifeq ($(CONFIG_FP_SENSOR_ELAN515),rw)
SENSOR=elan_515
ELANLIB=lib$(SENSOR).a
endif
endif

ifneq ("$(wildcard $(_elan_cur_dir)$(ELANLIB))","")
FPPATH:=$(out)/libsharedobjs/elan
$(out)/RW/ec.RW.elf: LDFLAGS_EXTRA += -L$(FPPATH) -l$(SENSOR)
$(out)/RW/ec.RW.elf: $(FPPATH)/$(ELANLIB)

$(FPPATH)/$(ELANLIB): $(_elan_cur_dir)$(ELANLIB)
	@mkdir -p $(FPPATH)
	@cp -f $^ $@
endif
endif
