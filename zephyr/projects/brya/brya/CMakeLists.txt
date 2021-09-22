# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(brya)

set(PLATFORM_EC_BOARD "${PLATFORM_EC}/board/brya" CACHE PATH
    "Path to the platform/ec board directory")
set(PLATFORM_EC_BASEBOARD "${PLATFORM_EC}/baseboard/brya" CACHE PATH
    "Path to the platform/ec baseboard directory")
set(PLATFORM_EC_POWER "${PLATFORM_EC}/power" CACHE PATH
    "Path to the platform/ec power directory")

zephyr_include_directories(include
        "${PLATFORM_EC_BASEBOARD}"
        "${PLATFORM_EC_BOARD}"
        "${PLATFORM_EC_POWER}")

# Include selected EC source for the board
zephyr_library_sources_ifdef(CONFIG_CHIPSET_ALDERLAKE_SLG4BD44540
  "${PLATFORM_EC_POWER}/alderlake_slg4bd44540.c")
