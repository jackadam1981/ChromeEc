# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.5)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

zephyr_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../intelrvp/include")

if(DEFINED CONFIG_BOARD_OCELOT_ITE)
	project(ocelot_ite)
endif()

if(DEFINED CONFIG_BOARD_OCELOT_NUVOTON)
	project(ocelot_nuvoton)
endif()

if(DEFINED CONFIG_BOARD_OCELOT_MICROCHIP)
	project(ocelot_microchip)
	zephyr_library_sources("../intelrvp/src/intelrvp.c")
	zephyr_library_sources("../intelrvp/src/intel_rvp_board_id.c")
	zephyr_library_sources("src/rvp_board_id.c")
	zephyr_library_sources("src/rvp_board_power.c")
endif()

add_subdirectory(${PROJECT_NAME})
