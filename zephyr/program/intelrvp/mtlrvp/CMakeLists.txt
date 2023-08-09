# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if(DEFINED CONFIG_BOARD_MTLRVP_MCHP)
  add_subdirectory(mtlrvpp_mchp)
elseif(DEFINED CONFIG_BOARD_MTLRVP_NPCX)
  add_subdirectory(mtlrvpp_npcx)
elseif(DEFINED CONFIG_BOARD_MTLRVP_NPCX_PD)
  add_subdirectory(mtlrvpp_pd)
endif()

zephyr_include_directories("include")
zephyr_library_sources("src/mtlrvp_sub.c")
zephyr_library_sources("src/board_power.c")
