# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(drivers)

# Include the local test directory for shimmed_test_tasks.h
zephyr_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
zephyr_include_directories("${PLATFORM_EC}/zephyr/test/drivers/include/")
zephyr_include_directories("${PLATFORM_EC}/driver/ppc/")

target_sources(app PRIVATE
  ./usb.c
  ./usb_20v_3a_pd_charger.c
  ./usb_5v_3a_pd_sink.c
  ./usb_5v_3a_pd_source.c
  ./usb_alt_mode.c
  ./usb_attach_src_snk.c
  ./usb_pd_ctrl_msg.c
  ./usb_malfunction_sink.c
  ../../main.c
  ../../stubs.c
  ../../test_mocks.c
  ../../utils.c
)

set_compiler_property(APPEND PROPERTY coverage -O0)
