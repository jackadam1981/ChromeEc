# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_SYSROOT /usr/armv7m-cros-eabi/)

set(CMAKE_C_COMPILER armv7m-cros-eabi-clang)
set(CMAKE_CXX_COMPILER armv7m-cros-eabi-clang)

set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
