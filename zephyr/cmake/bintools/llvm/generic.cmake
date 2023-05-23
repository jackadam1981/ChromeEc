# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set(CMAKE_AR         "/usr/bin/llvm-ar")
set(CMAKE_NM         "/usr/bin/llvm-nm")
set(CMAKE_OBJCOPY    "/usr/bin/llvm-objcopy")
set(CMAKE_OBJDUMP    "/usr/bin/llvm-objdump")
set(CMAKE_RANLIB     "/usr/bin/llvm-ranlib")
set(CMAKE_READELF    "/usr/bin/llvm-readelf")

# CMake is looking for bintools by adding a suffix to compiler binary
# e.g for AR it would be armv7m-cros-eabi-clang-ar, which doesn't exist.
# Set bintools locations manually
set(CMAKE_C_COMPILER_AR         "/usr/bin/llvm-ar")
set(CMAKE_C_COMPILER_NM         "/usr/bin/llvm-nm")
set(CMAKE_C_COMPILER_OBJCOPY    "/usr/bin/llvm-objcopy")
set(CMAKE_C_COMPILER_OBJDUMP    "/usr/bin/llvm-objdump")
set(CMAKE_C_COMPILER_RANLIB     "/usr/bin/llvm-ranlib")
set(CMAKE_C_COMPILER_READELF    "/usr/bin/llvm-readelf")
