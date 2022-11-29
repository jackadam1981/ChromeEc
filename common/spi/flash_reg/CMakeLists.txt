# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.0)

find_package(CrosEc REQUIRED HINTS "${PLATFORM_EC}")

# Create the library
ec_library(spi_flash_reg
  SOURCES
    src/spi_flash_reg.c
  HEADERS
    public/spi_flash_reg.h
  PUBLIC_INCLUDES
    public
)

ec_library_tests(spi_flash_reg
  SOURCES
    src/spi_flash_reg_test.c
  PRIVATE_INCLUDES
    private
  PUBLIC_COMPILE_OPTIONS
      -DCONFIG_SPI_FLASH_W25Q64
      -DCONFIG_FLASH_SIZE_BYTES=4194304
)
