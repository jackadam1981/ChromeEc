# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# When building code coverage, the final Zephyr image isn't actually linked
# and there's no valid image to program. Skip compiling and linking the
# NPCX/RTK monitor when coverage is enabled.
if (NOT DEFINED CONFIG_COVERAGE)
  add_subdirectory_ifdef(CONFIG_SOC_FAMILY_MICROCHIP_MEC mchp_second_loader)
  add_subdirectory_ifdef(CONFIG_SOC_FAMILY_NPCX npcx_monitor)
  add_subdirectory_ifdef(CONFIG_SOC_FAMILY_REALTEK_EC rtk_flame)
endif()
