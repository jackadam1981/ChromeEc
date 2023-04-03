# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Placeholder for future CLs in the starfish/src and starfish/include

cmake_minimum_required(VERSION 3.20.5)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

zephyr_include_directories(include)

zephyr_library_sources(src/buttons.c)
zephyr_library_sources(src/leds.c)
zephyr_library_sources(src/sim.c)
zephyr_library_sources(src/sim_button.c)
zephyr_library_sources(src/sim_console.c)
zephyr_library_sources(src/sim_gpio.c)
zephyr_library_sources(src/sim_info.c)
zephyr_library_sources(src/sim_persist.c)
zephyr_library_sources(src/sim_timer.c)
