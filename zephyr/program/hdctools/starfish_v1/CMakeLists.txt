# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.5)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
zephyr_compile_options(-fdiagnostics-color=always)
project(starfish_v1)
zephyr_compile_options(-fdiagnostics-color=always)
zephyr_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
zephyr_include_directories("${PLATFORM_EC}/include")

target_sources(app PRIVATE src/app_init.c)
target_sources(app PRIVATE src/dfu.c)
target_sources(app PRIVATE src/dfu_usb.c)

target_sources(app PRIVATE /mnt/host/source/src/platform/ec/zephyr/shim/src/libgcc_arm.S)
zephyr_compile_options(-fdiagnostics-color=always)
