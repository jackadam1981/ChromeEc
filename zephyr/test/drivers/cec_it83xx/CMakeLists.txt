# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

target_sources(app PRIVATE src/cec_it83xx.c)
