# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.0)

include_guard(GLOBAL)

if(BOARD STREQUAL unit_testing)
  find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})
  get_filename_component(PLATFORM_EC "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)
  include(${CMAKE_CURRENT_LIST_DIR}/../../../zephyr/cmake/modules/extensions.cmake)
endif()
