# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_include_directories("include")

zephyr_library_sources("src/led.c")
zephyr_library_sources_ifdef(
    CONFIG_PDC_RUNTIME_PORT_CONFIG "src/usbc_runtime.c")
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CUSTOM_FAN_CONTROL
	"src/fan.c"
	"src/thermal.c"
)
zephyr_library_sources("src/board.c")
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
	"src/keyboard_customization.c"
)
