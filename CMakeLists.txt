# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Note: this cmake system implements only a zephyr module, and is not
# intended to build a complete EC.  To build projects in platform/ec,
# you should continue to use the Makefile system.
#
# Googlers can find the design doc at go/zephyr-shim.

if(NOT DEFINED ZEPHYR_CURRENT_MODULE_DIR)
  message(FATAL_ERROR "This Cmake system implements only a Zephyr module, and
          should not be invoked directly.  Please continue to use the Makefile
          system for non-Zephyr builds.")
endif()

zephyr_include_directories(
  "${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/shim/include"
  "${ZEPHYR_CURRENT_MODULE_DIR}/fuzz"
  "${ZEPHYR_CURRENT_MODULE_DIR}/test"
  "${ZEPHYR_CURRENT_MODULE_DIR}/include")

add_subdirectory_ifdef(CONFIG_PLATFORM_EC "common")
add_subdirectory_ifdef(CONFIG_PLATFORM_EC "zephyr/shim/src")
