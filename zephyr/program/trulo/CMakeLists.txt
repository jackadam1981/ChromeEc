# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
cmake_minimum_required(VERSION 3.20.1)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

zephyr_library_sources_ifdef(CONFIG_AP_PWRSEQ "src/board_power.c")

if(DEFINED CONFIG_BOARD_TRULO)
	project(trulo)
	add_subdirectory(trulo)
	zephyr_include_directories("trulo/include")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
	"trulo/src/keyboard.c"
	"trulo/src/keyboard_customization.c"
	"trulo/src/touch_panel.c"
	)
endif()
