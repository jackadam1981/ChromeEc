# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(motion_sense)

target_sources(app
  PRIVATE
    src/data_rate.cc
    src/main.cc
    src/offset.cc
    src/range.cc
    src/read.cc
    src/resolution.cc
    src/scale.cc
)

target_include_directories(app PRIVATE include)

target_link_libraries(app
  PRIVATE
    pw_log
    pw_unit_test.light
    pw_unit_test.logging_event_handler
)

