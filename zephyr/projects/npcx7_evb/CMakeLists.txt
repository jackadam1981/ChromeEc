# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(BOARD volteer)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(npcx7_evb)

set(PLATFORM_EC_BOARD "${PLATFORM_EC}/board/volteer" CACHE PATH
    "Path to the platform/ec board directory")

# Include board specific header files
zephyr_include_directories(
  include
  ${PLATFORM_EC_BOARD})
