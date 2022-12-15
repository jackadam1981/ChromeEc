# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(it8xxx2_evb)

# Include board specific header files
zephyr_include_directories(include)

zephyr_library_sources_ifndef(CONFIG_I2C_TARGET "src/i2c_test.c")
zephyr_library_sources_ifdef(CONFIG_I2C_TARGET "src/i2c_ite_target_cb.c")
zephyr_library_sources_ifdef(CONFIG_I2C_TARGET "src/board.c")
