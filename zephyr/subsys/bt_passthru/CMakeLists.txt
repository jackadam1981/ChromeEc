# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources_ifdef(CONFIG_BT_PASSTHRU
	bt_passthru.c
)
