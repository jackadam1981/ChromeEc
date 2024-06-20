# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/intelrvp/include")


# Add sources for board_power.c functionality and related platform-specific files

target_sources_ifdef(CONFIG_TEST_PROJECT_BOARD_POWER app PRIVATE
  ${PLATFORM_EC_PROGRAM_DIR}/intelrvp/mtlrvp/src/board_power.c
)
target_sources_ifdef(CONFIG_TEST_PROJECT_BOARD_POWER app PRIVATE
  mtlrvp/src/board_power.c
)
