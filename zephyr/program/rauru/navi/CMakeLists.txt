# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a
# BSD-style license that can be found in the LICENSE file.

zephyr_library_sources("src/led.c")
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "src/dp_alt_mode.c")
zephyr_library_sources("src/usb_mux.c")
<<<<<<< HEAD   (43148c Revert "ec: Compile using coreboot-sdk from subtool")
=======
zephyr_library_sources("src/touch.c")
zephyr_library_sources("src/power_config.c")
>>>>>>> BRANCH (7ecdac ec: Support firmware branch synced builds)
