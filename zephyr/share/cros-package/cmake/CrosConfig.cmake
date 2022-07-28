# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../.." real_platform_ec)
set(PLATFORM_EC "${real_platform_ec}" CACHE PATH "Path to the platform/ec repo")

# Use the new ztest API
set(CONFIG_ZTEST_NEW_API TRUE CACHE BOOL "" FORCE)

# Create commands to generate or copy common headers needed
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/include/common.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/stub_common.h ${CMAKE_BINARY_DIR}/include/common.h
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/stub_common.h
)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/include/util.h
    COMMAND ${CMAKE_COMMAND} -E copy ${PLATFORM_EC}/include/util.h ${CMAKE_BINARY_DIR}/include/util.h
    DEPENDS ${PLATFORM_EC}/include/util.h
)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/include/compile_time_macros.h
    COMMAND ${CMAKE_COMMAND} -E copy ${PLATFORM_EC}/include/compile_time_macros.h ${CMAKE_BINARY_DIR}/include/compile_time_macros.h
    DEPENDS ${PLATFORM_EC}/include/compile_time_macros.h
)
add_custom_target(stub_common_h
  DEPENDS
    ${CMAKE_BINARY_DIR}/include/common.h
    ${CMAKE_BINARY_DIR}/include/util.h
    ${CMAKE_BINARY_DIR}/include/compile_time_macros.h
)

# Add the stub and common headers as a dependency for TARGET
# This should be invoked generally from a Zephyr unittest build for the
# 'testbinary' target:
#   cros_ec_stub_common(testbinary)
function(cros_ec_stub_common TARGET)
  add_dependencies(${TARGET} stub_common_h)
  target_include_directories(${TARGET} PRIVATE ${CMAKE_BINARY_DIR}/include)
endfunction()
