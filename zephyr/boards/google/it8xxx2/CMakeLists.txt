# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if(CONFIG_KERNEL_NO_LTO)
  # Search for ite_allow_lto.cmake under program/* or program/*/*
  file(GLOB PROJECT_NAME
    "${CMAKE_CURRENT_LIST_DIR}/../../../program/*/ite_allow_lto.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../../../program/*/*/ite_allow_lto.cmake"
  )

  foreach(fn ${PROJECT_NAME})
    include(${fn})
  endforeach()
endif()
