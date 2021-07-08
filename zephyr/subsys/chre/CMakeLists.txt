# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set (CMAKE_CXX_STANDARD 11)

zephyr_library()

zephyr_system_include_directories(
        "${THIRD_PARTY}/chre/chre_api/include"
        "${THIRD_PARTY}/chre/chre_api/include/chre_api"
        "${THIRD_PARTY}/chre/core/include"
        "${THIRD_PARTY}/chre/platform/include"
        "${THIRD_PARTY}/chre/util/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

zephyr_library_sources(
        "${THIRD_PARTY}/chre/core/sensor_type.cc"
)

add_compile_definitions(
        CHRE_MESSAGE_TO_HOST_MAX_SIZE=${CONFIG_CHRE_MESSAGE_TO_HOST_MAX_SIZE})

if (DEFINED CONFIG_CHRE_MINIMUM_LOG_LEVEL_MUTE)
    add_compile_definitions(CHRE_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_MUTE)
elseif (DEFINED CONFIG_CHRE_MINIMUM_LOG_LEVEL_ERROR)
    add_compile_definitions(CHRE_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_ERROR)
elseif (DEFINED CONFIG_CHRE_MINIMUM_LOG_LEVEL_WARN)
    add_compile_definitions(CHRE_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_WARN)
elseif (DEFINED CONFIG_CHRE_MINIMUM_LOG_LEVEL_INFO)
    add_compile_definitions(CHRE_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_INFO)
elseif (DEFINED CONFIG_CHRE_MINIMUM_LOG_LEVEL_DEBUG)
    add_compile_definitions(CHRE_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_DEBUG)
else()
    add_compile_definitions(CHRE_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_VERBOSE)
endif()

if (DEFINED CONFIG_CHRE_ASSERTIONS)
    add_compile_definitions(CHRE_ASSERTIONS_ENABLED)
else()
    add_compile_definitions(CHRE_ASSERTIONS_DISABLED)
endif()
