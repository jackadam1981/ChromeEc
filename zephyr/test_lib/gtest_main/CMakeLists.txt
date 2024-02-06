# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources(main.cc)
zephyr_library_link_libraries(
    pw_unit_test.light
    pw_unit_test.logging_event_handler
)
