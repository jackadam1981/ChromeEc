# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources_ifdef(CONFIG_CROS_FP_FPC1025	cros_fp_fpc1025.c)

if(DEFINED CONFIG_CROS_FP_FPC1025)
	zephyr_library_sources_ifdef(CONFIG_HAVE_FPC1025_PRIVATE_DRIVER
					fpc1025_pal_sensor.c)
	zephyr_library_sources_ifndef(CONFIG_HAVE_FPC1025_PRIVATE_DRIVER
					fpc1025_private_mock.c)
endif()
