# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set(CMAKE_BUILD_TYPE Release)

set(CMAKE_SYSTEM_NAME Linux)

set(ANDROID TRUE)

set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CROS_EC_REPO CROSS_COMPILE CC_NAME CXX_NAME)
include("${CROS_EC_REPO}/cmake/toolchain-common.cmake")

# Pretend as "Trusty", an embedded platform.
add_definitions(-D__TRUSTY__)

# See https://www.chromium.org/chromium-os/build/c-exception-support
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables")
