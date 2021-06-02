# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CBI_EEPROM	cros_cbi.c)
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CBI_VIRTUAL	cros_cbi.c)
