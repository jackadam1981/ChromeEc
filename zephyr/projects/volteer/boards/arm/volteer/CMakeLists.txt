# SPDX-License-Identifier: Apache-2.0

# find ECST tool for generating npcx header used by ROM code

# This calls into a helper script in the npcx eval board directory; this script
# should be moved into a better location (e.g. chip series directory)

if (DEFINED CONFIG_PLATFORM_EC_RO_HEADER)
    # Questions:
    # 1. Why npcx7m7? CONFIG_SOC_NPCX7M6FB is set to y.
    # 2. Why flashsize 8? All the docs say we're looking at 512KiB flash, not
    #    8MiB.
    # 3. Does zmake support passing a custom Kconfig file for each build type?
    if ("${CONFIG_FLASH_SIZE}" STREQUAL "512" OR "${CONFIG_FLASH_SIZE}" STREQUAL "1024")
        set(flash_size 1)
    elseif ("${CONFIG_FLASH_SIZE}" STREQUAL "2048")
        set(flash_size 2)
    elseif ("${CONFIG_FLASH_SIZE}" STREQUAL "4096")
        set(flash_size 4)
    elseif ("${CONFIG_FLASH_SIZE}" STREQUAL "8192")
        set(flash_size 8)
    elseif ("${CONFIG_FLASH_SIZE}" STREQUAL "16384")
        set(flash_size 16)
    endif()

    if (DEFINED CONFIG_SOC_NPCX7M6FB)
        set(chip "npcx7m6")
    endif()

    if (NOT DEFINED CONFIG_PLATFORM_EC_RO_HEADER_ENABLE_HEADER_CRC)
        set(hcrc "-nohcrc")
    endif()

    if (NOT DEFINED CONFIG_PLATFORM_EC_RO_HEADER_ENABLE_FIRMWARE_CRC)
        set(fcrc "-nofcrc")
    endif()

    set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
            COMMAND ${PYTHON_EXECUTABLE} ${ZEPHYR_BASE}/boards/arm/npcx7m6fb_evb/support/ecst.py
            -i ${PROJECT_BINARY_DIR}/${CONFIG_KERNEL_BIN_NAME}.bin
            -o ${TARGET_IMAGE_FILE} ${hcrc} ${fcrc}
            -chip ${chip} -flashsize ${flash_size}
            -spimaxclk ${CONFIG_PLATFORM_EC_RO_HEADER_SPI_MAX_CLOCK}
            -spireadmode ${CONFIG_PLATFORM_EC_RO_HEADER_SPI_READ_MODE})
endif()
