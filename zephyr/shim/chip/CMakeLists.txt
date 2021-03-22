# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Translate Zephyr CONFIG_SOC_* values to platform/ec CHIP_* values.
if (DEFINED CONFIG_SOC_NPCX7M6FB)
	set(CHIP_FAMILY "NPCX7")
	set(CHIP_VARIANT "NPCX7M6FB")
endif()

if (DEFINED CONFIG_SOC_FAMILY_NPCX)
	add_subdirectory(npcx)
endif()

