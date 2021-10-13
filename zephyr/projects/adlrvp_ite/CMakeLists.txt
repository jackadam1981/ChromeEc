# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(adlrvp_ite)

set(PLATFORM_EC_BASEBOARD "${PLATFORM_EC}/baseboard/intelrvp" CACHE PATH
    "Path to the platform/ec baseboard directory")

# Include board specific header files
zephyr_include_directories(
  include
  "${PLATFORM_EC}/include/power"
  "${PLATFORM_EC_BASEBOARD}")

# Include selected EC source from the baseboard
zephyr_library_sources(
  "${PLATFORM_EC_BASEBOARD}/power.c")
