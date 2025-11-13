# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cros_ec_library_include_directories(include)

zephyr_library_sources_ifdef(CONFIG_EGIS_ET171_OTP_KEY otp_key.c)

if(CONFIG_EGIS_ET171_OTP_KEY)
	zephyr_library_link_libraries(ET171HAL)
endif()
