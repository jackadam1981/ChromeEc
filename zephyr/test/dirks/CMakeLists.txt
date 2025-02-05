# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(dirks)

add_subdirectory(${PLATFORM_EC}/zephyr/test/test_utils test_utils)

zephyr_include_directories(
        "${PLATFORM_EC_PROGRAM_DIR}/dirks/include"
        include)

target_sources(app PRIVATE src/stubs.c)

if(CONFIG_TEST_DIRKS_BOARD_POWER)
    # Test source
    if(CONFIG_AP_PWRSEQ_DRIVER)
        target_sources(app PRIVATE src/board_ap_pwrseq_drv.c)
    else()
        target_sources(app PRIVATE src/board_power.c)
    endif()
    # Module under test
    target_sources(app PRIVATE ${PLATFORM_EC_PROGRAM_DIR}/dirks/src/board_power.c)
endif()

target_sources_ifdef(
        CONFIG_TEST_DIRKS_SUB_BOARD
        app PRIVATE
        src/sub_board.c
        src/log_module.c
        ${PLATFORM_EC_PROGRAM_DIR}/dirks/src/sub_board.c)

target_sources_ifdef(
        CONFIG_TEST_DIRKS_COMMON
        app PRIVATE
        src/common.c
        ${PLATFORM_EC_PROGRAM_DIR}/dirks/src/common.c)

if(CONFIG_TEST_DIRKS_DIRKS)
    zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/dirks/dirks/include")
    target_sources(
            app PRIVATE
            src/dirks.c
            ${PLATFORM_EC_PROGRAM_DIR}/dirks/dirks/src/barrel_jack.c
            ${PLATFORM_EC_PROGRAM_DIR}/dirks/dirks/src/led.c
            ${PLATFORM_EC_PROGRAM_DIR}/dirks/dirks/src/hdmi.c
            ${PLATFORM_EC_PROGRAM_DIR}/dirks/dirks/src/usbc.c
    )
endif()
