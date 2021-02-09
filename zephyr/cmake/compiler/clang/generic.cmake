# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if (DEFINED ENV{CC})
  set(CMAKE_C_COMPILER $ENV{CC})
else()
  set(CMAKE_C_COMPILER "/usr/bin/x86_64-pc-linux-gnu-clang")
endif()
