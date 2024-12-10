# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_include_directories(include)

zephyr_library_sources("src/touch_panel.c")
<<<<<<< HEAD   (4efc6a trulo: Initial zephyr config for uldrenite)
=======
zephyr_library_sources("src/common.c")
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
	"src/keyboard_customization.c"
)
>>>>>>> CHANGE (18e787 uldrenite: Enable strauss keyboard)
