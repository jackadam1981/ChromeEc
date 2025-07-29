# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# For Chipset: RTK EC
#
# Function: RTK Flash Utility
#

cmake_minimum_required (VERSION 3.20)
include(RTKToolChain.cmake)
zephyr_get_include_directories_for_lang(C zephyr_includes STRIP_PREFIX)

add_executable(rtk_flame)

target_sources(rtk_flame PRIVATE
  spi_upload.c
  flash_map_backend.c
  flash_spic.c
)

get_property(
  zephyr_compile_options
  TARGET zephyr_interface
  PROPERTY INTERFACE_COMPILE_OPTIONS
)
target_include_directories(rtk_flame PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  "${PLATFORM_EC}/zephyr/shim/include"
)
target_include_directories(rtk_flame PRIVATE "${zephyr_includes}")

target_compile_options(rtk_flame PRIVATE "${zephyr_compile_options};$<TARGET_PROPERTY:compiler,prohibit_lto>"
  -mthumb
  -mfpu=fpv5-sp-d16
  -std=gnu11
  -mlittle-endian
  -O0
  -g3
  -imacros ${AUTOCONF_H}
)

target_compile_definitions(rtk_flame PRIVATE
  __NO_SYSTEM_INIT=1
  __STACK_SIZE=0x000007FC
  __START=spic_flash_upload
)

target_link_libraries(rtk_flame INTERFACE -lc)
target_link_options(rtk_flame BEFORE PRIVATE
  -nostartfiles
  -nostdlib
  -lgcc
  -O0
  -g3
  -mthumb
  "$<TARGET_PROPERTY:compiler,prohibit_lto>"
  -Wl,-T ${CMAKE_CURRENT_SOURCE_DIR}/spi_upload.ld
)


set(rtk_flame_elf ${CMAKE_CURRENT_BINARY_DIR}/rtk_flame.elf)
set(rtk_flame_bin ${CMAKE_BINARY_DIR}/rts5915_flash_upload.bin)


add_custom_target(generate_rtk_flame
  COMMAND ${CMAKE_OBJCOPY} -O binary ${rtk_flame_elf} ${rtk_flame_bin}
  BYPRODUCTS ${rtk_flame_bin}
  DEPENDS rtk_flame
)
add_dependencies(zephyr generate_rtk_flame)
