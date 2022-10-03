# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.0)

set(PLATFORM_EC "${CMAKE_CURRENT_LIST_DIR}/../../..")
get_filename_component(PLATFORM_EC "${PLATFORM_EC}" REALPATH)

project(spi_flash_reg)
set(SOURCES
    src/main.c
    ${PLATFORM_EC}/common/spi/flash_reg/src/spi_flash_reg.c
)
find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})

configure_file(
    ${PLATFORM_EC}/common/spi/flash_reg/public/spi_flash_reg.h
    ${CMAKE_BINARY_DIR}/include/spi_flash_reg.h COPYONLY
)
target_include_directories(testbinary
  PRIVATE
    include/
    ${CMAKE_BINARY_DIR}/include
)

# W25Q64 has 0x400000 bytes as the first section, setting
# CONFIG_FLASH_SIZE_BYTES to that allows us to test every branch of
# spi_flash_reg.c
target_compile_definitions(testbinary
  PUBLIC
    CONFIG_SPI_FLASH_W25Q64
    CONFIG_FLASH_SIZE_BYTES=4194304
)
