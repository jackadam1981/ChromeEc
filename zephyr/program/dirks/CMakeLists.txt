# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

zephyr_include_directories(include)
zephyr_library_sources("src/common.c")
zephyr_library_sources_ifdef(CONFIG_DIRKS_SUB_BOARD "src/sub_board.c")
zephyr_library_sources_ifdef(CONFIG_AP_PWRSEQ "src/board_power.c")

if(DEFINED CONFIG_BOARD_DIRKS)
	project(dirks)
	zephyr_include_directories("dirks/include")
	zephyr_library_sources(
			"src/led.c"
			"dirks/src/hdmi.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "dirks/src/usbc.c")
endif()
