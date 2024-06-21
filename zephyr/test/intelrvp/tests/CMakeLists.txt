cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/intelrvp/include")


# Add sources for board_power.c functionality and related platform-specific files

target_sources_ifdef(CONFIG_TEST_PROJECT_BOARD_POWER app PRIVATE
  ${PLATFORM_EC_PROGRAM_DIR}/intelrvp/mtlrvp/src/board_power.c
)
target_sources_ifdef(CONFIG_TEST_PROJECT_BOARD_POWER app PRIVATE
  mtlrvp/src/board_power.c
)

# Add sources for mtlrvp.c functionality and related platform-specific files
target_sources_ifdef(CONFIG_TEST_PROJECT_BOARD_ID app PRIVATE
  "${PLATFORM_EC_PROGRAM_DIR}/intelrvp/src/intel_rvp_board_id.c"
)
target_sources_ifdef(CONFIG_TEST_PROJECT_BOARD_ID app PRIVATE
  "${PLATFORM_EC_PROGRAM_DIR}/intelrvp/mtlrvp/src/mtlrvp.c"
)
target_sources_ifdef(CONFIG_TEST_PROJECT_BOARD_ID app PRIVATE mtlrvp/src/board_id.c)

# Add sources for tcpc.c functionality and related platform-specific files
zephyr_include_directories_ifdef(CONFIG_TEST_PROJECT_TCPC ${PLATFORM_EC}/driver/charger)
zephyr_include_directories_ifdef(CONFIG_TEST_PROJECT_TCPC ${PLATFORM_EC}/driver/ppc)
zephyr_include_directories_ifdef(CONFIG_TEST_PROJECT_TCPC ${PLATFORM_EC}/driver/usb_mux)
zephyr_include_directories_ifdef(CONFIG_TEST_PROJECT_TCPC ${PLATFORM_EC}/driver/tcpm)
target_sources_ifdef(CONFIG_TEST_PROJECT_TCPC app PRIVATE
  "${PLATFORM_EC_PROGRAM_DIR}/intelrvp/mtlrvp/src/tcpc.c"
)
target_sources_ifdef(CONFIG_TEST_PROJECT_TCPC app PRIVATE mtlrvp_npcx/src/tcpc.c)

# Add sources for pd.c (power delivery) functionality and related platform-specific files
target_sources_ifdef(CONFIG_TEST_PROJECT_PD app PRIVATE mtlrvp_pd/src/pd.c)
target_sources_ifdef(CONFIG_TEST_PROJECT_PD app PRIVATE
  "${PLATFORM_EC_PROGRAM_DIR}/intelrvp/mtlrvp/src/pd.c"
)

# Add sources for chg_usb_pd_mecc_1_1.c functionality and related platform-specific files
target_sources_ifdef(CONFIG_TEST_PROJECT_CHG_USB_PD_MECC app PRIVATE mtlrvp/src/chg_usb_pd_mecc_1_1.c)
target_sources_ifdef(CONFIG_TEST_PROJECT_CHG_USB_PD_MECC app PRIVATE
  "${PLATFORM_EC_PROGRAM_DIR}/intelrvp/src/chg_usb_pd_mecc_1_1.c"
)

