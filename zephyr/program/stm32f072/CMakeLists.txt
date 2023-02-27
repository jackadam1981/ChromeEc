# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.5)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(kb)

zephyr_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# target_sources(app PRIVATE kb/src/main.c)
target_sources(app PRIVATE kb/src/fake.c)
target_sources(app PRIVATE kb/src/cortex-div.S)
target_sources(app PRIVATE kb/src/cortex-ldivmod.S)
