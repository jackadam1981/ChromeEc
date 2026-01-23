# Copyright 2026 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#zephyr_library_sources_ifdef(CONFIG_SOC_FAMILY_INTEL_ISH i2c_scan.c)
zephyr_library_sources(i2c_scan.c)
