# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.5)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

if(DEFINED CONFIG_BOARD_OCELOTRVP_ITE)
	project(ocelotrvp-ite)
	zephyr_library_sources("src/rvp_keyboard_customization.c")
endif()

if(DEFINED CONFIG_BOARD_OCELOTRVP_NPCX)
	project(ocelotrvp-npcx)
	zephyr_library_sources("src/rvp_keyboard_customization.c")
endif()

if(DEFINED CONFIG_BOARD_OCELOTRVP_MCHP)
	project(ocelotrvp-mchp)
	zephyr_library_sources("src/rvp_board_power.c")
endif()

zephyr_include_directories("include/")

add_subdirectory(${PROJECT_NAME})
