# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

zephyr_library_named(isl923x)

zephyr_include_directories(
	"${PLATFORM_EC}/zephyr/emul/include"
	"${PLATFORM_EC}/zephyr/shim/include"
	"${PLATFORM_EC}/zephyr/include"
	"${PLATFORM_EC}/fuzz"
	"${PLATFORM_EC}/test"
	"${PLATFORM_EC}"
	"${PLATFORM_EC}/include"
	"${PLATFORM_EC}/include/driver"
	"${PLATFORM_EC}/third_party")

zephyr_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
zephyr_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CONSOLE_CMD_CHARGER_ADC_AMON_BMON
			"${CMAKE_CURRENT_SOURCE_DIR}/src/console_cmd_amon_bmon.c")