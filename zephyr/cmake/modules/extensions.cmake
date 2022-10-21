# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include_guard(GLOBAL)

# Sets the provided variable to the multi_value_keywords from ec_library.
macro(_ec_add_library_multi_value_args variable)
  set("${variable}" SOURCES HEADERS
                    PUBLIC_DEPS PRIVATE_DEPS
                    PUBLIC_INCLUDES PRIVATE_INCLUDES
                    PUBLIC_DEFINES PRIVATE_DEFINES
                    PUBLIC_COMPILE_OPTIONS PRIVATE_COMPILE_OPTIONS
                    PUBLIC_LINK_OPTIONS PRIVATE_LINK_OPTIONS "${ARGN}")
endmacro()

# Wrapper around cmake_parse_arguments that fails with an error if any arguments
# remained unparsed.
macro(ec_parse_arguments_strict function_name start_arg options one multi)
  cmake_parse_arguments(PARSE_ARGV
      "${start_arg}" arg "${options}" "${one}" "${multi}"
  )
  if(NOT "${arg_UNPARSED_ARGUMENTS}" STREQUAL "")
    set(_all_args ${options} ${one} ${multi})
    message(FATAL_ERROR
        "Unexpected arguments to ${function_name}: ${arg_UNPARSED_ARGUMENTS}\n"
        "Valid arguments: ${_all_args}"
    )
  endif()
endmacro()

# Create an EC library
#
# Required Args:
#
#   <name> - the name of the library target to be created
#
# Optional Args:
#   SOURCES - source files for this library
#   HEADERS - header files for this library
#   PUBLIC_INCLUDES - public target_include_directories arguments
#   PRIVATE_INCLUDES - private target_include_directories arguments
#
function(ec_library NAME)
  _ec_add_library_multi_value_args(multi_value_args)
  ec_parse_arguments_strict(ec_library 1 "" "" "${multi_value_args}")
  if(BOARD STREQUAL unit_testing)
    add_library(${NAME})
  else()
    zephyr_library_named(${NAME})
    target_link_libraries(${NAME} PRIVATE cros_ec_interface)
    # When LTO is enabled, enable only for the "app" library, which compiles
    # and links all Chromium OS sources.
    # TODO: Enable LTO for all sources when Zephyr supports it.
    # See https://github.com/zephyrproject-rtos/zephyr/issues/2112
    if (DEFINED CONFIG_LTO)
      # The Zephyr toolchain generates linker errors if both CONFIG_LTO and
      # CONFIG_FPU are used. See b/184302085.
      if(("${ZEPHYR_TOOLCHAIN_VARIANT}" STREQUAL "zephyr") AND
          (DEFINED CONFIG_FPU))
        message(STATUS
            "Zephyr toolchain and CONFIG_FPU detected: disabling LTO")
      else()
        set_property(
          TARGET
            ${NAME}
          PROPERTY
            INTERPROCEDURAL_OPTIMIZATION True
        )
      endif()
    endif()
  endif()
  if(NOT "${arg_SOURCES}" STREQUAL "")
    target_sources(${NAME} PRIVATE ${arg_SOURCES})
  endif()
  if(NOT "${arg_HEADERS}" STREQUAL "")
    target_sources(${NAME} PUBLIC ${arg_HEADER})
  endif()
  if(NOT "${arg_PUBLIC_INCLUDES}" STREQUAL "")
    target_include_directories(${NAME} PUBLIC ${arg_PUBLIC_INCLUDES})
  endif()
  if(NOT "${arg_PRIVATE_INCLUDES}" STREQUAL "")
    target_include_directories(${NAME} PRIVATE ${arg_PRIVATE_INCLUDES})
  endif()
endfunction()

function(ec_library_tests NAME)
  if(NOT BOARD STREQUAL unit_testing)
    return()
  endif()
  project(${NAME}_test)

  _ec_add_library_multi_value_args(multi_value_args)
  ec_parse_arguments_strict(ec_library_tests 1 "" "" "${multi_value_args}")

  # Zephyr's unittest also generates a test_interface which we need in the
  # library (not just in the test binary)
  target_link_libraries(${NAME} PRIVATE test_interface)
  string(TOLOWER "${CMAKE_C_COMPILER_ID}" lowercase_compiler_id)
  if("${lowercase_compiler_id}" STREQUAL "clang")
    target_link_options(testbinary PRIVATE -fprofile-instr-generate)
  endif()

  target_link_libraries(testbinary PRIVATE ${NAME})

  if(NOT "${arg_SOURCES}" STREQUAL "")
    target_sources(testbinary PRIVATE ${arg_SOURCES})
  endif()

  # Add the private directory used to mock components of the EC
  if(NOT "${arg_PRIVATE_INCLUDES}" STREQUAL "")
    target_include_directories(testbinary PRIVATE ${arg_PRIVATE_INCLUDES})
    target_include_directories(${NAME} PRIVATE ${arg_PRIVATE_INCLUDES})
  endif()

  if(NOT "${arg_PUBLIC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${NAME} PUBLIC ${arg_PUBLIC_COMPILE_OPTIONS})
  endif()

  if(NOT "${arg_PRIVATE_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${NAME} PRIVATE ${arg_PRIVATE_COMPILE_OPTIONS})
  endif()
endfunction()
