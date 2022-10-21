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

if(NOT EXISTS "${PLATFORM_EC}/zephyr/module.yml")
  message(FATAL_ERROR "Failed to resolve PLATFORM_EC at
          ${PLATFORM_EC}/zephyr/module.yml")
endif()

include(zephyr/cmake/modules/extensions.cmake)

# Create an interface library for CrosEC (cros_ec_interface) that can be used
# as a dependency for individual CrosEC libraries. To use:
#  zephyr_library_named(my_lib)
#  target_link_libraries(my_lib PRIVATE cros_ec_interface)
add_library(cros_ec_interface INTERFACE)
target_include_directories_ifdef(CONFIG_PLATFORM_EC cros_ec_interface
  INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/fuzz"
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/include/driver"
    "${CMAKE_CURRENT_LIST_DIR}/test"
    "${CMAKE_CURRENT_LIST_DIR}/third_party"
    "${CMAKE_CURRENT_LIST_DIR}/zephyr/shim/include"
)

add_subdirectory(zephyr)

# Switch from the "zephyr" library to the "app" library for all Chromium OS
# sources.
set(ZEPHYR_CURRENT_LIBRARY app)
target_link_libraries(app PRIVATE cros_ec_interface)

add_subdirectory(builtin)
add_subdirectory(common)
