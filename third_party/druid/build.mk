
# Copyright 2012 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include private/fingerprint/druid/build.mk

# If CONFIG_LIB_EIGEN3 is enabled, include the header-only library eigen3,
# which resides outside of ec root directory.
includes-$(if $(CONFIG_LIB_EIGEN3),y) += ../../third_party/eigen3

$(out)/RO/third_party/druid/%.o : ../fingerprint/druid/%.cc
	$(call quiet,cxx_to_o,CXX    )
$(out)/RW/third_party/druid/%.o : ../fingerprint/druid/%.cc
	$(call quiet,cxx_to_o,CXX    )

$(eval $(call vars_from_dir,third-party,third_party,druid))
includes-y += $(addprefix ../fingerprint/druid/,$(druid-incs))
