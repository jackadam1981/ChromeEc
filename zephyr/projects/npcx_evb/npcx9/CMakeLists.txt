# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(npcx9)

zephyr_include_directories(include)

set(PLATFORM_EC_BOARD "${PLATFORM_EC}/board/${BOARD}" CACHE PATH
    "Path to the platform/ec board directory")

zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD
  "${PLATFORM_EC_BOARD}/keyboard.c")
