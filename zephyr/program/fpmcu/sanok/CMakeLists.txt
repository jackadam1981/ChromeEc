# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources("src/platform.c")

if(DEFINED CONFIG_CROS_EC_RO)

add_library(flash_header OBJECT src/flash_header.c)
set_source_files_properties(src/flash_header.c PROPERTIES
  COMPILE_OPTIONS $<TARGET_PROPERTY:compiler,prohibit_lto>
)

target_link_libraries(flash_header PUBLIC zephyr_interface)
target_link_libraries(flash_header PUBLIC cros_ec_interface)

add_custom_target(flash_header_binary ALL
  COMMAND ${CMAKE_OBJCOPY} -Obinary --only-section .data.flash_header
	$<TARGET_OBJECTS:flash_header>
	${CMAKE_BINARY_DIR}/flash_header.bin
  COMMENT "Generating binary with flash header"
  DEPENDS flash_header
  VERBATIM
)

endif()

zephyr_library_sources_ifdef(CONFIG_EC_HOST_CMD "src/fp_transport.c")
