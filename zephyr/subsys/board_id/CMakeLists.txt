# SPDX-License-Identifier: Apache-2.0

zephyr_library_include_directories(include)

zephyr_library_sources_ifdef(CONFIG_INTEL_BOARD_ID board_id.c)
zephyr_library_sources_ifdef(CONFIG_INTEL_VARIANT_GPIO variant_gpio.c)
