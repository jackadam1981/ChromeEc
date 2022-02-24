# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(intelrvp)

zephyr_include_directories(include)

if(DEFINED CONFIG_BOARD_ADLRVP_NPCX)
  add_subdirectory(adlrvp)
  zephyr_include_directories("${PLATFORM_EC}/driver/charger")
  zephyr_include_directories("${PLATFORM_EC}/driver/ppc")
  zephyr_include_directories("${PLATFORM_EC}/driver/tcpm")
  zephyr_include_directories("${PLATFORM_EC}/driver/usb_mux")
  zephyr_include_directories("adlrvp/include")
  zephyr_library_sources("src/intelrvp.c")
  zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "adlrvp/adlrvp_npcx/src/usbc.c")
  zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "${PLATFORM_EC}/baseboard/intelrvp/usb_pd_policy_mecc_1_0.c")
  zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "${PLATFORM_EC}/baseboard/intelrvp/chg_usb_pd_mecc_1_0.c")
  zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "${PLATFORM_EC}/baseboard/intelrvp/chg_usb_pd.c")
endif()

if(DEFINED CONFIG_PLATFORM_EC_IOEX_CROS_DRV)
	zephyr_include_directories("${PLATFORM_EC}/driver/ioexpander")
endif()
