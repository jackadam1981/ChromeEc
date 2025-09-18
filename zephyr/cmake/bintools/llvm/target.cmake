# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include("${ZEPHYR_BASE}/cmake/bintools/llvm/target.cmake")

if ("${ARCH}" STREQUAL "arm" OR "${ARCH}" STREQUAL "riscv")
  # CMake is looking for bintools by adding a suffix to compiler binary
  # e.g for AR it would be armv7m-cros-eabi-clang-ar, which doesn't exist.
  # Set bintools locations manually
  set(CMAKE_C_COMPILER_AR       "${CMAKE_AR}")
  set(CMAKE_C_COMPILER_NM       "${CMAKE_NM}")
  set(CMAKE_C_COMPILER_OBJCOPY  "${CMAKE_OBJCOPY}")
  set(CMAKE_C_COMPILER_OBJDUMP  "${CMAKE_OBJDUMP}")
  set(CMAKE_C_COMPILER_RANLIB   "${CMAKE_RANLIB}")
  set(CMAKE_C_COMPILER_READELF  "${CMAKE_READELF}")

  # And for C++
  set(CMAKE_CXX_COMPILER_AR       "${CMAKE_AR}")
  set(CMAKE_CXX_COMPILER_NM       "${CMAKE_NM}")
  set(CMAKE_CXX_COMPILER_OBJCOPY  "${CMAKE_OBJCOPY}")
  set(CMAKE_CXX_COMPILER_OBJDUMP  "${CMAKE_OBJDUMP}")
  set(CMAKE_CXX_COMPILER_RANLIB   "${CMAKE_RANLIB}")
  set(CMAKE_CXX_COMPILER_READELF  "${CMAKE_READELF}")
endif()
