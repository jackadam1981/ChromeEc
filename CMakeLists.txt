# Copyright 2022 The ChromiumOS Authors
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

set(PLATFORM_EC "${ZEPHYR_CURRENT_MODULE_DIR}" CACHE PATH
    "Path to the platform/ec repo.")

assert_exists("${PLATFORM_EC}/zephyr/module.yml")

add_subdirectory(zephyr)
