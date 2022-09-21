# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(craask)

zephyr_include_directories("${PLATFORM_EC}/zephyr/projects/nissa/include")

target_sources(app PRIVATE src/fakes.c)

target_sources_ifdef(CONFIG_TEST_CRAASK
app PRIVATE ${PLATFORM_EC}/zephyr/projects/nissa/craask/src/form_factor.c)
target_sources_ifdef(CONFIG_TEST_ALT_SENSOR_PROBE
			app PRIVATE src/alt_sensor.c)
