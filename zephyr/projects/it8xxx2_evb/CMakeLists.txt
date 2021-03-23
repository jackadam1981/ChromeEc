# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(it8xxx2_evb)

#set(PLATFORM_EC_BOARD "${PLATFORM_EC}/board/it8xxx2_evb" CACHE PATH "Path to the #platform/ec board directory")
#set(PLATFORM_EC_BASEBOARD "${PLATFORM_EC}/baseboard/ite_evb" CACHE PATH "Path to #the platform/ec baseboard directory")

# Include board specific header files
#zephyr_include_directories(
#  include
#  ${PLATFORM_EC_BASEBOARD}
#  ${PLATFORM_EC_BOARD})

# Include board specific header files
zephyr_include_directories(include)

# Include selected EC source for the board
#zephyr_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_BOARD_CONFIG
#  "${PLATFORM_EC_BOARD}/keyboard.c")
# needn't, we use common setting keyscan_config in keyboard_scan.c
