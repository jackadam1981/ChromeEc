# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

set(BOARD_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hayato)

set(PLATFORM_EC_BASEBOARD "${PLATFORM_EC}/baseboard/asurada" CACHE PATH
    "Path to the platform/ec baseboard directory")

zephyr_include_directories(include)

zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_POWERSEQ
  "${PLATFORM_EC_BASEBOARD}/power.c")
zephyr_library_sources_ifdef(CONFIG_AP_ARM_MTK_MT8192
  "${PLATFORM_EC}/power/mt8192.c")
