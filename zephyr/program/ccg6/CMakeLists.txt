# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.5)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(ccg6)

zephyr_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
zephyr_library_sources_ifdef(CONFIG_AP_PWRSEQ "board_power.c")
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "usb_pd_policy.c")
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "board.c")
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "chg_usb_pd.c")
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "chg_usb_pd_mecc_1_1.c")
