# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources(cros_button.c)
zephyr_library_sources_ifdef(CONFIG_DT_HAS_CROS_EC_BUTTON_ENABLED button_driver.c)
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_POWER_BUTTON_V2 power_button.c)
