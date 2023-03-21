# SPDX-License-Identifier: Apache-2.0

add_library(rollback_initial OBJECT rollback_initial_data.c)

target_link_libraries(rollback_initial PUBLIC zephyr_interface)
target_link_libraries(rollback_initial PUBLIC cros_ec_interface)

# Add 'common' directory to include directories for rollback_private.h
target_include_directories(rollback_initial PUBLIC ${PLATFORM_EC}/common)

add_custom_target(initial_rollback_binary ALL
  COMMAND ${CMAKE_OBJCOPY} -Obinary --only-section .data.rollback_initial_data
	$<TARGET_OBJECTS:rollback_initial>
	${CMAKE_BINARY_DIR}/rollback_initial_data.bin
  COMMENT "Generating binary with initial rollback data"
  DEPENDS rollback_initial
  VERBATIM
)
