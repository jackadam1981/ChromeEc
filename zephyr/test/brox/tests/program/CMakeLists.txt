# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

target_sources_ifdef(CONFIG_TEST_PROGRAM_BOARD_POWER app PRIVATE src/board_power.c)
target_sources_ifdef(CONFIG_TEST_PROGRAM_BOARD_POWER app PRIVATE ${PLATFORM_EC_PROGRAM_DIR}/brox/src/board_power.c)
target_sources_ifdef(CONFIG_TEST_PROGRAM_BOARD_POWER app PRIVATE ${PLATFORM_EC_PROGRAM_DIR}/brox/src/charger.c)