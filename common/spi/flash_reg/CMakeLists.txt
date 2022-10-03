

cmake_minimum_required(VERSION 3.20.0)

add_library(spi_flash_reg src/spi_flash_reg.c)
target_sources(spi_flash_reg PUBLIC public/spi_flash_reg.h)
target_include_directories(spi_flash_reg INTERFACE public)
target_include_directories(spi_flash_reg
  PRIVATE
    ${PLATFORM_EC}/include
)

if(NOT BOARD STREQUAL unit_testing)
#  zephyr_link_library(spi_flash_reg)
endif()

if(BOARD STREQUAL unit_testing)
  project(flash_reg)
  set(SOURCES
      src/test.c
  )
  find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})

  target_link_libraries(testbinary PRIVATE spi_flash_reg)
  target_include_directories(testbinary PRIVATE private)

  target_link_libraries(spi_flash_reg PRIVATE test_interface)
  target_include_directories(spi_flash_reg PRIVATE private)

  add_compile_definitions(
      CONFIG_SPI_FLASH_W25Q64
      CONFIG_FLASH_SIZE_BYTES=4194304
  )
endif()