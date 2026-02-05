# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.5)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

if(DEFINED CONFIG_BOARD_ATRIARVP_RTK)
	project(atriarvp-rtk)
endif()

zephyr_include_directories("include/")

# FIXME: Check directory structure
zephyr_include_directories_ifdef(CONFIG_PLATFORM_EC_DEDICATED_CHARGE_PORT
  "../intelrvp/include")
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_DEDICATED_CHARGE_PORT
  "../intelrvp/src/dc_jack.c")

# FIXME: Save for later commit?
if(DEFINED CONFIG_BOARD_INTEL_ISH_5_8_0)
	project(ocelot-ish)
endif()
