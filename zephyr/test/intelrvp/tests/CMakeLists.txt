# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/intelrvp/include")

# Add sources for testing board_id functionality
target_sources_ifdef(CONFIG_TEST_PROJECT_BOARD_ID app PRIVATE
  "${PLATFORM_EC_PROGRAM_DIR}/intelrvp/src/intel_rvp_board_id.c"
)
target_sources_ifdef(CONFIG_TEST_PROJECT_BOARD_ID app PRIVATE intelrvp/src/board_id.c)

# Add sources for dc_jack.c functionality and related platform-specific files
target_sources_ifdef(CONFIG_TEST_PROJECT_DC_JACK app PRIVATE intelrvp/src/dc_jack.c)
target_sources_ifdef(CONFIG_TEST_PROJECT_DC_JACK app PRIVATE
  "${PLATFORM_EC_PROGRAM_DIR}/intelrvp/src/dc_jack.c"
)

if((CONFIG_TEST_PROJECT_MTLRVPP_NPCX) OR (CONFIG_TEST_PROJECT_MTLRVPP_PD)
  OR (CONFIG_TEST_PROJECT_MTLRVPP_MCHP) OR (CONFIG_TEST_PROJECT_MTLRVPP_COMMON))
# Add sources for board_power.c functionality and related platform-specific files
target_sources_ifdef(CONFIG_TEST_PROJECT_MTL_BOARD_POWER app PRIVATE
  ${PLATFORM_EC_PROGRAM_DIR}/intelrvp/mtlrvp/src/board_power.c
)
target_sources_ifdef(CONFIG_TEST_PROJECT_MTL_BOARD_POWER app PRIVATE
  ${PLATFORM_EC}/zephyr/test/power/meteorlake.c
)

# Add board specific sources for board_id.c functionality and related platform-specific files
target_sources_ifdef(CONFIG_TEST_PROJECT_BOARD_ID app PRIVATE
  "${PLATFORM_EC_PROGRAM_DIR}/intelrvp/mtlrvp/src/board_id.c"
)

# Add sources for tcpc.c functionality and related platform-specific files
zephyr_include_directories_ifdef(CONFIG_TEST_PROJECT_TCPC ${PLATFORM_EC}/driver/charger)
zephyr_include_directories_ifdef(CONFIG_TEST_PROJECT_TCPC ${PLATFORM_EC}/driver/ppc)
zephyr_include_directories_ifdef(CONFIG_TEST_PROJECT_TCPC ${PLATFORM_EC}/driver/usb_mux)
zephyr_include_directories_ifdef(CONFIG_TEST_PROJECT_TCPC ${PLATFORM_EC}/driver/tcpm)
target_sources_ifdef(CONFIG_TEST_PROJECT_TCPC app PRIVATE
  "${PLATFORM_EC_PROGRAM_DIR}/intelrvp/mtlrvp/src/tcpc.c"
)
target_sources_ifdef(CONFIG_TEST_PROJECT_TCPC app PRIVATE mtlrvp_npcx/src/tcpc.c)

# Add sources for chg_usb_pd_mecc_1_1.c functionality and related platform-specific files
target_sources_ifdef(CONFIG_TEST_PROJECT_CHG_USB_PD_MECC app PRIVATE intelrvp/src/chg_usb_pd_mecc_1_1.c)
target_sources_ifdef(CONFIG_TEST_PROJECT_CHG_USB_PD_MECC app PRIVATE
  "${PLATFORM_EC_PROGRAM_DIR}/intelrvp/src/chg_usb_pd_mecc_1_1.c"
)
endif()

if (CONFIG_TEST_PROJECT_PTLRVPP_MCHP)
zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/intelrvp/ptlrvp/ptlrvp_mchp/include")
# Add sources for board_power.c functionality and related ptlrvp platform-specific files
target_sources_ifdef(CONFIG_TEST_PROJECT_PTL_BOARD_POWER app PRIVATE
  ${PLATFORM_EC_PROGRAM_DIR}/intelrvp/ptlrvp/src/board_power.c
)
target_sources_ifdef(CONFIG_TEST_PROJECT_PTL_BOARD_POWER app PRIVATE
  ${PLATFORM_EC}/zephyr/test/power/meteorlake.c
)
endif()
