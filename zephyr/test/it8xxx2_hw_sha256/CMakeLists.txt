# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(it8xxx2_hw_sha256)

zephyr_include_directories("${PLATFORM_EC}/zephyr/shim/chip/it8xxx2/include")
zephyr_library_sources("${PLATFORM_EC}/driver/sha256/sha256_it8xxx2.c")
# Include the test source and the file under test
target_sources(app PRIVATE main.c)
