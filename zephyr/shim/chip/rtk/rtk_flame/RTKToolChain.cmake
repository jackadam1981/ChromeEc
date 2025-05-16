# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# For Chipset: RTK EC
#
# Function: RTK Flash Utility
#

set_linker_property(NO_CREATE PROPERTY c_library    "-lc")
set_linker_property(NO_CREATE PROPERTY rt_library   "-lgcc")
set_linker_property(NO_CREATE PROPERTY c++_library  "-lstdc++")
set_linker_property(NO_CREATE PROPERTY math_library "-lm")

set(CMAKE_FIND_ROOT_PATH $ENV{ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi)

if(CONFIG_NEWLIB_LIBC AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
  # We are using c;rt;c (expands to '-lc -lgcc -lc') in code below.
  # This is needed because when linking with newlib on aarch64, then libgcc has a
  # link dependency to libc (strchr), but libc also has dependencies to libgcc.
  # Lib C depends on libgcc. e.g. libc.a(lib_a-fvwrite.o) references __aeabi_idiv
  set_property(TARGET linker APPEND PROPERTY link_order_library "math;c;rt;c")
else()
  set_property(TARGET linker APPEND PROPERTY link_order_library "c;rt")
endif()
