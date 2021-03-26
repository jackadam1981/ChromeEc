# SPDX-License-Identifier: Apache-2.0

#zephyr_library()

zephyr_library_sources_ifdef(CONFIG_CROS_SHI_NPCX	cros_shi_npcx.c)
