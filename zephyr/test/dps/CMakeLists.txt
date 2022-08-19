# Copyright 2022 The Chromium OS Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(crc)

# Include the test source and the file under test
target_sources(app PRIVATE ${PLATFORM_EC}/common/dps.c
			   ${PLATFORM_EC}/common/usb_pd_dual_role.c)

target_sources(
  app PRIVATE
    src/main.c
)

add_compile_definitions(PD_MAX_VOLTAGE_MV=15000)
add_compile_definitions(PD_MAX_CURRENT_MA=3000)
add_compile_definitions(PD_MAX_POWER_MW=45000)
add_compile_definitions(PD_OPERATING_POWER_MW=45000)
