# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(fpc1025_test)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

message(STATUS "PLATFORM_EC: ${PLATFORM_EC}")


# Add fingerprint drivers directory to include fingerprint_fpc1025_pal.h
target_include_directories(app PRIVATE ${PLATFORM_EC}/zephyr/drivers/fingerprint)
target_include_directories(ec_shim  PUBLIC ${PLATFORM_EC}/include)
