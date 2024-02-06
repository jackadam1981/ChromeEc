# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources(runtime_interrupt_toggle_test.cc)
zephyr_library_link_libraries(pw_string pw_function)
