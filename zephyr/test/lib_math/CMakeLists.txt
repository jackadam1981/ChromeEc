# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.24.0)
find_package(Zephyr REQUIRED HINTS "$ENV{ZEPHYR_BASE}")
project(cros.dsp)
target_sources(app PRIVATE ${ZEPHYR_BASE}/misc/empty_file.c)

if("${TEST_LIB}" STREQUAL "")
  message(FATAL_ERROR "TEST_LIB must be set")
endif()
target_link_libraries(app
  PRIVATE
    pw_unit_test.simple_printing_main
    ${TEST_LIB}.lib
)
