# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20.0)
set(CMAKE_CXX_STANDARD 20)

project(uart_printf)

set(SOURCES src/main.cc)
# Create the unittest libraries and binary
find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Tell CMake to compile uart_printf.c as C++
set_source_files_properties(../../../common/uart_printf.c
  PROPERTIES
    LANGUAGE CXX
)

#target_precompile_headers(testbinary
#  PRIVATE
#    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/fff_function_signature.pch>"
#)

target_sources(testbinary
  PRIVATE
    ../../../common/uart_printf.c
    src/fakes.cc
    src/main.cc
)

target_include_directories(testbinary
  PRIVATE
    include
)
