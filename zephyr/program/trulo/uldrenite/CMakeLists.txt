# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_include_directories(include)

zephyr_library_sources("src/touch_panel.c")
zephyr_library_sources("src/common.c")
<<<<<<< PATCH SET (63ebd3 uldrenite: Enable strauss keyboard)
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
	"src/keyboard_customization.c"
)
=======
>>>>>>> BASE      (46a818 uldrenite: Modify PDC to TI)
