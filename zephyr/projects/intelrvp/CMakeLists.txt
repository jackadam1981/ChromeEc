# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

zephyr_include_directories(include)

if(DEFINED CONFIG_BOARD_ADLRVP_NPCX)
	project(adlrvp_npcx)
	zephyr_include_directories(adlrvp/adlrvp_npcx/include)
	zephyr_library_sources("adlrvp/adlrvp.c")
endif()
