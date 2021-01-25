# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# On ARM, we don't use libgcc: It's built against a fixed target (e.g.
# used instruction set, ABI, ISA extensions) and doesn't adapt when
# compiler flags change any of these assumptions. Use our own mini-libgcc
# instead.
if("${ARCH}" STREQUAL "arm")
set(no_libgcc True)
endif()

# Zephyr cmake system looks into ${TOOLCHAIN_ROOT}, but we just send
# this out to the copy in ${ZEPHYR_BASE}.
include("${ZEPHYR_BASE}/cmake/compiler/gcc/target.cmake")
