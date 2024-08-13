# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(utils)

<<<<<<< HEAD   (404853 Revert "Reland "zephyr: Enable automatic download of coreboo)
=======
target_sources_ifdef(CONFIG_TEST_LIB_SBRK app PRIVATE src/sbrk.c)
>>>>>>> BRANCH (ba59ee pdc: ti: Overwrite data role in GET_CONNECTOR_STATUS output)
target_sources_ifdef(CONFIG_TEST_LIB_STD_VECTOR app PRIVATE src/std_vector.cc)
target_sources_ifdef(CONFIG_TEST_LIB_STDLIB app PRIVATE src/stdlib.c)

add_subdirectory(${PLATFORM_EC}/zephyr/test/test_utils test_utils)
