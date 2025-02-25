# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include("${ZEPHYR_BASE}/cmake/compiler/clang/target.cmake")

message("RTLIB_DIR: ${RTLIB_DIR}")
#set_property(TARGET linker PROPERTY lib_include_dir "-L${RTLIB_DIR}/baremetal")
set_property(TARGET linker PROPERTY lib_include_dir "-L/usr/lib64/clang/20/lib/baremetal")
set_property(TARGET linker PROPERTY rt_library "-lclang_rt.builtins-riscv32")

set(CMAKE_C_COMPILER "${CROSS_COMPILE}clang")
set(CMAKE_CXX_COMPILER "${CROSS_COMPILE}clang++")
