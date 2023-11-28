# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(i2c)

target_sources(app PRIVATE src/common.c)

target_sources_ifdef(CONFIG_TEST_I2C_DEFAULT app PRIVATE src/default.c)

target_sources_ifdef(CONFIG_TEST_I2C_PEC app PRIVATE src/pec.c)
