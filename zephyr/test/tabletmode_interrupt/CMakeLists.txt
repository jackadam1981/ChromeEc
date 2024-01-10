# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(tabletmode_interrupt)

target_sources(app PRIVATE src/main.c)
