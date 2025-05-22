# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../program/intelrvp/include")

zephyr_library_sources_ifdef(CONFIG_CROS_EC_INTEL_RVP_BOARD_ID board_id_intel_rvp.c)
