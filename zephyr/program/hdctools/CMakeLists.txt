# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.5)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

zephyr_include_directories("${PLATFORM_EC}/include")

zephyr_library_sources("${PLATFORM_EC}/zephyr/shim/src/libgcc_arm.S")

zephyr_include_directories(include)

zephyr_library_sources_ifdef(CONFIG_DFU_BOOTLOADER
	src/dfu_stm32.c
	src/dfu_usb.c)

zephyr_library_sources(src/console_util.c)
zephyr_library_sources(src/info_cmd.c)
zephyr_library_sources(src/reboot_cmd.c)

if(DEFINED CONFIG_BOARD_STARFISH_V1)
	project(starfish_v1)
	add_subdirectory(starfish_v1)
endif()


