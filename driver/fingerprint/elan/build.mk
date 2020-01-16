# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build for ELAN fingerprint drivers

# Note that this variable includes the trailing "/"
_elan_cur_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

# Make sure output directory is created (in build directory)
dirs-y+="$(_elan_cur_dir)"
ifeq ($(CONFIG_FP_SENSOR_ELAN),rw)
all-obj-rw+= $(_elan_cur_dir)elan_private.o

ifneq ("$(wildcard $(_elan_cur_dir)libelan.a)","")
FPPATH:=$(out)/libsharedobjs/elan
$(out)/RW/ec.RW.elf: LDFLAGS_EXTRA += -L$(FPPATH) -lelan
$(out)/RW/ec.RW.elf: $(FPPATH)/libelan.a

$(FPPATH)/libelan.a: $(_elan_cur_dir)libelan.a
	@mkdir $(FPPATH)
	@cp $^ $@
endif
endif
