# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Translate Zephyr CONFIG_SOC_* values to platform/ec CHIP_* values.
if (DEFINED CONFIG_SOC_NPCX7M6FB)
	zephyr_compile_definitions(CHIP_FAMILY_NPCX7)
	zephyr_compile_definitions(CHIP_VARIANT_NPCX7M6FB)
	zephyr_include_directories("${PLATFORM_EC}/chip/npcx")
endif()

# For now zephyr only supports npcx7m6fb of the npcx chip family. Once more
# are added, it is likely that this condition will need to be updated.
add_subdirectory_ifdef(CONFIG_SOC_NPCX7M6FB "npcx")

