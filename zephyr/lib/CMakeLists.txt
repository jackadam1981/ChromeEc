# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if(NOT "${ZEPHYR_PIGWEED_MODULE_DIR}" STREQUAL "")
  add_subdirectory(math)
endif()
