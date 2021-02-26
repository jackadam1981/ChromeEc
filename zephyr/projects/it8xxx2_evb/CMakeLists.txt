# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#zephyr_library()
#zephyr_library_sources(pinmux.c)

cmake_minimum_required(VERSION 3.13.1)
set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(BOARD it8xxx2_evb)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(it8xxx2_evb)

set(PLATFORM_EC_BOARD "${PLATFORM_EC}/board/it8xxx2_evb" CACHE PATH "Path to the platform/ec board directory")
set(PLATFORM_EC_BASEBOARD "${PLATFORM_EC}/baseboard/ite_evb" CACHE PATH "Path to the platform/ec baseboard directory")

# Include board specific header files
zephyr_include_directories(
  include
  ${PLATFORM_EC_BASEBOARD}
  ${PLATFORM_EC_BOARD})

# Include selected EC source for the baseboard
zephyr_sources_ifdef(CONFIG_PLATFORM_EC_USB_POWER_DELIVERY
  "${PLATFORM_EC_BASEBOARD}/usb_pd_policy.c")
